import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import { describe, expect, test } from "bun:test";

import { run as runCall } from "../src/commands/call";
import { CliError, ExitCode, isIpcRequestSent } from "../src/errors";

function encodeFrame(value: unknown): Buffer {
  const payload = Buffer.from(JSON.stringify(value), "utf8");
  const header = Buffer.alloc(4);
  header.writeUInt32BE(payload.length, 0);
  return Buffer.concat([header, payload]);
}

function frameHandler(onCall: (socket: net.Socket, id: number) => void): (socket: net.Socket) => void {
  return (socket) => {
    let buffer = Buffer.alloc(0);
    socket.on("data", (chunk: Buffer) => {
      buffer = Buffer.concat([buffer, chunk]);
      while (buffer.length >= 4) {
        const size = buffer.readUInt32BE(0);
        if (buffer.length < 4 + size) return;
        const text = buffer.subarray(4, 4 + size).toString("utf8");
        buffer = buffer.subarray(4 + size);
        let message: { id?: number; method?: string };
        try {
          message = JSON.parse(text) as { id?: number; method?: string };
        } catch {
          return;
        }
        if (message.id === undefined) continue;
        if (message.method === "tools/call") {
          onCall(socket, message.id);
        } else {
          socket.write(encodeFrame({ jsonrpc: "2.0", id: message.id, result: {} }));
        }
      }
    });
  };
}

async function listenUnix(
  onCall: (socket: net.Socket, id: number) => void,
): Promise<{ socketPath: string; close: () => Promise<void> }> {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-ipc-call-"));
  const socketPath = path.join(dir, "ipc.sock");
  const sockets = new Set<net.Socket>();
  const handler = frameHandler(onCall);
  const server = net.createServer((socket) => {
    sockets.add(socket);
    socket.once("close", () => sockets.delete(socket));
    handler(socket);
  });
  try {
    await new Promise<void>((resolve, reject) => {
      server.once("error", reject);
      server.listen(socketPath, resolve);
    });
  } catch (error) {
    fs.rmSync(dir, { recursive: true, force: true });
    throw error;
  }
  return {
    socketPath,
    close: async () => {
      for (const socket of sockets) {
        socket.destroy();
      }
      await new Promise<void>((resolve) => server.close(() => resolve()));
      fs.rmSync(dir, { recursive: true, force: true });
    },
  };
}

function unusedSocketPath(): { socketPath: string; cleanup: () => void } {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-ipc-call-"));
  return {
    socketPath: path.join(dir, "missing.sock"),
    cleanup: () => fs.rmSync(dir, { recursive: true, force: true }),
  };
}

function serveHttp(onToolCall: () => void): ReturnType<typeof Bun.serve> {
  return Bun.serve({
    port: 0,
    hostname: "127.0.0.1",
    fetch: async (req) => {
      const body = await req.json() as { id?: number; method?: string };
      if (body.method === "initialize") {
        return Response.json({
          jsonrpc: "2.0",
          id: body.id ?? 1,
          result: {
            protocolVersion: "2025-11-25",
            capabilities: {},
            serverInfo: { name: "mock", version: "1" },
          },
        });
      }
      if (body.method === "notifications/initialized") {
        return new Response(null, { status: 202 });
      }
      if (body.method === "tools/call") {
        onToolCall();
        return Response.json({
          jsonrpc: "2.0",
          id: body.id ?? 1,
          result: { content: [{ type: "text", text: '{"ok":true}' }] },
        });
      }
      return Response.json({ jsonrpc: "2.0", id: body.id ?? 1, error: { code: -32600, message: "unexpected method" } }, { status: 400 });
    },
  });
}

function captureOutput(): { stdout(): string; stderr(): string; restore(): void } {
  const originalOut = process.stdout.write;
  const originalErr = process.stderr.write;
  let stdout = "";
  let stderr = "";
  process.stdout.write = ((chunk: unknown) => {
    stdout += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  process.stderr.write = ((chunk: unknown) => {
    stderr += String(chunk);
    return true;
  }) as typeof process.stderr.write;
  return {
    stdout: () => stdout,
    stderr: () => stderr,
    restore: () => {
      process.stdout.write = originalOut;
      process.stderr.write = originalErr;
    },
  };
}

async function catchCliError(promise: Promise<number>): Promise<CliError> {
  try {
    await promise;
  } catch (error) {
    expect(error).toBeInstanceOf(CliError);
    return error as CliError;
  }
  throw new Error("expected rejection but the promise resolved");
}

describe("call --socket 回退安全", () => {
  test("IPC timeout 后不回退 HTTP，工具调用仅一次", async () => {
    let ipcCalls = 0;
    const ipc = await listenUnix(() => {
      ipcCalls += 1;
    });
    let httpCalls = 0;
    const http = serveHttp(() => {
      httpCalls += 1;
    });
    const output = captureOutput();
    let caught: unknown;
    try {
      caught = await catchCliError(runCall(["mutating_tool"], {
        socketText: ipc.socketPath,
        portText: String(http.port),
        timeoutText: "0.2",
      }));
    } finally {
      output.restore();
      http.stop(true);
      await ipc.close();
    }
    expect((caught as CliError).code).toBe(ExitCode.Transport);
    expect(isIpcRequestSent(caught)).toBe(true);
    expect((caught as CliError).message).toContain("timed out");
    expect(ipcCalls).toBe(1);
    expect(httpCalls).toBe(0);
    expect(output.stderr()).not.toContain("falling back");
  });

  test("IPC 收到工具调用后断开不回退 HTTP，工具调用仅一次", async () => {
    let ipcCalls = 0;
    const ipc = await listenUnix((socket) => {
      ipcCalls += 1;
      socket.destroy();
    });
    let httpCalls = 0;
    const http = serveHttp(() => {
      httpCalls += 1;
    });
    const output = captureOutput();
    let caught: unknown;
    try {
      caught = await catchCliError(runCall(["mutating_tool"], {
        socketText: ipc.socketPath,
        portText: String(http.port),
        timeoutText: "2",
      }));
    } finally {
      output.restore();
      http.stop(true);
      await ipc.close();
    }
    expect((caught as CliError).code).toBe(ExitCode.Transport);
    expect(isIpcRequestSent(caught)).toBe(true);
    expect((caught as CliError).message).toContain("execution result is unknown");
    expect(ipcCalls).toBe(1);
    expect(httpCalls).toBe(0);
    expect(output.stderr()).not.toContain("falling back");
  });

  test("IPC 请求未写入时仍回退 HTTP，工具调用仅一次", async () => {
    const endpoint = unusedSocketPath();
    let httpCalls = 0;
    const http = serveHttp(() => {
      httpCalls += 1;
    });
    const output = captureOutput();
    let code: number = ExitCode.Success;
    try {
      code = await runCall(["mutating_tool"], {
        socketText: endpoint.socketPath,
        portText: String(http.port),
        timeoutText: "2",
      });
    } finally {
      output.restore();
      http.stop(true);
      endpoint.cleanup();
    }
    expect(code).toBe(ExitCode.Success);
    expect(httpCalls).toBe(1);
    expect(output.stdout()).toBe('{"ok":true}\n');
    expect(output.stderr()).toContain("falling back");
  });
});
