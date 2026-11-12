import net from "node:net";

import { CliError, ExitCode } from "../errors";
import type { JsonRpcResponse } from "../mcp/client";

export type IpcKind = "unix" | "tcp" | "pipe";

export interface IpcEndpoint {
  kind?: IpcKind;
  path?: string;
  name?: string;
  host?: string;
  port?: number;
  token?: string;
  framing?: string;
  timeoutMs?: number;
}

const DEFAULT_TIMEOUT_MS = 60_000;
const MAX_FRAME_BYTES = 4 * 1024 * 1024;

function endpointLabel(endpoint: IpcEndpoint): string {
  if (endpoint.kind === "tcp") {
    return `tcp ${endpoint.host ?? "127.0.0.1"}:${endpoint.port ?? 0}`;
  }
  if (endpoint.kind === "pipe") {
    return `pipe ${endpoint.name ?? ""}`;
  }
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
    if (buffer.length - offset - 4 < size) {
      break;
    }
    frames.push(buffer.subarray(offset + 4, offset + 4 + size).toString("utf8"));
    offset += 4 + size;
  }
  return { frames, remainder: buffer.subarray(offset), overflow: false };
}

function mapSocketError(error: NodeJS.ErrnoException, endpoint: IpcEndpoint): CliError {
  const suffix = ` (${endpointLabel(endpoint)})`;
  switch (error.code) {
    case "ENOENT":
      return new CliError(
        ExitCode.Transport,
        `the IPC endpoint does not exist${suffix}; the editor may be closed or the plugin did not bind the channel.`,
        "Retry with --port or check that the editor is running with the plugin enabled.",
      );
    case "ECONNREFUSED":
      return new CliError(
        ExitCode.Transport,
        `the IPC endpoint refused the connection${suffix}; the editor may be shutting down.`,
        "Retry, or fall back to the port channel with --port.",
      );
    case "EACCES":
      return new CliError(ExitCode.Transport, `permission denied connecting to the IPC endpoint${suffix}.`);
    default:
      return new CliError(
        ExitCode.Transport,
        `IPC connection failed${suffix}: ${error.message ?? "unknown error"}`,
      );
  }
}

interface PendingRequest {
  resolve: (response: JsonRpcResponse) => void;
  reject: (error: Error) => void;
  timer: ReturnType<typeof setTimeout>;
}

export class IpcSession {
  private socket: net.Socket | null = null;
  private buffer: Buffer = Buffer.alloc(0);
  private nextId = 1;
  private readonly pending = new Map<number, PendingRequest>();
  private readonly endpoint: IpcEndpoint;
  private readonly timeoutMs: number;

  constructor(endpoint: string | IpcEndpoint, timeoutMs: number = DEFAULT_TIMEOUT_MS) {
    this.endpoint = typeof endpoint === "string" ? { path: endpoint } : endpoint;
    this.timeoutMs = this.endpoint.timeoutMs ?? timeoutMs;
  }

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
        const mapped = mapSocketError(error, this.endpoint);
        if (!opened) reject(mapped);
        this.failAll(mapped);
      });
      socket.once("close", () => {
        const error = new CliError(
          ExitCode.Transport,
          `the IPC connection closed (${endpointLabel(this.endpoint)}).`,
        );
        if (!opened) reject(error);
        this.failAll(error);
      });
      socket.on("data", (chunk: Buffer) => {
        this.onData(chunk);
      });
    });
  }

  private onData(chunk: Buffer): void {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    if (this.buffer.length > MAX_FRAME_BYTES + 4) {
      this.failAll(new CliError(ExitCode.Transport, "the IPC response exceeds the 4 MiB frame limit."));
      return;
    }
    const decoded = decodeFrames(this.buffer);
    this.buffer = decoded.remainder;
    if (decoded.overflow) {
      this.failAll(new CliError(ExitCode.Transport, "the IPC response exceeds the 4 MiB frame limit."));
      return;
    }
    for (const text of decoded.frames) {
      let parsed: JsonRpcResponse;
      try {
        parsed = JSON.parse(text) as JsonRpcResponse;
      } catch {
        this.failAll(new CliError(ExitCode.Transport, "the IPC server returned a frame that is not valid JSON."));
        return;
      }
      const id = typeof parsed.id === "number" ? parsed.id : null;
      if (id === null) continue;
      const entry = this.pending.get(id);
      if (entry === undefined) continue;
      clearTimeout(entry.timer);
      this.pending.delete(id);
      entry.resolve(parsed);
    }
  }

  request(envelope: Record<string, unknown>): Promise<JsonRpcResponse> {
    if (this.socket === null) {
      return Promise.reject(new CliError(ExitCode.Transport, "the IPC session is not open."));
    }
    const id = typeof envelope.id === "number" ? envelope.id : this.nextId++;
    const text = JSON.stringify({ ...envelope, id });
    if (Buffer.byteLength(text, "utf8") > MAX_FRAME_BYTES) {
      return Promise.reject(new CliError(ExitCode.Transport, "the IPC request exceeds the 4 MiB frame limit."));
    }
    const socket = this.socket;
    return new Promise<JsonRpcResponse>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(
          new CliError(
            ExitCode.Transport,
            `the IPC request timed out after ${this.timeoutMs} ms (${endpointLabel(this.endpoint)}).`,
          ),
        );
      }, this.timeoutMs);
      this.pending.set(id, { resolve, reject, timer });
      socket.write(encodeFrame(text));
    });
  }

  notify(envelope: Record<string, unknown>): void {
    this.socket?.write(encodeFrame(JSON.stringify(envelope)));
  }

  close(): void {
    if (this.socket !== null) {
      this.socket.destroy();
      this.socket = null;
    }
    this.failAll(new CliError(ExitCode.Transport, "the IPC session closed."));
  }

  private failAll(error: Error): void {
    for (const [id, entry] of this.pending) {
      clearTimeout(entry.timer);
      entry.reject(error);
      this.pending.delete(id);
    }
  }
}

export async function requestOverIpc(
  endpoint: string | IpcEndpoint,
  request: Record<string, unknown> | string,
): Promise<JsonRpcResponse> {
  const session = new IpcSession(endpoint);
  try {
    await session.open();
    const envelope =
      typeof request === "string" ? (JSON.parse(request) as Record<string, unknown>) : request;
    return await session.request(envelope);
  } finally {
    session.close();
  }
}
