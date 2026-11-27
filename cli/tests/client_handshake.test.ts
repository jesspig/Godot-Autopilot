import { describe, expect, test } from "bun:test";

import { callTool, getHandshakeInfo } from "../src/rpc/client";
import { CliError, ExitCode } from "../src/errors";
import { CLI_VERSION } from "../src/version";
import { rpcError, rpcResult, startNativeServer } from "./support/native-ipc";

async function catchError(p: Promise<unknown>): Promise<CliError> {
  try {
    await p;
  } catch (e) {
    expect(e).toBeInstanceOf(CliError);
    return e as CliError;
  }
  throw new Error("expected rejection but the promise resolved");
}

const SERVER_INFO = { name: "gda-testbed", version: "1.0.0" };

describe("callTool 原生握手", () => {
  test("initialize 声明原生协议版本，握手后只发一次 tools/call", async () => {
    const initParams: unknown[] = [];
    const methods: string[] = [];
    const server = await startNativeServer((message, _socket, send) => {
      methods.push(String(message.method));
      if (message.method === "initialize") {
        initParams.push(message.params);
        send(rpcResult(message.id, {
          protocolVersion: "1",
          capabilities: { tools: {} },
          serverInfo: SERVER_INFO,
        }));
        return;
      }
      if (message.method === "notifications/initialized") return;
      send(rpcResult(message.id, { content: [{ type: "text", text: "{}" }] }));
    });
    try {
      const res = await callTool("system_status", {}, { port: server.port });
      expect(res.result).toEqual({ content: [{ type: "text", text: "{}" }] });
      expect(methods).toEqual(["initialize", "notifications/initialized", "tools/call"]);
      expect(initParams[0]).toEqual({
        protocolVersion: "1",
        capabilities: { elicitation: {} },
        clientInfo: { name: "gda-cli", version: CLI_VERSION },
      });
      expect(getHandshakeInfo(server.port)?.protocol_version).toBe("1");
      expect(getHandshakeInfo(server.port)?.server_info).toEqual(SERVER_INFO);
    } finally {
      await server.close();
    }
  });

  test("initialize 携带注册表中的 IPC token，且不使用任何 HTTP 头", async () => {
    let presented: unknown;
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        const params = message.params as Record<string, unknown> | undefined;
        presented = (params?._meta as Record<string, unknown> | undefined)?.gda_token;
        send(rpcResult(message.id, { protocolVersion: "1", capabilities: {}, serverInfo: SERVER_INFO }));
        return;
      }
      if (message.method === "notifications/initialized") return;
      send(rpcResult(message.id, { content: [] }));
    });
    try {
      await callTool("system_status", {}, {
        port: 0,
        ipcEndpoint: { kind: "tcp", host: "127.0.0.1", port: server.port, token: "registry-token" },
      });
      expect(presented).toBe("registry-token");
    } finally {
      await server.close();
    }
  });

  test("initialize 返回 token 错误时不重发工具调用", async () => {
    let calls = 0;
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        send(rpcError(message.id, -32001, "IPC token rejected"));
        return;
      }
      calls += 1;
      send(rpcResult(message.id, { content: [] }));
    });
    try {
      const err = await catchError(callTool("x", {}, { port: server.port }));
      expect(err.code).toBe(ExitCode.Transport);
      expect(err.message).toContain("IPC authentication failed");
      expect(calls).toBe(0);
    } finally {
      await server.close();
    }
  });

  test("initialize 业务错误作为结果透传，不重试", async () => {
    let calls = 0;
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        send(rpcError(message.id, -32600, "initialize refused"));
        return;
      }
      calls += 1;
      send(rpcResult(message.id, { content: [] }));
    });
    try {
      const res = await callTool("x", {}, { port: server.port });
      expect(res.error?.code).toBe(-32600);
      expect(res.error?.message).toBe("initialize refused");
      expect(calls).toBe(0);
    } finally {
      await server.close();
    }
  });

  test("服务端重启后下一次调用重新握手，不复用失效会话", async () => {
    const methods: string[] = [];
    let valid = true;
    const server = await startNativeServer((message, _socket, send) => {
      methods.push(String(message.method));
      if (message.method === "initialize") {
        valid = true;
        send(rpcResult(message.id, { protocolVersion: "1", capabilities: {}, serverInfo: SERVER_INFO }));
        return;
      }
      if (message.method === "notifications/initialized") return;
      if (!valid) {
        send(rpcError(message.id, -32000, "stale session"));
        return;
      }
      send(rpcResult(message.id, { content: [{ type: "text", text: "{}" }] }));
    });
    try {
      const first = await callTool("system_status", {}, { port: server.port });
      expect(first.result).toEqual({ content: [{ type: "text", text: "{}" }] });
      valid = false;
      const second = await callTool("system_status", {}, { port: server.port });
      expect(second.result).toEqual({ content: [{ type: "text", text: "{}" }] });
      expect(methods).toEqual([
        "initialize", "notifications/initialized", "tools/call",
        "initialize", "notifications/initialized", "tools/call",
      ]);
    } finally {
      await server.close();
    }
  });

  test("IPC 不可达时报传输错误且不尝试任何其它通道", async () => {
    const probe = await startNativeServer(() => {});
    const port = probe.port;
    await probe.close();
    const err = await catchError(callTool("x", {}, { port }));
    expect(err.code).toBe(ExitCode.Transport);
    expect(err.message).toContain("refused the connection");
  });
});
