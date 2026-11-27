import { CLI_VERSION } from "../version";
import { CliError, ExitCode, markIpcAuthRejected } from "../errors";
import { IpcSession, type IpcEndpoint } from "../ipc/client";
import { probeTcp } from "../probe";

export interface JsonRpcResponse {
  id: number;
  result?: unknown;
  error?: { code: number; message?: string; data?: unknown };
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
  traceparent?: string;
  progressSink?: (progress: RequestProgress) => void;
  inputResponses?: Record<string, unknown>;
  keepTask?: boolean;
  ipcSession?: IpcSession;
}

export const META_DIRECT_TOOLS: ReadonlySet<string> = new Set([
  "ping",
  "search_tools",
  "list_categories",
  "get_tool_detail",
  "batch_execute",
  "code_execute",
]);

const DEFAULT_TIMEOUT_SEC = 60;
const MAX_FRAME_BYTES = 4 * 1024 * 1024;
const HANDSHAKE_METHOD = "notifications/initialized";
const PROTOCOL_VERSION = "1";
const CLIENT_NAME = "gda-cli";
const IPC_TOKEN_ERROR_CODES: ReadonlySet<number> = new Set([-32001, -32002]);

export interface HandshakeInfo {
  protocol_version: string;
  capabilities: unknown;
  server_info: unknown;
}

const handshakeInfos = new Map<string, HandshakeInfo>();

function endpointKey(endpoint: IpcEndpoint): string {
  if (endpoint.kind === "pipe") return `pipe:${endpoint.name ?? ""}`;
  if (endpoint.kind === "tcp") return `tcp:${endpoint.host ?? "127.0.0.1"}:${endpoint.port ?? 0}`;
  return `socket:${endpoint.path ?? ""}`;
}

export function getHandshakeInfo(port: number): HandshakeInfo | null {
  return handshakeInfos.get(`tcp:127.0.0.1:${port}`) ?? null;
}

export async function ensureConnection(port: number, timeoutSec = DEFAULT_TIMEOUT_SEC): Promise<void> {
  if (!(await probeTcp("127.0.0.1", port, timeoutSec * 1000))) {
    throw new CliError(
      ExitCode.Transport,
      `cannot reach the native control plane on port ${port}.`,
      "Open the editor with the plugin enabled, then verify the target with 'gda status --json'.",
    );
  }
}

let nextRequestId = 1;

function makeInitializeEnvelope(endpoint: IpcEndpoint): Record<string, unknown> {
  const params: Record<string, unknown> = {
    protocolVersion: PROTOCOL_VERSION,
    capabilities: { elicitation: {} },
    clientInfo: { name: CLIENT_NAME, version: CLI_VERSION },
  };
  if (endpoint.token !== undefined && endpoint.token !== "") {
    params._meta = { gda_token: endpoint.token };
  }
  return { jsonrpc: "2.0", id: nextRequestId++, method: "initialize", params };
}

function learnHandshakeInfo(endpoint: IpcEndpoint, envelope: JsonRpcResponse): void {
  const result = envelope.result;
  if (result === null || typeof result !== "object" || Array.isArray(result)) return;
  const record = result as {
    protocolVersion?: unknown;
    capabilities?: unknown;
    serverInfo?: unknown;
  };
  handshakeInfos.set(endpointKey(endpoint), {
    protocol_version: typeof record.protocolVersion === "string" ? record.protocolVersion : "",
    capabilities: record.capabilities ?? {},
    server_info: record.serverInfo ?? {},
  });
}

function progressFromFrame(frame: Record<string, unknown>): RequestProgress | null {
  if (frame.method !== "notifications/progress") return null;
  const params = frame.params;
  if (params === null || typeof params !== "object" || Array.isArray(params)) return null;
  const record = params as { progress?: unknown; total?: unknown; message?: unknown };
  if (typeof record.progress !== "number") return null;
  return {
    progress: record.progress,
    total: typeof record.total === "number" ? record.total : undefined,
    message: typeof record.message === "string" ? record.message : undefined,
  };
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
    const task = result as {
      status?: unknown;
      result?: unknown;
      error?: unknown;
      statusMessage?: unknown;
    };
    const status = typeof task.status === "string" ? task.status : "";
    if (status === "completed") {
      return { id: initial.id, result: task.result ?? { content: [] } };
    }
    if (status === "failed" || status === "cancelled") {
      const errorRecord =
        task.error !== null && typeof task.error === "object" && !Array.isArray(task.error)
          ? (task.error as { code?: unknown; message?: unknown; data?: unknown })
          : null;
      const code =
        errorRecord !== null && typeof errorRecord.code === "number" ? errorRecord.code : -32000;
      const message =
        errorRecord !== null && typeof errorRecord.message === "string"
          ? errorRecord.message
          : typeof task.statusMessage === "string"
            ? task.statusMessage
            : `task ${status}`;
      const data = errorRecord !== null ? errorRecord.data : undefined;
      return {
        id: initial.id,
        error: data !== undefined ? { code, message, data } : { code, message },
      };
    }
    if (Date.now() >= deadline) {
      return { id: initial.id, error: { code: -32000, message: `task ${taskId} timed out` } };
    }
    await new Promise((resolve) => setTimeout(resolve, 250));
  }
}

function endpointFor(opts: CallToolOptions): IpcEndpoint {
  if (opts.ipcEndpoint !== undefined) return opts.ipcEndpoint;
  if (opts.socketPath !== undefined) return { path: opts.socketPath };
  return { kind: "tcp", host: "127.0.0.1", port: opts.port };
}

export async function request(
  method: string,
  params: Record<string, unknown>,
  opts: CallToolOptions,
): Promise<JsonRpcResponse> {
  const endpoint = endpointFor(opts);
  if (endpoint.kind === "tcp" && (!Number.isInteger(endpoint.port) || (endpoint.port ?? 0) < 1)) {
    throw new CliError(
      ExitCode.Transport,
      "a native TCP endpoint requires a valid port.",
      "Pass --port, --id, --project or --socket to select a running instance.",
    );
  }
  const timeoutMs = (opts.timeoutSec ?? DEFAULT_TIMEOUT_SEC) * 1000;
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
  const body = JSON.stringify({ jsonrpc: "2.0", method, params: requestParams });
  if (Buffer.byteLength(body, "utf8") > MAX_FRAME_BYTES) {
    throw new CliError(ExitCode.Transport, `request body exceeds 4 MiB (${Buffer.byteLength(body, "utf8")} bytes).`);
  }

  if (opts.ipcSession !== undefined) {
    const id = nextRequestId++;
    return await opts.ipcSession.request({ jsonrpc: "2.0", id, method, params: requestParams });
  }

  const session = new IpcSession(endpoint, timeoutMs, (frame) => {
    if (opts.progressSink === undefined) return;
    const progress = progressFromFrame(frame);
    if (progress !== null) opts.progressSink(progress);
  });
  try {
    await session.open();
    const initResponse = await session.request(makeInitializeEnvelope(endpoint));
    if (initResponse.error !== undefined) {
      const code = initResponse.error.code;
      if (IPC_TOKEN_ERROR_CODES.has(code)) {
        throw markIpcAuthRejected(new CliError(
          ExitCode.Transport,
          `IPC authentication failed: ${initResponse.error.message ?? "token rejected"} (code ${code}).`,
          "Check the IPC token in the instance registry; no unauthenticated channel is used.",
        ));
      }
      return initResponse;
    }
    learnHandshakeInfo(endpoint, initResponse);
    session.notify({ jsonrpc: "2.0", method: HANDSHAKE_METHOD });
    const id = nextRequestId++;
    return await session.request({ jsonrpc: "2.0", id, method, params: requestParams });
  } finally {
    session.close();
  }
}

export async function createBatchIpcSession(
  endpoint: IpcEndpoint,
  timeoutMs: number,
): Promise<IpcSession> {
  const session = new IpcSession(endpoint, timeoutMs);
  try {
    await session.open();
    const initResponse = await session.request(makeInitializeEnvelope(endpoint));
    if (initResponse.error !== undefined) {
      const detail =
        initResponse.error.message !== undefined && initResponse.error.message !== ""
          ? `: ${initResponse.error.message}`
          : "";
      throw new CliError(
        ExitCode.Transport,
        `IPC initialize failed (code ${initResponse.error.code})${detail}.`,
      );
    }
    learnHandshakeInfo(endpoint, initResponse);
    session.notify({ jsonrpc: "2.0", method: HANDSHAKE_METHOD });
    return session;
  } catch (error) {
    session.close();
    throw error;
  }
}

export function closeBatchIpcSession(session: IpcSession): void {
  session.close();
}
