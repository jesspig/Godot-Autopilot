import { CLI_VERSION } from "../version";
import { CliError, ExitCode } from "../errors";

export interface JsonRpcResponse {
  id: number;
  result?: unknown;
  error?: { code: number; message?: string };
}

export interface CallToolOptions {
  port: number;
  timeoutSec?: number;
  direct?: boolean;
}

export const META_DIRECT_TOOLS: ReadonlySet<string> = new Set([
  "ping",
  "search_tools",
  "list_categories",
  "get_tool_detail",
  "batch_execute",
  "code_execute",
]);

const HOST = "127.0.0.1";
const DEFAULT_TIMEOUT_SEC = 60;
const MAX_BODY_BYTES = 4 * 1024 * 1024;
const HANDSHAKE_METHOD = "notifications/initialized";
const HANDSHAKE_ERROR_CODE = -32600;
const PROTOCOL_VERSION = "2025-03-26";
const CLIENT_NAME = "gda-cli";

const CONNECT_FAILURE_SUGGESTION =
  "Is the editor open with the Godot Autopilot plugin enabled? Verify the port with --port or GODOT_AUTOPILOT_PORT.";
const TIMEOUT_SUGGESTION = "Split the work into smaller calls or raise --timeout SEC.";

function adviceForStatus(status: number, bodyBytes?: number): string {
  switch (status) {
    case 400:
      return "Check the tool name and arguments; discover exact names with the search command.";
    case 403:
      return "The server only accepts loopback hosts; this script always targets 127.0.0.1.";
    case 404:
      return "The endpoint only accepts POST at /mcp; verify the port discovery.";
    case 413:
      return `The request body exceeds 4 MiB: ${bodyBytes ?? 0} bytes sent. Shrink the payload (split into batches).`;
    case 503:
      return "The editor may be closed or busy. Open it with the plugin enabled and reduce parallel calls.";
    case 504:
      return "Processing exceeded the server time budget. Split the work into smaller calls.";
    default:
      return "Verify the port (--port, GODOT_AUTOPILOT_PORT, trace file, config.json) and retry.";
  }
}

let nextRequestId = 1;
let handshakeDone = false;

interface RawResponse {
  status: number;
  contentType: string;
  text: string;
}

function makeCallEnvelope(id: number, name: string, args: Record<string, unknown>, direct: boolean): string {
  const params = direct
    ? { name, arguments: args }
    : { name: "call_tool", arguments: { name, arguments: args } };
  return JSON.stringify({
    jsonrpc: "2.0",
    id,
    method: "tools/call",
    params,
  });
}

function makeInitializeEnvelope(): string {
  return JSON.stringify({
    jsonrpc: "2.0",
    id: nextRequestId++,
    method: "initialize",
    params: {
      protocolVersion: PROTOCOL_VERSION,
      capabilities: {},
      clientInfo: { name: CLIENT_NAME, version: CLI_VERSION },
    },
  });
}

async function performHandshake(url: string, timeoutMs: number): Promise<void> {
  const initRes = await postJson(url, makeInitializeEnvelope(), timeoutMs);
  if (initRes.status < 200 || initRes.status >= 300) {
    throw new CliError(ExitCode.Transport, `initialize failed with HTTP ${initRes.status}.`);
  }
  const ackRes = await postJson(url, JSON.stringify({ jsonrpc: "2.0", method: HANDSHAKE_METHOD }), timeoutMs);
  if (ackRes.status < 200 || ackRes.status >= 300) {
    throw new CliError(ExitCode.Transport, `handshake failed with HTTP ${ackRes.status}.`);
  }
  handshakeDone = true;
}

async function postJson(url: string, body: string, timeoutMs: number): Promise<RawResponse> {
  let res: Response;
  try {
    res = await fetch(url, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Accept: "application/json, text/event-stream",
      },
      body,
      signal: AbortSignal.timeout(timeoutMs),
    });
  } catch (err) {
    if (err && ((err as Error).name === "TimeoutError" || (err as Error).name === "AbortError")) {
      throw new CliError(ExitCode.Transport, "request timed out.", TIMEOUT_SUGGESTION);
    }
    const e = err as { cause?: unknown; message?: unknown } | null;
    const cause = e && e.cause ? `: ${String(e.cause)}` : "";
    const detail = e && e.message ? `${String(e.message)}${cause}` : String(err);
    throw new CliError(ExitCode.Transport, `cannot reach the server (${detail}).`, CONNECT_FAILURE_SUGGESTION);
  }
  let text: string;
  try {
    text = await res.text();
  } catch (err) {
    throw new CliError(ExitCode.Transport, `cannot read the response body (${(err as Error)?.message ?? String(err)}).`);
  }
  return { status: res.status, contentType: res.headers.get("content-type") || "", text };
}

function parseSseEnvelope(text: string): unknown | null | undefined {
  const chunks: string[] = [];
  for (const line of text.split(/\r?\n/)) {
    if (line.startsWith("data:")) {
      let value = line.slice("data:".length);
      if (value.startsWith(" ")) value = value.slice(1);
      chunks.push(value);
    }
  }
  if (chunks.length === 0) return null;
  try {
    return JSON.parse(chunks.join("\n"));
  } catch {
    return undefined;
  }
}

type Extracted =
  | { kind: "envelope"; envelope: unknown }
  | { kind: "no-data" }
  | { kind: "bad-sse" }
  | { kind: "bad-json" };

function extractEnvelope(res: RawResponse): Extracted {
  if (res.contentType.includes("text/event-stream")) {
    const envelope = parseSseEnvelope(res.text);
    if (envelope === null) return { kind: "no-data" };
    if (envelope === undefined) return { kind: "bad-sse" };
    return { kind: "envelope", envelope };
  }
  try {
    return { kind: "envelope", envelope: JSON.parse(res.text) };
  } catch {
    return { kind: "bad-json" };
  }
}

function needsHandshake(status: number, envelope: unknown): boolean {
  if (status !== 400) return false;
  if (!envelope || typeof envelope !== "object") return false;
  const err = (envelope as { error?: unknown }).error;
  if (!err || typeof err !== "object") return false;
  return (err as { code?: unknown }).code === HANDSHAKE_ERROR_CODE;
}

function serverMessage(text: string): string {
  let serverMsg = "";
  try {
    const parsed = JSON.parse(text) as { error?: { message?: unknown; code?: unknown }; message?: unknown } | null;
    const err = parsed && parsed.error;
    if (err && (err.message || err.code !== undefined)) {
      serverMsg = ` Server says: ${err.message ?? ""} (code ${err.code ?? "?"}).`;
    } else if (parsed && parsed.message) {
      serverMsg = ` Server says: ${parsed.message}.`;
    }
  } catch {
    if (text.trim() !== "") serverMsg = ` Server says: ${text.trim().slice(0, 200)}.`;
  }
  return serverMsg;
}

export async function callTool(
  name: string,
  args: Record<string, unknown>,
  opts: CallToolOptions,
): Promise<JsonRpcResponse> {
  const timeoutMs = (opts.timeoutSec ?? DEFAULT_TIMEOUT_SEC) * 1000;
  const url = `http://${HOST}:${opts.port}/mcp`;

  let id = nextRequestId++;
  const body = makeCallEnvelope(id, name, args, opts.direct === true);
  const bodyBytes = Buffer.byteLength(body, "utf8");
  if (bodyBytes > MAX_BODY_BYTES) {
    throw new CliError(ExitCode.Transport, `request body exceeds 4 MiB (${bodyBytes} bytes).`);
  }

  let res = await postJson(url, body, timeoutMs);
  let extracted = extractEnvelope(res);
  if (
    !handshakeDone &&
    ((extracted.kind === "envelope" && needsHandshake(res.status, extracted.envelope)) ||
      (res.status === 400 && extracted.kind !== "envelope"))
  ) {
    let envelope = extracted.kind === "envelope" ? extracted.envelope : null;
    if (envelope === null) {
      try {
        envelope = JSON.parse(res.text);
      } catch {
        envelope = null;
      }
    }
    if (envelope === null || needsHandshake(res.status, envelope)) {
      await performHandshake(url, timeoutMs);
      id = nextRequestId++;
      res = await postJson(url, makeCallEnvelope(id, name, args, opts.direct === true), timeoutMs);
      extracted = extractEnvelope(res);
    }
  }

  if (res.status < 200 || res.status >= 300) {
    throw new CliError(ExitCode.Transport, `HTTP ${res.status}.${serverMessage(res.text)}`, adviceForStatus(res.status, bodyBytes));
  }
  if (extracted.kind === "no-data" || extracted.kind === "bad-sse") {
    throw new CliError(ExitCode.Transport, "could not parse the SSE response (no usable data: frame).");
  }
  if (extracted.kind === "bad-json") {
    throw new CliError(ExitCode.Transport, "could not parse the plain-JSON response.");
  }
  return extracted.envelope as JsonRpcResponse;
}
