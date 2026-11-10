import fs from "node:fs";
import net from "node:net";
import path from "node:path";

import { afterEach, beforeEach, describe, expect, test } from "bun:test";

import { requestOverIpc } from "../src/ipc/client";
import { CliError, ExitCode } from "../src/errors";

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
  test("单行 JSON-RPC 请求-响应往返", async () => {
    const server = await listenLoopback(socketPath, (socket) => {
      let buffer = "";
      socket.on("data", (chunk) => {
        buffer += chunk.toString("utf8");
        const newline = buffer.indexOf("\n");
        if (newline === -1) return;
        const request = JSON.parse(buffer.slice(0, newline));
        socket.write(`${JSON.stringify({ id: request.id, result: { tools: [] } })}\n`);
      });
    });
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
    const server = await listenLoopback(socketPath, (socket) => {
      socket.on("data", () => socket.write("{not-json}\n"));
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