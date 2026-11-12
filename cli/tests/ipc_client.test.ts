import fs from "node:fs";
import net from "node:net";
import path from "node:path";

import { afterEach, beforeEach, describe, expect, test } from "bun:test";

import { requestOverIpc } from "../src/ipc/client";
import { callTool } from "../src/mcp/client";
import { run as runCall } from "../src/commands/call";
import { CliError, ExitCode } from "../src/errors";

function captureOutput(): { stderr(): string; restore(): void } {
  const origErr = process.stderr.write;
  let errBuf = "";
  process.stderr.write = ((chunk: unknown) => {
    errBuf += String(chunk);
    return true;
  }) as typeof process.stderr.write;
  return {
    stderr: () => errBuf,
    restore: () => {
      process.stderr.write = origErr;
    },
  };
}

function encodeFrame(value: unknown): Buffer {
  const payload = Buffer.from(JSON.stringify(value), "utf8");
  const header = Buffer.alloc(4);
  header.writeUInt32BE(payload.length, 0);
  return Buffer.concat([header, payload]);
}

function frameServer(handler: (message: any, socket: net.Socket) => void): (socket: net.Socket) => void {
  return (socket) => {
    let buffer = Buffer.alloc(0);
    socket.on("data", (chunk: Buffer) => {
      buffer = Buffer.concat([buffer, chunk]);
      while (buffer.length >= 4) {
        const size = buffer.readUInt32BE(0);
        if (buffer.length < 4 + size) return;
        const text = buffer.subarray(4, 4 + size).toString("utf8");
        buffer = buffer.subarray(4 + size);
        try {
          handler(JSON.parse(text), socket);
        } catch {
        }
      }
    });
  };
}

const SOCKET_NAME = "i17a.sock";

const activeSockets: net.Socket[] = [];

function listenLoopback(socketPath: string, handler: (socket: net.Socket) => void): Promise<net.Server> {
  return new Promise((resolve, reject) => {
    const server = net.createServer(handler);
    server.on("connection", (socket) => activeSockets.push(socket));
    server.once("error", reject);
    server.listen(socketPath, () => resolve(server));
  });
}

function closeServer(server: net.Server): Promise<void> {
  for (const socket of activeSockets.splice(0)) {
    socket.destroy();
  }
  return new Promise((resolve) => server.close(() => resolve()));
}

let socketPath: string;

beforeEach(() => {
  socketPath = path.join(process.cwd(), SOCKET_NAME);
  fs.rmSync(socketPath, { force: true });
});

afterEach(() => {
  fs.rmSync(socketPath, { force: true });
});

describe("ipc client", () => {
  test("长度前缀 JSON-RPC 请求-响应往返", async () => {
    const server = await listenLoopback(
      socketPath,
      frameServer((request, socket) => {
        socket.write(encodeFrame({ id: request.id, result: { tools: [] } }));
      }),
    );
    try {
      const response = await requestOverIpc({ path: socketPath, timeoutMs: 2000 }, { jsonrpc: "2.0", id: 1, method: "tools/list" });
      expect(response.id).toBe(1);
      expect(response.result).toEqual({ tools: [] });
    } finally {
      await closeServer(server);
    }
  });

  test("socket 不存在映射为传输错误并提示", async () => {
    let caught: unknown;
    try {
      await requestOverIpc({ path: socketPath, timeoutMs: 1000 }, { jsonrpc: "2.0", id: 1, method: "ping" });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Transport);
    expect((caught as CliError).message).toContain("does not exist");
  });

  test("非法 JSON 帧映射为传输错误", async () => {
    const payload = Buffer.from("{not-json}", "utf8");
    const header = Buffer.alloc(4);
    header.writeUInt32BE(payload.length, 0);
    const invalid = Buffer.concat([header, payload]);
    const server = await listenLoopback(socketPath, (socket) => {
      socket.on("data", () => socket.write(invalid));
    });
    try {
      let caught: unknown;
      try {
        await requestOverIpc({ path: socketPath, timeoutMs: 2000 }, { jsonrpc: "2.0", id: 1, method: "ping" });
      } catch (error) {
        caught = error;
      }
      expect(caught).toBeInstanceOf(CliError);
      expect((caught as CliError).code).toBe(ExitCode.Transport);
      expect((caught as CliError).message).toContain("not valid JSON");
    } finally {
      await closeServer(server);
    }
  });

  test("超时映射为传输错误", async () => {
    const server = await listenLoopback(socketPath, () => {});
    try {
      let caught: unknown;
      try {
        await requestOverIpc({ path: socketPath, timeoutMs: 150 }, { jsonrpc: "2.0", id: 1, method: "ping" });
      } catch (error) {
        caught = error;
      }
      expect(caught).toBeInstanceOf(CliError);
      expect((caught as CliError).code).toBe(ExitCode.Transport);
      expect((caught as CliError).message).toContain("timed out");
    } finally {
      await closeServer(server);
    }
  });

  test("连接提前关闭映射为传输错误", async () => {
    const server = await listenLoopback(socketPath, (socket) => {
      socket.on("data", () => socket.destroy());
    });
    try {
      let caught: unknown;
      try {
        await requestOverIpc({ path: socketPath, timeoutMs: 2000 }, { jsonrpc: "2.0", id: 1, method: "ping" });
      } catch (error) {
        caught = error;
      }
      expect(caught).toBeInstanceOf(CliError);
      expect((caught as CliError).code).toBe(ExitCode.Transport);
    } finally {
      await closeServer(server);
    }
  });
});
function respondServer(): (socket: net.Socket) => void {
  return frameServer((message, socket) => {
    if (message.id === undefined) return;
    const result =
      message.method === "initialize"
        ? { protocolVersion: "2025-11-25", capabilities: {}, serverInfo: { name: "mock", version: "1" } }
        : { content: [{ type: "text", text: "{}" }] };
    socket.write(encodeFrame({ jsonrpc: "2.0", id: message.id, result }));
  });
}

describe("ipc session and socket channel", () => {
  test("callTool 经 IPC 完成握手与调用", async () => {
    const server = await listenLoopback(socketPath, respondServer());
    try {
      const response = await callTool("ping", {}, { port: 0, socketPath, timeoutSec: 5 });
      expect(typeof response.id).toBe("number");
      expect(response.result).toEqual({ content: [{ type: "text", text: "{}" }] });
    } finally {
      await closeServer(server);
    }
  });

  test("call --socket 失败且 --no-fallback 时直接报传输错误", async () => {
    let caught: unknown;
    try {
      await runCall(["ping"], { socketText: socketPath, noFallback: true });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Transport);
  });

  test("call --socket 失败时提示回退到端口通道", async () => {
    const cap = captureOutput();
    let caught: unknown;
    try {
      await runCall(["ping"], { socketText: socketPath, portText: "1" });
    } catch (error) {
      caught = error;
    } finally {
      cap.restore();
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Transport);
    expect(cap.stderr()).toContain("falling back");
  });
});
