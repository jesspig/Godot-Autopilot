import net from "node:net";

import { CliError, ExitCode } from "../errors";
import type { JsonRpcResponse } from "../mcp/client";

export interface IpcEndpoint {
  path: string;
  timeoutMs?: number;
}

const DEFAULT_TIMEOUT_MS = 60_000;
const MAX_FRAME_BYTES = 4 * 1024 * 1024;

function mapSocketError(error: NodeJS.ErrnoException, socketPath: string): CliError {
  const suffix = ` (socket ${socketPath})`;
  switch (error.code) {
    case "ENOENT":
      return new CliError(
        ExitCode.Transport,
        `the IPC socket does not exist${suffix}; the editor may be closed or the plugin did not bind the channel.`,
        "Retry with --port or check that the editor is running with the plugin enabled.",
      );
    case "ECONNREFUSED":
      return new CliError(
        ExitCode.Transport,
        `the IPC socket refused the connection${suffix}; the editor may be shutting down.`,
        "Retry, or fall back to the port channel with --port.",
      );
    case "EACCES":
      return new CliError(ExitCode.Transport, `permission denied connecting to the IPC socket${suffix}.`);
    default:
      return new CliError(
        ExitCode.Transport,
        `IPC connection failed${suffix}: ${error.message ?? "unknown error"}`,
      );
  }
}

export async function requestOverIpc(
  endpoint: IpcEndpoint,
  request: Record<string, unknown> | string,
): Promise<JsonRpcResponse> {
  const timeoutMs = endpoint.timeoutMs ?? DEFAULT_TIMEOUT_MS;
  const serialized = typeof request === "string" ? request : JSON.stringify(request);
  const payload = `${serialized}\n`;
  if (Buffer.byteLength(payload, "utf8") > MAX_FRAME_BYTES) {
    throw new CliError(ExitCode.Transport, "the IPC request exceeds the 4 MiB frame limit.");
  }
  return await new Promise<JsonRpcResponse>((resolve, reject) => {
    let settled = false;
    let buffered = "";
    const socket = net.createConnection({ path: endpoint.path });
    const finish = (error: CliError | null, response?: JsonRpcResponse): void => {
      if (settled) return;
      settled = true;
      socket.destroy();
      if (error !== null) {
        reject(error);
      } else if (response !== undefined) {
        resolve(response);
      }
    };
    const timer = setTimeout(() => {
      finish(new CliError(ExitCode.Transport, `the IPC request timed out after ${timeoutMs} ms (socket ${endpoint.path}).`));
    }, timeoutMs);
    socket.once("error", (error: NodeJS.ErrnoException) => {
      clearTimeout(timer);
      finish(mapSocketError(error, endpoint.path));
    });
    socket.once("connect", () => {
      socket.write(payload);
    });
    socket.on("data", (chunk: Buffer) => {
      buffered += chunk.toString("utf8");
      if (Buffer.byteLength(buffered, "utf8") > MAX_FRAME_BYTES) {
        clearTimeout(timer);
        finish(new CliError(ExitCode.Transport, "the IPC response exceeds the 4 MiB frame limit."));
        return;
      }
      const newline = buffered.indexOf("\n");
      if (newline === -1) return;
      const line = buffered.slice(0, newline).trim();
      clearTimeout(timer);
      if (line === "") {
        finish(new CliError(ExitCode.Transport, "the IPC server returned an empty frame."));
        return;
      }
      try {
        finish(null, JSON.parse(line) as JsonRpcResponse);
      } catch {
        finish(new CliError(ExitCode.Transport, "the IPC server returned a frame that is not valid JSON."));
      }
    });
    socket.once("close", () => {
      if (!settled) {
        clearTimeout(timer);
        finish(new CliError(ExitCode.Transport, `the IPC connection closed before a full frame arrived (socket ${endpoint.path}).`));
      }
    });
  });
}
interface PendingRequest {
  resolve: (response: JsonRpcResponse) => void;
  reject: (error: Error) => void;
  timer: ReturnType<typeof setTimeout>;
}

export class IpcSession {
  private socket: net.Socket | null = null;
  private buffer = "";
  private nextId = 1;
  private readonly pending = new Map<number, PendingRequest>();
  private readonly path: string;
  private readonly timeoutMs: number;

  constructor(socketPath: string, timeoutMs: number = DEFAULT_TIMEOUT_MS) {
    this.path = socketPath;
    this.timeoutMs = timeoutMs;
  }

  open(): Promise<void> {
    return new Promise((resolve, reject) => {
      const socket = net.createConnection({ path: this.path });
      this.socket = socket;
      let opened = false;
      socket.once("connect", () => {
        opened = true;
        resolve();
      });
      socket.once("error", (error: NodeJS.ErrnoException) => {
        const mapped = mapSocketError(error, this.path);
        if (!opened) reject(mapped);
        this.failAll(mapped);
      });
      socket.once("close", () => {
        const error = new CliError(ExitCode.Transport, `the IPC connection closed (socket ${this.path}).`);
        if (!opened) reject(error);
        this.failAll(error);
      });
      socket.on("data", (chunk: Buffer) => {
        this.onData(chunk);
      });
    });
  }

  private onData(chunk: Buffer): void {
    this.buffer += chunk.toString("utf8");
    if (Buffer.byteLength(this.buffer, "utf8") > MAX_FRAME_BYTES) {
      this.failAll(new CliError(ExitCode.Transport, "the IPC response exceeds the 4 MiB frame limit."));
      return;
    }
    let newline = this.buffer.indexOf("\n");
    while (newline !== -1) {
      const line = this.buffer.slice(0, newline).trim();
      this.buffer = this.buffer.slice(newline + 1);
      newline = this.buffer.indexOf("\n");
      if (line === "") continue;
      let parsed: JsonRpcResponse;
      try {
        parsed = JSON.parse(line) as JsonRpcResponse;
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
    const id = this.nextId++;
    const text = `${JSON.stringify({ ...envelope, id })}\n`;
    if (Buffer.byteLength(text, "utf8") > MAX_FRAME_BYTES) {
      return Promise.reject(new CliError(ExitCode.Transport, "the IPC request exceeds the 4 MiB frame limit."));
    }
    const socket = this.socket;
    return new Promise<JsonRpcResponse>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new CliError(ExitCode.Transport, `the IPC request timed out after ${this.timeoutMs} ms (socket ${this.path}).`));
      }, this.timeoutMs);
      this.pending.set(id, { resolve, reject, timer });
      socket.write(text);
    });
  }

  notify(envelope: Record<string, unknown>): void {
    this.socket?.write(`${JSON.stringify(envelope)}\n`);
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
