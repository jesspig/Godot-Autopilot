import { CLI_VERSION } from "../version";
import { CliError, ExitCode } from "../errors";
import { IpcSession, type IpcEndpoint } from "../ipc/client";

export interface JsonRpcResponse {
  id: number;
  result?: unknown;
  error?: { code: number; message?: string };
}

export interface RequestProgress {
  progress: number;
  total?: number;
  message?: string;
}

export interface CallToolOptions {
  port: number;
  timeoutSec?: number;
  direct?: boolean;
  socketPath?: string;
  ipcEndpoint?: IpcEndpoint;
  noFallback?: boolean;
  transport?: "auto" | "ipc" | "http";
  transportSink?: (transport: "ipc" | "http") => void;
  traceparent?: string;
  progressSink?: (progress: RequestProgress) => void;
  inputResponses?: Record<string, unknown>;
  keepTask?: boolean;
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
const MAX_REHANDSHAKES = 1;
const PROTOCOL_VERSION = "2025-11-25";
const MODERN_PROTOCOL_VERSION = "2026-07-28";
const KNOWN_PROTOCOL_VERSIONS: readonly string[] = [
  "2024-11-05",
  "2025-03-26",
  "2025-06-18",
  "2025-11-25",
  "2026-07-28",
];
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

interface HandshakeState {
  done: boolean;
  version: string | null;
  rehandshakes: number;
}

export interface HandshakeInfo {
  protocol_version: string;
  capabilities: unknown;
  server_info: unknown;
}

const handshakeStates = new Map<string, HandshakeState>();
const handshakeInfos = new Map<string, HandshakeInfo>();

export function getHandshakeInfo(port: number): HandshakeInfo | null {
  return handshakeInfos.get(`http://${HOST}:${port}/mcp`) ?? null;
}

export async function ensureConnection(port: number, timeoutSec = DEFAULT_TIMEOUT_SEC): Promise<void> {
  const url = `http://${HOST}:${port}/mcp`;
  const state = handshakeStateFor(url);
  if (!state.done) {
    await performHandshake(url, timeoutSec * 1000);
  }
}

function handshakeStateFor(url: string): HandshakeState {
  let state = handshakeStates.get(url);
  if (state === undefined) {
    state = { done: false, version: null, rehandshakes: 0 };
    handshakeStates.set(url, state);
  }
  return state;
}

interface RawResponse {
  status: number;
  contentType: string;
  text: string;
}

function makeInitializeEnvelope(): string {
  return JSON.stringify({
    jsonrpc: "2.0",
    id: nextRequestId++,
    method: "initialize",
    params: {
      protocolVersion: PROTOCOL_VERSION,
      capabilities: { elicitation: {} },
      clientInfo: { name: CLIENT_NAME, version: CLI_VERSION },
    },
  });
}

function isModernProtocolVersion(version: string): boolean {
  return version >= MODERN_PROTOCOL_VERSION;
}

function learnNegotiatedVersion(res: RawResponse, state: HandshakeState, url: string): void {
  const extracted = extractEnvelope(res);
  if (extracted.kind !== "envelope") return;
  const envelope = extracted.envelope as {
    result?: { protocolVersion?: unknown; capabilities?: unknown; serverInfo?: unknown };
  };
  const version = envelope.result?.protocolVersion;
  if (typeof version === "string" && KNOWN_PROTOCOL_VERSIONS.includes(version)) {
    state.version = version;
  }
  handshakeInfos.set(url, {
    protocol_version: typeof version === "string" ? version : (state.version ?? ""),
    capabilities: envelope.result?.capabilities ?? {},
    server_info: envelope.result?.serverInfo ?? {},
  });
}

function requestHeaders(url: string): Record<string, string> {
  const headers: Record<string, string> = {
    "Content-Type": "application/json",
    Accept: "application/json, text/event-stream",
  };
  const version = handshakeStateFor(url).version;
  if (version !== null && !isModernProtocolVersion(version)) {
    headers["MCP-Protocol-Version"] = version;
  }
  return headers;
}

async function performHandshake(url: string, timeoutMs: number): Promise<void> {
  const state = handshakeStateFor(url);
  const initRes = await postJson(url, makeInitializeEnvelope(), timeoutMs);
  if (initRes.status < 200 || initRes.status >= 300) {
    throw new CliError(ExitCode.Transport, `initialize failed with HTTP ${initRes.status}.`);
  }
  learnNegotiatedVersion(initRes, state, url);
  const ackRes = await postJson(url, JSON.stringify({ jsonrpc: "2.0", method: HANDSHAKE_METHOD }), timeoutMs);
  if (ackRes.status < 200 || ackRes.status >= 300) {
    throw new CliError(ExitCode.Transport, `handshake failed with HTTP ${ackRes.status}.`);
  }
  state.done = true;
}

async function postJson(url: string, body: string, timeoutMs: number): Promise<RawResponse> {
  let res: Response;
  try {
    res = await fetch(url, {
      method: "POST",
      headers: requestHeaders(url),
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

function parseSseEnvelopes(text: string): unknown[] | null | undefined {
  const frames: unknown[] = [];
  for (const block of text.split(/\r?\n\r?\n/)) {
    const chunks: string[] = [];
    for (const line of block.split(/\r?\n/)) {
      if (line.startsWith("data:")) {
        let value = line.slice("data:".length);
        if (value.startsWith(" ")) value = value.slice(1);
        chunks.push(value);
      }
    }
    if (chunks.length === 0) continue;
    try {
      frames.push(JSON.parse(chunks.join("\n")));
    } catch {
      return undefined;
    }
  }
  if (frames.length === 0) return null;
  return frames;
}

type Extracted =
  | { kind: "envelope"; envelope: unknown }
  | { kind: "no-data" }
  | { kind: "bad-sse" }
  | { kind: "bad-json" };

function extractEnvelope(
  res: RawResponse,
  progressSink?: (progress: RequestProgress) => void,
): Extracted {
  if (res.contentType.includes("text/event-stream")) {
    const frames = parseSseEnvelopes(res.text);
    if (frames === null) return { kind: "no-data" };
    if (frames === undefined) return { kind: "bad-sse" };
    let finalEnvelope: unknown = undefined;
    for (const frame of frames) {
      if (frame === null || typeof frame !== "object" || Array.isArray(frame)) continue;
      const record = frame as { method?: unknown; params?: unknown; result?: unknown; error?: unknown };
      if (record.method === "notifications/progress" && progressSink !== undefined) {
        const params = record.params;
        if (params !== null && typeof params === "object" && !Array.isArray(params)) {
          const progress = params as { progress?: unknown; total?: unknown; message?: unknown };
          if (typeof progress.progress === "number") {
            progressSink({
              progress: progress.progress,
              total: typeof progress.total === "number" ? progress.total : undefined,
              message: typeof progress.message === "string" ? progress.message : undefined,
            });
          }
        }
        continue;
      }
      if (record.result !== undefined || record.error !== undefined) {
        finalEnvelope = frame;
      }
    }
    if (finalEnvelope === undefined) return { kind: "no-data" };
    return { kind: "envelope", envelope: finalEnvelope };
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

function handshakeRequired(res: RawResponse, extracted: Extracted): boolean {
  if (res.status !== 400) return false;
  if (extracted.kind === "envelope") return needsHandshake(res.status, extracted.envelope);
  return true;
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

export async function listTools(
  port: number,
  timeoutSec: number = DEFAULT_TIMEOUT_SEC,
): Promise<unknown[]> {
  const url = `http://${HOST}:${port}/mcp`;
  const timeoutMs = timeoutSec * 1000;
  const state = handshakeStateFor(url);
  if (!state.done) {
    await performHandshake(url, timeoutMs);
  }
  const id = nextRequestId++;
  const res = await postJson(
    url,
    JSON.stringify({ jsonrpc: "2.0", id, method: "tools/list", params: {} }),
    timeoutMs,
  );
  if (res.status < 200 || res.status >= 300) {
    throw new CliError(ExitCode.Transport, `tools/list failed with HTTP ${res.status}.`);
  }
  const extracted = extractEnvelope(res);
  if (extracted.kind !== "envelope") {
    throw new CliError(ExitCode.Transport, "could not parse the tools/list response.");
  }
  const result = (extracted.envelope as { result?: { tools?: unknown } }).result;
  return Array.isArray(result?.tools) ? result.tools : [];
}

export async function callTool(
  name: string,
  args: Record<string, unknown>,
  opts: CallToolOptions,
): Promise<JsonRpcResponse> {
  const params: Record<string, unknown> = opts.direct === true
    ? { name, arguments: args }
    : { name: "call_tool", arguments: { name, arguments: args } };
  const response = await request("tools/call", params, opts);
  if (opts.keepTask === true) return response;
  const result = response.result;
  if (result === null || typeof result !== "object" || Array.isArray(result)) return response;
  const record = result as { resultType?: unknown; taskId?: unknown; status?: unknown };
  const taskId = typeof record.taskId === "string" ? record.taskId : "";
  if (record.resultType !== "task" && taskId === "") return response;
  return await waitForTask(response, taskId, opts);
}

async function waitForTask(
  initial: JsonRpcResponse,
  taskId: string,
  opts: CallToolOptions,
): Promise<JsonRpcResponse> {
  if (taskId === "") {
    return { id: initial.id, error: { code: -32000, message: "task result is missing taskId" } };
  }
  const timeoutMs = (opts.timeoutSec ?? DEFAULT_TIMEOUT_SEC) * 1000;
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const polled = await request("tasks/get", { taskId }, { ...opts, keepTask: true });
    if (polled.error !== undefined) return polled;
    const result = polled.result;
    if (result === null || typeof result !== "object" || Array.isArray(result)) {
      return { id: initial.id, error: { code: -32000, message: "invalid tasks/get result" } };
    }
    const task = result as { status?: unknown; result?: unknown; error?: unknown; statusMessage?: unknown };
    const status = typeof task.status === "string" ? task.status : "";
    if (status === "completed") {
      return { id: initial.id, result: task.result ?? { content: [] } };
    }
    if (status === "failed" || status === "cancelled") {
      const message = typeof task.statusMessage === "string" ? task.statusMessage : `task ${status}`;
      return { id: initial.id, error: { code: -32000, message } };
    }
    if (Date.now() >= deadline) {
      return { id: initial.id, error: { code: -32000, message: `task ${taskId} timed out` } };
    }
    await new Promise((resolve) => setTimeout(resolve, 250));
  }
}

export async function request(
  method: string,
  params: Record<string, unknown>,
  opts: CallToolOptions,
): Promise<JsonRpcResponse> {
  const timeoutMs = (opts.timeoutSec ?? DEFAULT_TIMEOUT_SEC) * 1000;
  const transportMode = opts.transport ?? "auto";
  const ipcEndpoint =
    opts.ipcEndpoint ?? (opts.socketPath !== undefined ? { path: opts.socketPath } : undefined);
  const requestParams: Record<string, unknown> = { ...params };
  if (opts.inputResponses !== undefined) {
    requestParams.inputResponses = opts.inputResponses;
  }
  if (opts.traceparent !== undefined && opts.traceparent !== "") {
    const meta = requestParams._meta;
    requestParams._meta = {
      ...(meta !== null && typeof meta === "object" && !Array.isArray(meta) ? meta : {}),
      traceparent: opts.traceparent,
    };
  }
  if (transportMode !== "http" && ipcEndpoint !== undefined) {
    try {
      const response = await requestOverIpc(method, requestParams, ipcEndpoint, timeoutMs);
      const tokenFailure = response.error?.code === -32001 || response.error?.code === -32002;
      if (!tokenFailure) {
        opts.transportSink?.("ipc");
        return response;
      }
      if (transportMode === "ipc" || opts.noFallback === true) {
        return response;
      }
    } catch (error) {
      if (transportMode === "ipc" || opts.noFallback === true) {
        throw error;
      }
    }
  }
  if (transportMode === "ipc") {
    throw new CliError(
      ExitCode.Transport,
      "IPC transport was requested but no usable IPC endpoint is available.",
      "Run 'gda status --json' to inspect the instance endpoint, or use --transport http.",
    );
  }
  const url = `http://${HOST}:${opts.port}/mcp`;

  const makeBody = (id: number): string =>
    JSON.stringify({ jsonrpc: "2.0", id, method, params: requestParams });
  let id = nextRequestId++;
  const body = makeBody(id);
  const bodyBytes = Buffer.byteLength(body, "utf8");
  if (bodyBytes > MAX_BODY_BYTES) {
    throw new CliError(ExitCode.Transport, `request body exceeds 4 MiB (${bodyBytes} bytes).`);
  }

  let res = await postJson(url, body, timeoutMs);
  let extracted = extractEnvelope(res, opts.progressSink);
  const state = handshakeStateFor(url);
  if (handshakeRequired(res, extracted)) {
    const expired = state.done;
    if (!expired || state.rehandshakes < MAX_REHANDSHAKES) {
      if (expired) {
        state.done = false;
        state.version = null;
        state.rehandshakes += 1;
      }
      await performHandshake(url, timeoutMs);
      id = nextRequestId++;
      res = await postJson(url, makeBody(id), timeoutMs);
      extracted = extractEnvelope(res, opts.progressSink);
    }
  }
  if (!state.done && extracted.kind === "envelope" &&
      (extracted.envelope as { error?: unknown } | null)?.error !== undefined) {
    await performHandshake(url, timeoutMs);
    id = nextRequestId++;
    res = await postJson(url, makeBody(id), timeoutMs);
    extracted = extractEnvelope(res, opts.progressSink);
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
  opts.transportSink?.("http");
  return extracted.envelope as JsonRpcResponse;
}

async function requestOverIpc(
  method: string,
  params: Record<string, unknown>,
  endpoint: IpcEndpoint,
  timeoutMs: number,
): Promise<JsonRpcResponse> {
  const session = new IpcSession(endpoint, timeoutMs);
  try {
    await session.open();
    const init = JSON.parse(makeInitializeEnvelope()) as Record<string, unknown>;
    const initParams = (init.params ?? {}) as Record<string, unknown>;
    const meta: Record<string, string> = {};
    if (endpoint.token !== undefined && endpoint.token !== "") {
      meta.gda_token = endpoint.token;
    }
    if (Object.keys(meta).length > 0) {
      initParams._meta = meta;
      init.params = initParams;
    }
    const initResponse = await session.request(init);
    if (initResponse.error !== undefined) {
      return initResponse;
    }
    session.notify({ jsonrpc: "2.0", method: HANDSHAKE_METHOD });
    const id = nextRequestId++;
    return await session.request({ jsonrpc: "2.0", id, method, params });
  } finally {
    session.close();
  }
}
