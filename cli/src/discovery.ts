import fs from "node:fs";
import net from "node:net";
import path from "node:path";

import { CliError, ExitCode, markIpcRequestSent } from "./errors";
import { fnv1a32 } from "./hash";
import type { IpcEndpoint } from "./ipc/client";
import { gdaRoot } from "./platform";
import { makeTraceparent } from "./trace";
import { CLI_VERSION } from "./version";

export const MCP_PROTOCOL_TOOLS: ReadonlySet<string> = new Set([
  "ping",
  "search_tools",
  "list_categories",
  "get_tool_detail",
  "call_tool",
]);

export function isProtocolTool(name: string): boolean {
  return MCP_PROTOCOL_TOOLS.has(name);
}

export function filterProtocolToolNames(names: string[]): string[] {
  return names.filter((name) => !isProtocolTool(name));
}

export function filterProtocolTools<T extends { name: string }>(entries: T[]): T[] {
  return entries.filter((entry) => !isProtocolTool(entry.name));
}

export interface GcpParam {
  key: string;
  value: string;
}

export interface GcpEnvelopeLike {
  v: number;
  kind: string;
  id: string;
  instance_id: string;
  method: string;
  params: GcpParam[];
  trace?: { traceparent?: string };
  deadline_ms?: number;
  status?: string;
  error?: number;
  error_message?: string;
  task_id?: string;
  request_id?: string;
  seq?: number;
  event_type?: string;
  confirmation?: {
    ttl_ms?: number;
    issued_at_ms?: number;
    param_digest?: string;
    nonce?: string;
  };
}

export interface NativeCatalogEntry {
  name: string;
  description: string;
  category: string;
  tags: string[];
  aliases: string[];
  mutating: boolean;
  high_risk: boolean;
}

export interface CatalogDocument {
  schema_version: number;
  hash: string;
  tools: NativeCatalogEntry[];
}

const GCP_VERSION = 1;
const DEFAULT_NATIVE_TIMEOUT_MS = 30_000;
const MAX_FRAME_BYTES = 4 * 1024 * 1024;
const CATALOG_SCHEMA_VERSION = 1;

let requestCounter = 0;

export function buildGcpRequest(
  method: string,
  params: Record<string, string> = {},
  instanceId = "",
  timeoutMs = DEFAULT_NATIVE_TIMEOUT_MS,
  id?: string,
): GcpEnvelopeLike {
  requestCounter += 1;
  return {
    v: GCP_VERSION,
    kind: "request",
    id: id ?? `gda-${process.pid}-${requestCounter}`,
    instance_id: instanceId,
    method,
    params: Object.keys(params)
      .sort()
      .map((key) => ({ key, value: params[key] })),
    trace: { traceparent: makeTraceparent() },
    deadline_ms: timeoutMs,
  };
}

export function gcpResult(response: GcpEnvelopeLike): unknown {
  const param = (response.params ?? []).find((entry) => entry.key === "result");
  if (param === undefined) return undefined;
  try {
    return JSON.parse(param.value) as unknown;
  } catch {
    throw new CliError(ExitCode.Transport, "the native response carried an invalid result payload.");
  }
}

export function gcpError(response: GcpEnvelopeLike): CliError | undefined {
  if (response.status === "ok" || response.status === "pending") return undefined;
  const numeric = typeof response.error === "number" ? response.error : 0;
  const detail = response.error_message ?? "";
  if (numeric === 0 && detail === "") return undefined;
  return new CliError(ExitCode.Transport, `native request ${response.method || "call"} failed (code ${numeric})${detail === "" ? "" : `: ${detail}`}.`);
}

function endpointLabel(endpoint: IpcEndpoint): string {
  if (endpoint.kind === "tcp") return `tcp ${endpoint.host ?? "127.0.0.1"}:${endpoint.port ?? 0}`;
  if (endpoint.kind === "pipe") return `pipe ${endpoint.name ?? ""}`;
  return `socket ${endpoint.path ?? ""}`;
}

function connectOptions(endpoint: IpcEndpoint): net.NetConnectOpts {
  if (endpoint.kind === "tcp") {
    return { host: endpoint.host ?? "127.0.0.1", port: endpoint.port ?? 0 };
  }
  if (endpoint.kind === "pipe") {
    return { path: `\\\\.\\pipe\\${endpoint.name ?? ""}` };
  }
  return { path: endpoint.path ?? "" };
}

function encodeFrame(text: string): Buffer {
  const payload = Buffer.from(text, "utf8");
  const header = Buffer.alloc(4);
  header.writeUInt32BE(payload.length, 0);
  return Buffer.concat([header, payload]);
}

interface DecodedFrames {
  frames: string[];
  remainder: Buffer;
  overflow: boolean;
}

function decodeFrames(buffer: Buffer): DecodedFrames {
  const frames: string[] = [];
  let offset = 0;
  while (buffer.length - offset >= 4) {
    const size = buffer.readUInt32BE(offset);
    if (size > MAX_FRAME_BYTES) {
      return { frames, remainder: Buffer.alloc(0), overflow: true };
    }
    if (buffer.length - offset - 4 < size) break;
    frames.push(buffer.subarray(offset + 4, offset + 4 + size).toString("utf8"));
    offset += 4 + size;
  }
  return { frames, remainder: buffer.subarray(offset), overflow: false };
}

interface PendingNative {
  resolve: (response: GcpEnvelopeLike) => void;
  reject: (error: Error) => void;
  timer: ReturnType<typeof setTimeout>;
}

class GcpIpcSession {
  private socket: net.Socket | null = null;
  private buffer = Buffer.alloc(0);
  private readonly pending = new Map<string, PendingNative>();
  private handshakeId = 1;
  private handshakeResolve: ((response: Record<string, unknown>) => void) | null = null;
  private handshakeReject: ((error: Error) => void) | null = null;
  private handshakeTimer: ReturnType<typeof setTimeout> | null = null;

  constructor(
    private readonly endpoint: IpcEndpoint,
    private readonly timeoutMs: number,
  ) {}

  open(): Promise<void> {
    return new Promise((resolve, reject) => {
      const socket = net.createConnection(connectOptions(this.endpoint));
      this.socket = socket;
      let opened = false;
      socket.once("connect", () => {
        opened = true;
        this.authenticate().then(() => resolve(), (error: unknown) => {
          reject(error instanceof Error ? error : new Error(String(error)));
        });
      });
      socket.once("error", (error: NodeJS.ErrnoException) => {
        const mapped = new CliError(
          ExitCode.Transport,
          `native IPC connection failed (${endpointLabel(this.endpoint)}): ${error.message ?? "unknown error"}`,
        );
        if (!opened) reject(mapped);
        this.failAll(mapped);
      });
      socket.once("close", () => {
        const error = new CliError(ExitCode.Transport, `native IPC connection closed (${endpointLabel(this.endpoint)}).`);
        if (!opened) reject(error);
        this.failAll(error);
      });
      socket.on("data", (chunk: Buffer) => this.onData(chunk));
    });
  }

  private authenticate(): Promise<void> {
    const token = this.endpoint.token;
    if (token === undefined || token === "" || this.socket === null) {
      return Promise.resolve();
    }
    const id = this.handshakeId;
    const init = {
      jsonrpc: "2.0",
      id,
      method: "initialize",
      params: {
        protocolVersion: "2025-11-25",
        capabilities: { elicitation: {} },
        clientInfo: { name: "gda-cli", version: CLI_VERSION },
        _meta: { gda_token: token },
      },
    };
    return new Promise<void>((resolve, reject) => {
      this.handshakeResolve = (response) => {
        const error = response.error as { code?: unknown; message?: unknown } | undefined;
        if (error !== undefined) {
          const detail = typeof error.message === "string" ? `: ${error.message}` : "";
          reject(new CliError(
            ExitCode.Transport,
            `native IPC initialize failed (code ${String(error.code ?? "unknown")})${detail}.`,
          ));
          return;
        }
        resolve();
      };
      this.handshakeReject = reject;
      this.handshakeTimer = setTimeout(() => {
        this.handshakeResolve = null;
        this.handshakeReject = null;
        this.handshakeTimer = null;
        reject(new CliError(
          ExitCode.Transport,
          `native IPC initialize timed out after ${this.timeoutMs} ms (${endpointLabel(this.endpoint)}).`,
        ));
      }, this.timeoutMs);
      this.socket?.write(encodeFrame(JSON.stringify(init)));
    }).then(() => {
      this.socket?.write(encodeFrame(JSON.stringify({ jsonrpc: "2.0", method: "notifications/initialized" })));
    });
  }

  private onData(chunk: Buffer): void {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    const decoded = decodeFrames(this.buffer);
    this.buffer = Buffer.from(decoded.remainder);
    if (decoded.overflow) {
      this.failAll(new CliError(ExitCode.Transport, "the native IPC response exceeds the 4 MiB frame limit."));
      return;
    }
    for (const text of decoded.frames) {
      let parsed: unknown;
      try {
        parsed = JSON.parse(text);
      } catch {
        this.failAll(new CliError(ExitCode.Transport, "the native IPC server returned a frame that is not valid JSON."));
        return;
      }
      if (parsed === null || typeof parsed !== "object" || Array.isArray(parsed)) continue;
      const record = parsed as { id?: unknown };
      if (typeof record.id === "number" && this.handshakeResolve !== null) {
        if (this.handshakeTimer !== null) clearTimeout(this.handshakeTimer);
        this.handshakeTimer = null;
        const resolveHandshake = this.handshakeResolve;
        this.handshakeResolve = null;
        this.handshakeReject = null;
        resolveHandshake(parsed as Record<string, unknown>);
        continue;
      }
      if (typeof record.id !== "string") continue;
      const responseId = record.id.startsWith("r-") ? record.id.slice(2) : record.id;
      const entry = this.pending.get(responseId);
      if (entry === undefined) continue;
      clearTimeout(entry.timer);
      this.pending.delete(responseId);
      entry.resolve(parsed as GcpEnvelopeLike);
    }
  }

  request(request: GcpEnvelopeLike): Promise<GcpEnvelopeLike> {
    if (this.socket === null) {
      return Promise.reject(new CliError(ExitCode.Transport, "the native IPC session is not open."));
    }
    const socket = this.socket;
    return new Promise<GcpEnvelopeLike>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(request.id);
        reject(markIpcRequestSent(new CliError(
          ExitCode.Transport,
          `the native request timed out after ${this.timeoutMs} ms (${endpointLabel(this.endpoint)}); it was not retried.`,
        )));
      }, this.timeoutMs);
      this.pending.set(request.id, { resolve, reject, timer });
      socket.write(encodeFrame(JSON.stringify(request)));
    });
  }

  close(): void {
    if (this.socket !== null) {
      this.socket.destroy();
      this.socket = null;
    }
    this.failAll(new CliError(ExitCode.Transport, "the native IPC session closed."));
  }

  private failAll(error: CliError): void {
    for (const [id, entry] of this.pending) {
      clearTimeout(entry.timer);
      entry.reject(markIpcRequestSent(new CliError(
        error.code,
        `${error.message} The request ${id} had already been sent; the result is unknown and it was not retried.`,
        error.suggestion,
      )));
      this.pending.delete(id);
    }
  }
}

export async function nativeRequest(
  endpoint: IpcEndpoint,
  method: string,
  params: Record<string, string> = {},
  instanceId = "",
  timeoutMs = DEFAULT_NATIVE_TIMEOUT_MS,
  id?: string,
): Promise<GcpEnvelopeLike> {
  const session = new GcpIpcSession(endpoint, timeoutMs);
  try {
    await session.open();
    return await session.request(buildGcpRequest(method, params, instanceId, timeoutMs, id));
  } finally {
    session.close();
  }
}

export async function nativeHello(
  endpoint: IpcEndpoint,
  instanceId = "",
  timeoutMs = DEFAULT_NATIVE_TIMEOUT_MS,
): Promise<{ instance_id: string; capabilities: unknown }> {
  const response = await nativeRequest(endpoint, "hello", {}, instanceId, timeoutMs);
  const failure = gcpError(response);
  if (failure !== undefined) throw failure;
  const result = gcpResult(response);
  if (result === null || typeof result !== "object" || Array.isArray(result)) {
    throw new CliError(ExitCode.Transport, "the native hello response is malformed.");
  }
  const record = result as { instance_id?: unknown; capabilities?: unknown };
  return {
    instance_id: typeof record.instance_id === "string" ? record.instance_id : instanceId,
    capabilities: record.capabilities ?? {},
  };
}

export async function nativeCapabilities(
  endpoint: IpcEndpoint,
  instanceId = "",
  timeoutMs = DEFAULT_NATIVE_TIMEOUT_MS,
): Promise<Record<string, unknown>> {
  const response = await nativeRequest(endpoint, "capabilities.get", {}, instanceId, timeoutMs);
  const failure = gcpError(response);
  if (failure !== undefined) throw failure;
  const result = gcpResult(response);
  if (result === null || typeof result !== "object" || Array.isArray(result)) {
    throw new CliError(ExitCode.Transport, "the native capabilities response is malformed.");
  }
  return result as Record<string, unknown>;
}

export async function nativeCatalogList(
  endpoint: IpcEndpoint,
  instanceId: string,
  params: Record<string, string> = {},
  timeoutMs = DEFAULT_NATIVE_TIMEOUT_MS,
): Promise<NativeCatalogEntry[]> {
  const response = await nativeRequest(endpoint, "catalog.list", { limit: "4096", ...params }, instanceId, timeoutMs);
  const failure = gcpError(response);
  if (failure !== undefined) throw failure;
  const result = gcpResult(response);
  if (!Array.isArray(result)) {
    throw new CliError(ExitCode.Transport, "the native catalog.list response is malformed.");
  }
  return result.filter(isCatalogEntry);
}

export async function nativeCatalogSearch(
  endpoint: IpcEndpoint,
  instanceId: string,
  params: Record<string, string>,
  timeoutMs = DEFAULT_NATIVE_TIMEOUT_MS,
): Promise<Array<{ name: string; description: string; category: string; score: number }>> {
  const response = await nativeRequest(endpoint, "catalog.search", { limit: "64", ...params }, instanceId, timeoutMs);
  const failure = gcpError(response);
  if (failure !== undefined) throw failure;
  const result = gcpResult(response);
  if (!Array.isArray(result)) {
    throw new CliError(ExitCode.Transport, "the native catalog.search response is malformed.");
  }
  return result.filter((item): item is { name: string; description: string; category: string; score: number } =>
    item !== null && typeof item === "object" && typeof (item as { name?: unknown }).name === "string");
}

export async function nativeCatalogDescribe(
  endpoint: IpcEndpoint,
  instanceId: string,
  name: string,
  timeoutMs = DEFAULT_NATIVE_TIMEOUT_MS,
): Promise<Record<string, unknown>> {
  const response = await nativeRequest(endpoint, "catalog.describe", { name }, instanceId, timeoutMs);
  const failure = gcpError(response);
  if (failure !== undefined) throw failure;
  const result = gcpResult(response);
  if (result === null || typeof result !== "object" || Array.isArray(result)) {
    throw new CliError(ExitCode.Transport, "the native catalog.describe response is malformed.");
  }
  return result as Record<string, unknown>;
}

function isCatalogEntry(value: unknown): value is NativeCatalogEntry {
  return value !== null && typeof value === "object" && !Array.isArray(value) &&
    typeof (value as { name?: unknown }).name === "string";
}

function catalogDir(): string {
  const override = process.env.GODOT_AUTOPILOT_CLI_HOME;
  if (override !== undefined && override.trim() !== "") return path.resolve(override.trim());
  return path.join(gdaRoot(), "cli");
}

export function catalogPath(): string {
  return path.join(catalogDir(), "catalog.json");
}

export function computeCatalogHash(tools: NativeCatalogEntry[]): string {
  const canonical = [...tools]
    .sort((a, b) => a.name.localeCompare(b.name))
    .map((entry) => ({
      name: entry.name,
      description: entry.description,
      category: entry.category,
      tags: [...entry.tags].sort(),
      aliases: [...entry.aliases].sort(),
      mutating: entry.mutating,
      high_risk: entry.high_risk,
    }));
  return fnv1a32(JSON.stringify(canonical));
}

export function makeCatalogDocument(tools: NativeCatalogEntry[]): CatalogDocument {
  return { schema_version: CATALOG_SCHEMA_VERSION, hash: computeCatalogHash(tools), tools: [...tools] };
}

export function readOfflineCatalog(): CatalogDocument {
  const file = catalogPath();
  let raw: string;
  try {
    raw = fs.readFileSync(file, "utf8");
  } catch {
    throw new CliError(ExitCode.Transport, `no offline catalog at ${file} (run 'gda tool list' against a live instance first).`);
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch {
    throw new CliError(ExitCode.Transport, `cannot parse the offline catalog at ${file}.`);
  }
  if (parsed === null || typeof parsed !== "object" || Array.isArray(parsed)) {
    throw new CliError(ExitCode.Transport, `cannot parse the offline catalog at ${file}.`);
  }
  const doc = parsed as Partial<CatalogDocument>;
  if (doc.schema_version !== CATALOG_SCHEMA_VERSION || !Array.isArray(doc.tools) || typeof doc.hash !== "string") {
    throw new CliError(ExitCode.Transport, `unsupported offline catalog schema at ${file}.`);
  }
  const tools = doc.tools.filter(isCatalogEntry);
  if (computeCatalogHash(tools) !== doc.hash) {
    throw new CliError(ExitCode.Transport, `offline catalog hash mismatch at ${file}; the file was modified or truncated.`);
  }
  return { schema_version: CATALOG_SCHEMA_VERSION, hash: doc.hash, tools };
}

export function hasOfflineCatalog(): boolean {
  try {
    fs.accessSync(catalogPath(), fs.constants.R_OK);
    return true;
  } catch {
    return false;
  }
}

export function writeOfflineCatalog(tools: NativeCatalogEntry[]): CatalogDocument {
  const doc = makeCatalogDocument(tools);
  const dir = catalogDir();
  fs.mkdirSync(dir, { recursive: true });
  const target = catalogPath();
  const tmp = `${target}.${process.pid}.tmp`;
  fs.writeFileSync(tmp, `${JSON.stringify(doc, null, 2)}\n`);
  fs.renameSync(tmp, target);
  return doc;
}

export function filterCatalogEntries(
  tools: NativeCatalogEntry[],
  query: { category?: string; tags?: string[]; text?: string; limit?: number },
): NativeCatalogEntry[] {
  const tags = (query.tags ?? []).filter((tag) => tag !== "");
  const text = (query.text ?? "").trim().toLowerCase();
  const filtered = tools.filter((entry) => {
    if (query.category !== undefined && entry.category !== query.category) return false;
    if (tags.length > 0 && !tags.every((tag) => entry.tags.includes(tag))) return false;
    if (text === "") return true;
    const haystack = `${entry.name} ${entry.description} ${entry.category} ${entry.aliases.join(" ")}`.toLowerCase();
    return haystack.includes(text);
  });
  const limit = query.limit ?? filtered.length;
  return filtered.slice(0, Math.max(0, limit));
}

export function catalogCategories(tools: NativeCatalogEntry[]): string[] {
  return [...new Set(tools.map((entry) => entry.category).filter((category) => category !== ""))].sort();
}

export interface NativeCallChallenge {
  ttl_ms: number;
  issued_at_ms: number;
  param_digest: string;
  nonce: string;
}

export interface NativeCallOutcome {
  ok: boolean;
  status: string;
  payload: unknown;
  task_id: string;
  request_id: string;
  artifact_id: string;
  digest: string;
  challenge?: NativeCallChallenge;
  error_code: number;
  error_message: string;
}

export interface NativeCallOptions {
  confirm?: boolean;
  timeoutMs?: number;
}

let callCounter = 0;

function callParamValue(value: unknown): string {
  if (typeof value === "string") return value;
  if (value === undefined || value === null) return "";
  if (typeof value === "number" || typeof value === "boolean") return String(value);
  return JSON.stringify(value);
}

export function encodeCallParams(args: Record<string, unknown>): Record<string, string> {
  const encoded: Record<string, string> = {};
  for (const key of Object.keys(args).sort()) {
    encoded[key] = callParamValue(args[key]);
  }
  return encoded;
}

function callOutcome(response: GcpEnvelopeLike, requestId: string): NativeCallOutcome {
  const status = typeof response.status === "string" ? response.status : "";
  let raw: unknown;
  const resultParam = (response.params ?? []).find((entry) => entry.key === "result");
  if (resultParam !== undefined) {
    try {
      raw = JSON.parse(resultParam.value);
    } catch {
      raw = undefined;
    }
  }
  const record: Record<string, unknown> = raw !== null && typeof raw === "object" && !Array.isArray(raw)
    ? raw as Record<string, unknown>
    : {};
  const challenge = response.confirmation;
  return {
    ok: status === "ok",
    status,
    payload: record.payload !== undefined ? record.payload : raw ?? null,
    task_id: typeof record.task_id === "string" ? record.task_id : "",
    request_id: typeof record.request_id === "string" ? record.request_id : requestId,
    artifact_id: typeof record.artifact_id === "string" ? record.artifact_id : "",
    digest: typeof record.digest === "string" ? record.digest : "",
    challenge: challenge === undefined ? undefined : {
      ttl_ms: typeof challenge.ttl_ms === "number" ? challenge.ttl_ms : 0,
      issued_at_ms: typeof challenge.issued_at_ms === "number" ? challenge.issued_at_ms : 0,
      param_digest: typeof challenge.param_digest === "string" ? challenge.param_digest : "",
      nonce: typeof challenge.nonce === "string" ? challenge.nonce : "",
    },
    error_code: typeof response.error === "number" ? response.error : 0,
    error_message: typeof response.error_message === "string" ? response.error_message : "",
  };
}

export async function nativeCallStart(
  endpoint: IpcEndpoint,
  instanceId: string,
  tool: string,
  args: Record<string, unknown>,
  options: NativeCallOptions = {},
): Promise<NativeCallOutcome> {
  callCounter += 1;
  const requestId = `gda-call-${process.pid}-${callCounter}`;
  const params: Record<string, string> = { tool, ...encodeCallParams(args) };
  if (options.confirm === true) params.confirm = "true";
  const response = await nativeRequest(
    endpoint, "call.start", params, instanceId, options.timeoutMs, requestId,
  );
  return callOutcome(response, requestId);
}

export async function nativeCallConfirm(
  endpoint: IpcEndpoint,
  instanceId: string,
  requestId: string,
  options: NativeCallOptions = {},
): Promise<NativeCallOutcome> {
  const response = await nativeRequest(
    endpoint, "call.confirm", { confirm: "true", request_id: requestId },
    instanceId, options.timeoutMs,
  );
  return callOutcome(response, requestId);
}

export async function nativeCallGet(
  endpoint: IpcEndpoint,
  instanceId: string,
  taskId: string,
  options: NativeCallOptions = {},
): Promise<NativeCallOutcome> {
  const response = await nativeRequest(
    endpoint, "call.get", { task_id: taskId }, instanceId, options.timeoutMs,
  );
  return callOutcome(response, taskId);
}

const TERMINAL_TASK_STATES = new Set([
  "succeeded", "success", "completed", "complete",
  "failed", "error", "cancelled", "canceled",
]);

function taskRecord(outcome: NativeCallOutcome): Record<string, unknown> {
  const value = outcome.payload;
  return value !== null && typeof value === "object" && !Array.isArray(value)
    ? value as Record<string, unknown>
    : {};
}

export async function nativeToolInvoke(
  endpoint: IpcEndpoint,
  instanceId: string,
  tool: string,
  args: Record<string, unknown>,
  options: NativeCallOptions = {},
): Promise<NativeCallOutcome> {
  const started = await nativeCallStart(endpoint, instanceId, tool, args, options);
  if (started.status === "needs_confirmation" && options.confirm === true) {
    return nativeCallConfirm(endpoint, instanceId, started.request_id, options);
  }
  if (started.status !== "pending" || started.task_id === "") {
    return started;
  }
  const deadline = Date.now() + (options.timeoutMs ?? DEFAULT_NATIVE_TIMEOUT_MS);
  let current = started;
  while (Date.now() < deadline) {
    await new Promise((resolve) => setTimeout(resolve, 50));
    current = await nativeCallGet(endpoint, instanceId, started.task_id, options);
    const record = taskRecord(current);
    const state = typeof record.state === "string" ? record.state : "";
    if (TERMINAL_TASK_STATES.has(state)) {
      const errorCode = typeof record.error === "number" ? record.error : current.error_code;
      const errorMessage = typeof record.error_message === "string" ? record.error_message : current.error_message;
      return {
        ...current,
        ok: errorCode === 0 || state === "succeeded" || state === "success" || state === "completed" || state === "complete",
        payload: record.result !== undefined ? record.result : current.payload,
        error_code: errorCode,
        error_message: errorMessage,
      };
    }
  }
  return {
    ...current,
    ok: false,
    error_message: `native task ${started.task_id} did not reach a terminal state in time.`,
  };
}

export interface NativeTaskRecord {
  task_id: string;
  instance_id: string;
  generation: number;
  state: string;
  progress: number;
  progress_message: string;
  result: unknown;
  error: number;
  error_symbol: string;
  error_message: string;
  cancel_stage: string;
}

export interface NativeEventRecord {
  seq: number;
  instance_id: string;
  event_type: string;
  payload: unknown;
  traceparent: string;
  task_id: string;
  request_id: string;
}

export interface NativeEventPage {
  subscription_id: string;
  events: NativeEventRecord[];
  delivered: number;
  next_cursor: number;
}

export interface NativeTraceContext {
  traceparent: string;
  trace_id: string;
  span_id: string;
  parent_span: string;
  flags: string;
}

export interface NativeErrorInfo {
  code: number;
  message: string;
}

export const NATIVE_ERROR_CURSOR_EXPIRED = 10;
export const NATIVE_ERROR_TASK_NOT_FOUND = 14;

export function nativeErrorInfo(response: GcpEnvelopeLike): NativeErrorInfo | undefined {
  if (response.status === "ok" || response.status === "pending") return undefined;
  const code = typeof response.error === "number" ? response.error : 0;
  const message = response.error_message ?? "";
  if (code === 0 && message === "") return undefined;
  return { code, message };
}

function nativeResultRaw(response: GcpEnvelopeLike, method: string): unknown {
  const failure = nativeErrorInfo(response);
  if (failure !== undefined) {
    const error = new CliError(
      ExitCode.Transport,
      `native ${method} failed (code ${failure.code})${failure.message === "" ? "" : `: ${failure.message}`}.`,
    );
    (error as CliError & { gcp_code?: number }).gcp_code = failure.code;
    throw error;
  }
  return gcpResult(response);
}

function nativeResultRecord(response: GcpEnvelopeLike, method: string): Record<string, unknown> {
  const result = nativeResultRaw(response, method);
  if (result === null || typeof result !== "object" || Array.isArray(result)) {
    throw new CliError(ExitCode.Transport, `the native ${method} response is malformed.`);
  }
  return result as Record<string, unknown>;
}

function recordText(record: Record<string, unknown>, key: string): string {
  const value = record[key];
  return typeof value === "string" ? value : "";
}

function recordNumber(record: Record<string, unknown>, key: string): number {
  const value = record[key];
  return typeof value === "number" && Number.isFinite(value) ? value : 0;
}

function parseTaskRecord(record: Record<string, unknown>): NativeTaskRecord {
  return {
    task_id: recordText(record, "task_id"),
    instance_id: recordText(record, "instance_id"),
    generation: recordNumber(record, "generation"),
    state: recordText(record, "state"),
    progress: recordNumber(record, "progress"),
    progress_message: recordText(record, "progress_message"),
    result: record.result ?? null,
    error: recordNumber(record, "error"),
    error_symbol: recordText(record, "error_symbol"),
    error_message: recordText(record, "error_message"),
    cancel_stage: recordText(record, "cancel_stage"),
  };
}

export async function nativeTaskGet(
  endpoint: IpcEndpoint,
  instanceId: string,
  taskId: string,
  generation: number,
  options: NativeCallOptions = {},
): Promise<NativeTaskRecord> {
  const response = await nativeRequest(
    endpoint, "task.get", { task_id: taskId, generation: String(generation) },
    instanceId, options.timeoutMs,
  );
  return parseTaskRecord(nativeResultRecord(response, "task.get"));
}

export async function nativeTaskList(
  endpoint: IpcEndpoint,
  instanceId: string,
  generation: number,
  options: NativeCallOptions = {},
): Promise<NativeTaskRecord[]> {
  const response = await nativeRequest(
    endpoint, "task.list", { generation: String(generation) }, instanceId, options.timeoutMs,
  );
  const raw = nativeResultRaw(response, "task.list");
  const record = raw !== null && typeof raw === "object" && !Array.isArray(raw)
    ? raw as Record<string, unknown>
    : {};
  const tasks = Array.isArray(record.tasks) ? record.tasks : raw;
  if (!Array.isArray(tasks)) {
    throw new CliError(ExitCode.Transport, "the native task.list response is malformed.");
  }
  return tasks.filter((item) => item !== null && typeof item === "object" && !Array.isArray(item))
    .map((item) => parseTaskRecord(item as Record<string, unknown>));
}

export async function nativeTaskCancel(
  endpoint: IpcEndpoint,
  instanceId: string,
  taskId: string,
  generation: number,
  reason: string,
  options: NativeCallOptions = {},
): Promise<NativeTaskRecord> {
  const response = await nativeRequest(
    endpoint, "task.cancel",
    { task_id: taskId, generation: String(generation), reason },
    instanceId, options.timeoutMs,
  );
  return parseTaskRecord(nativeResultRecord(response, "task.cancel"));
}

function parseEventRecord(value: unknown): NativeEventRecord {
  const record = value !== null && typeof value === "object" && !Array.isArray(value)
    ? value as Record<string, unknown>
    : {};
  return {
    seq: recordNumber(record, "seq"),
    instance_id: recordText(record, "instance_id"),
    event_type: recordText(record, "event_type"),
    payload: record.payload ?? null,
    traceparent: recordText(record, "traceparent"),
    task_id: recordText(record, "task_id"),
    request_id: recordText(record, "request_id"),
  };
}

export async function nativeEventsSubscribe(
  endpoint: IpcEndpoint,
  instanceId: string,
  cursor: number,
  eventType: string,
  options: NativeCallOptions = {},
): Promise<string> {
  const response = await nativeRequest(
    endpoint, "events.subscribe",
    { cursor: String(cursor), event_type: eventType === "" ? "all" : eventType },
    instanceId, options.timeoutMs,
  );
  const result = nativeResultRecord(response, "events.subscribe");
  const id = recordText(result, "subscription_id");
  if (id === "") {
    throw new CliError(ExitCode.Transport, "the native events.subscribe response has no subscription_id.");
  }
  return id;
}

export async function nativeEventsResume(
  endpoint: IpcEndpoint,
  instanceId: string,
  subscriptionId: string,
  limit: number,
  options: NativeCallOptions = {},
): Promise<NativeEventPage> {
  const response = await nativeRequest(
    endpoint, "events.resume",
    { subscription_id: subscriptionId, limit: String(limit) },
    instanceId, options.timeoutMs,
  );
  const result = nativeResultRecord(response, "events.resume");
  const events = (Array.isArray(result.events) ? result.events : []).map(parseEventRecord);
  const nextCursor = events.length === 0 ? 0 : events[events.length - 1].seq + 1;
  return {
    subscription_id: subscriptionId,
    events,
    delivered: recordNumber(result, "delivered"),
    next_cursor: nextCursor,
  };
}

export async function nativeTraceGet(
  endpoint: IpcEndpoint,
  instanceId: string,
  traceparent: string,
  options: NativeCallOptions = {},
): Promise<NativeTraceContext> {
  const params: Record<string, string> | undefined = traceparent === "" ? undefined : { traceparent };
  const response = await nativeRequest(endpoint, "trace.get", params, instanceId, options.timeoutMs);
  const result = nativeResultRecord(response, "trace.get");
  return {
    traceparent: recordText(result, "traceparent"),
    trace_id: recordText(result, "trace_id"),
    span_id: recordText(result, "span_id"),
    parent_span: recordText(result, "parent_span"),
    flags: recordText(result, "flags"),
  };
}
