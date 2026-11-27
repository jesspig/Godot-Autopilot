import net from "node:net";

import { describe, expect, test } from "bun:test";

import { callTool } from "../src/rpc/client";
import { CliError, ExitCode, isIpcAuthRejected, isIpcRequestSent, markIpcAuthRejected, markIpcRequestSent } from "../src/errors";
import { startNativeServer, sendHandshake } from "./support/native-ipc";

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

describe("IPC 发送后失败禁止重发", () => {
  test("tools/call 已收到后连接断开，不重发也不改投其它实例", async () => {
    let seenCall = false;
    const ipc = await startNativeServer((message, socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      if (message.method === "tools/call") {
        seenCall = true;
        socket.destroy();
      }
    });
    let alternateHits = 0;
    const alternate = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      alternateHits += 1;
      send({ jsonrpc: "2.0", id: message.id, result: { content: [] } });
    });
    try {
      let caught: unknown;
      try {
        await callTool("mutating_tool", {}, {
          port: alternate.port,
          timeoutSec: 2,
          ipcEndpoint: { kind: "tcp", host: "127.0.0.1", port: ipc.port, token: "registry-token" },
        });
      } catch (error) {
        caught = error;
      }
      expect(seenCall).toBe(true);
      expect(alternateHits).toBe(0);
      expect(caught).toBeInstanceOf(CliError);
      expect((caught as CliError).code).toBe(ExitCode.Transport);
      expect(isIpcRequestSent(caught)).toBe(true);
    } finally {
      await ipc.close();
      await alternate.close();
    }
  });
});
