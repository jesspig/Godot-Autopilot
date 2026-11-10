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
  request: Record<string, unknown>,
): Promise<JsonRpcResponse> {
  const timeoutMs = endpoint.timeoutMs ?? DEFAULT_TIMEOUT_MS;
  const payload = `${JSON.stringify(request)}\n`;
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