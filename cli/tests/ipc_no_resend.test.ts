import net from "node:net";

import { describe, expect, test } from "bun:test";

import { callTool } from "../src/mcp/client";
import { run as runCall } from "../src/commands/call";
import { CliError, ExitCode, isIpcAuthRejected, isIpcRequestSent, markIpcAuthRejected, markIpcRequestSent } from "../src/errors";

function encodeFrame(value: unknown): Buffer {
  const payload = Buffer.from(JSON.stringify(value), "utf8");
  const header = Buffer.alloc(4);
  header.writeUInt32BE(payload.length, 0);
  return Buffer.concat([header, payload]);
}

function frameServer(onCall: (socket: net.Socket, id: number) => void): Promise<{ port: number; close: () => Promise<void> }> {
  const server = net.createServer((socket) => {
    let buffer = Buffer.alloc(0);
    socket.on("data", (chunk: Buffer) => {
      buffer = Buffer.concat([buffer, chunk]);
      while (buffer.length >= 4) {
        const size = buffer.readUInt32BE(0);
        if (buffer.length < 4 + size) return;
        const text = buffer.subarray(4, 4 + size).toString("utf8");
        buffer = buffer.subarray(4 + size);
        let msg: { id?: number; method?: string };
        try { msg = JSON.parse(text); } catch { return; }
        if (msg.method === "tools/call") onCall(socket, msg.id ?? 1);
        else socket.write(encodeFrame({ jsonrpc: "2.0", id: msg.id, result: {} }));
      }
    });
  });
  return new Promise((resolve) => {
    server.listen(0, "127.0.0.1", () => {
      const address = server.address();
      const port = typeof address === "object" && address ? address.port : 0;
      resolve({ port, close: () => new Promise((done) => server.close(() => done())) });
    });
  });
}

describe("IPC 失败分类标记", () => {
  test("请求已发送与认证拒绝标记互不混淆", () => {
    const plain = new CliError(ExitCode.Transport, "connection refused");
    expect(isIpcRequestSent(plain)).toBe(false);
    expect(isIpcAuthRejected(plain)).toBe(false);

    const sent = markIpcRequestSent(new CliError(ExitCode.Transport, "closed"));
    expect(isIpcRequestSent(sent)).toBe(true);
    expect(isIpcAuthRejected(sent)).toBe(false);

    const auth = markIpcAuthRejected(new CliError(ExitCode.Transport, "token rejected"));
    expect(isIpcAuthRejected(auth)).toBe(true);
    expect(isIpcRequestSent(auth)).toBe(false);
  });
});

describe("IPC 发送后失败禁止回退重发", () => {
  test("tools/call 已收到后连接断开，auto 模式不回退 HTTP", async () => {
    let httpHits = 0;
    const http = Bun.serve({ port: 0, fetch: () => { httpHits += 1; return Response.json({ jsonrpc: "2.0", id: 1, result: { content: [] } }); } });
    let seenCall = false;
    const ipc = await frameServer((socket) => {
      seenCall = true;
      socket.destroy();
    });
    try {
      let caught: unknown;
      try {
        await callTool("mutating_tool", {}, {
          port: http.port!,
          timeoutSec: 2,
          transport: "auto",
          ipcEndpoint: { kind: "tcp", host: "127.0.0.1", port: ipc.port, token: "test-token" },
        });
      } catch (error) {
        caught = error;
      }
      expect(seenCall).toBe(true);
      expect(httpHits).toBe(0);
      expect(caught).toBeInstanceOf(CliError);
      expect((caught as CliError).code).toBe(ExitCode.Transport);
    } finally {
      http.stop();
      await ipc.close();
    }
  });
});
