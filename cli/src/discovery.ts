import fs from "node:fs";
import net from "node:net";
import path from "node:path";

import { CliError, ExitCode, markIpcRequestSent } from "./errors";
import { fnv1a32 } from "./hash";
import type { IpcEndpoint } from "./ipc/client";
import { gdaRoot } from "./platform";

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
    trace: {},
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
        resolve();
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
): Promise<GcpEnvelopeLike> {
  const session = new GcpIpcSession(endpoint, timeoutMs);
  try {
    await session.open();
    return await session.request(buildGcpRequest(method, params, instanceId, timeoutMs));
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
