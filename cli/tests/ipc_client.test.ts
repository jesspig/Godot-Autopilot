import net from "node:net";

import { describe, expect, test } from "bun:test";

import { requestOverIpc } from "../src/ipc/client";
import { callTool } from "../src/rpc/client";
import { run as runCall } from "../src/commands/call";
import type { GlobalOptions } from "../src/args";
import { CliError, ExitCode } from "../src/errors";

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

const activeSockets = new Set<net.Socket>();

function listenLoopback(handler: (socket: net.Socket) => void): Promise<{ server: net.Server; port: number }> {
  return new Promise((resolve, reject) => {
    const server = net.createServer((socket) => {
      activeSockets.add(socket);
      socket.once("close", () => activeSockets.delete(socket));
      handler(socket);
    });
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () => {
      const address = server.address();
      if (address === null || typeof address === "string") {
        reject(new Error("no port"));
        return;
      }
      resolve({ server, port: address.port });
    });
  });
}

function closeServer(server: net.Server): Promise<void> {
  for (const socket of Array.from(activeSockets)) socket.destroy();
  return new Promise((resolve) => server.close(() => resolve()));
}

function unusedPort(): Promise<number> {
  return listenLoopback(() => {}).then(async ({ server, port }) => {
    await closeServer(server);
    return port;
  });
}

describe("ipc client", () => {
  test("长度前缀 JSON-RPC 请求-响应往返", async () => {
    const { server, port } = await listenLoopback(
      frameServer((request, socket) => {
        socket.write(encodeFrame({ id: request.id, result: { tools: [] } }));
      }),
    );
    try {
      const response = await requestOverIpc(
        { kind: "tcp", host: "127.0.0.1", port, timeoutMs: 2000 },
        { jsonrpc: "2.0", id: 1, method: "tools/list" },
      );
      expect(response.id).toBe(1);
      expect(response.result).toEqual({ tools: [] });
    } finally {
      await closeServer(server);
    }
  });

  test("端口不可达映射为传输错误并提示", async () => {
    const port = await unusedPort();
    let caught: unknown;
    try {
      await requestOverIpc(
        { kind: "tcp", host: "127.0.0.1", port, timeoutMs: 1000 },
        { jsonrpc: "2.0", id: 1, method: "ping" },
      );
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Transport);
    expect((caught as CliError).message).toContain("refused the connection");
  });

  test("非法 JSON 帧映射为传输错误", async () => {
    const payload = Buffer.from("{not-json}", "utf8");
    const header = Buffer.alloc(4);
    header.writeUInt32BE(payload.length, 0);
    const invalid = Buffer.concat([header, payload]);
    const { server, port } = await listenLoopback((socket) => {
      socket.on("data", () => socket.write(invalid));
    });
    try {
      let caught: unknown;
      try {
        await requestOverIpc(
          { kind: "tcp", host: "127.0.0.1", port, timeoutMs: 2000 },
          { jsonrpc: "2.0", id: 1, method: "ping" },
        );
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
    const { server, port } = await listenLoopback(() => {});
    try {
      let caught: unknown;
      try {
        await requestOverIpc(
          { kind: "tcp", host: "127.0.0.1", port, timeoutMs: 150 },
          { jsonrpc: "2.0", id: 1, method: "ping" },
        );
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

  test("分块累计缓冲超过 4 MiB 但单帧合法时按完整帧接收", async () => {
    const big = "a".repeat(4_000_000);
    const { server, port } = await listenLoopback((socket) => {
      socket.on("data", () => {
        const frame = encodeFrame({ id: 1, result: { data: big } });
        for (let offset = 0; offset < frame.length; offset += 64 * 1024) {
          socket.write(frame.subarray(offset, offset + 64 * 1024));
        }
      });
    });
    try {
      const response = await requestOverIpc(
        { kind: "tcp", host: "127.0.0.1", port, timeoutMs: 5000 },
        { jsonrpc: "2.0", id: 1, method: "ping" },
      );
      expect(response.error).toBeUndefined();
      expect((response.result as { data: string }).data).toBe(big);
    } finally {
      await closeServer(server);
    }
  });

  test("单帧声明超过 4 MiB 上限时拒绝", async () => {
    const payload = Buffer.from(JSON.stringify({ id: 1, result: "x" }), "utf8");
    const header = Buffer.alloc(4);
    header.writeUInt32BE(4 * 1024 * 1024 + 1, 0);
    const oversized = Buffer.concat([header, payload]);
    const { server, port } = await listenLoopback((socket) => {
      socket.on("data", () => socket.write(oversized));
    });
    try {
      let caught: unknown;
      try {
        await requestOverIpc(
          { kind: "tcp", host: "127.0.0.1", port, timeoutMs: 2000 },
          { jsonrpc: "2.0", id: 1, method: "ping" },
        );
      } catch (error) {
        caught = error;
      }
      expect(caught).toBeInstanceOf(CliError);
      expect((caught as CliError).code).toBe(ExitCode.Transport);
      expect((caught as CliError).message).toContain("4 MiB frame limit");
    } finally {
      await closeServer(server);
    }
  });

  test("连接提前关闭映射为传输错误", async () => {
    const { server, port } = await listenLoopback((socket) => {
      socket.on("data", () => socket.destroy());
    });
    try {
      let caught: unknown;
      try {
        await requestOverIpc(
          { kind: "tcp", host: "127.0.0.1", port, timeoutMs: 2000 },
          { jsonrpc: "2.0", id: 1, method: "ping" },
        );
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
        ? { protocolVersion: "1", capabilities: {}, serverInfo: { name: "mock", version: "1" } }
        : { content: [{ type: "text", text: "{}" }] };
    socket.write(encodeFrame({ jsonrpc: "2.0", id: message.id, result }));
  });
}

describe("native-only transport", () => {
  test("callTool 以端口为目标时只走原生 IPC 并完成握手", async () => {
    const { server, port } = await listenLoopback(respondServer());
    try {
      const response = await callTool("ping", {}, { port, timeoutSec: 5 });
      expect(typeof response.id).toBe("number");
      expect(response.result).toEqual({ content: [{ type: "text", text: "{}" }] });
    } finally {
      await closeServer(server);
    }
  });

  test("原生 IPC 不可达时直接失败且没有其它通道", async () => {
    const port = await unusedPort();
    let caught: unknown;
    try {
      await callTool("ping", {}, {
        port,
        timeoutSec: 2,
        ipcEndpoint: { kind: "tcp", host: "127.0.0.1", port, token: "bad" },
      });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Transport);
    expect((caught as CliError).message).not.toContain("falling back");
  });
});

describe("ipc session and socket channel", () => {
  test("callTool 经 IPC 完成握手与调用", async () => {
    const { server, port } = await listenLoopback(respondServer());
    try {
      const response = await callTool("ping", {}, {
        port: 0,
        ipcEndpoint: { kind: "tcp", host: "127.0.0.1", port },
        timeoutSec: 5,
      });
      expect(typeof response.id).toBe("number");
      expect(response.result).toEqual({ content: [{ type: "text", text: "{}" }] });
    } finally {
      await closeServer(server);
    }
  });

  test("call --port 失败时直接报传输错误", async () => {
    const port = await unusedPort();
    let caught: unknown;
    try {
      await runCall(["ping"], { portText: String(port) } as unknown as GlobalOptions);
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Transport);
    expect((caught as CliError).message).not.toContain("falling back");
  });
});