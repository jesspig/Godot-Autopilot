import { describe, expect, test } from "bun:test";

import { callTool, request } from "../src/rpc/client";
import { CLI_VERSION } from "../src/version";
import { CliError, ExitCode } from "../src/errors";
import { extractPayload } from "../src/payload";
import { rpcError, rpcResult, startNativeServer, sendHandshake, contentResult } from "./support/native-ipc";

async function catchError(p: Promise<unknown>): Promise<CliError> {
  try {
    await p;
  } catch (e) {
    expect(e).toBeInstanceOf(CliError);
    return e as CliError;
  }
  throw new Error("expected rejection but the promise resolved");
}

describe("callTool", () => {
  test("原生 IPC 成功帧", async () => {
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      send(rpcResult(message.id, { content: [{ type: "text", text: '{"ok":true}' }] }));
    });
    try {
      const res = await callTool("system_status", {}, { port: server.port });
      expect(typeof res.id).toBe("number");
      expect(res.result).toEqual({ content: [{ type: "text", text: '{"ok":true}' }] });
    } finally {
      await server.close();
    }
  });

  test("进度通知帧与结果帧分别处理", async () => {
    const seen: number[] = [];
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      if (message.method === "notifications/initialized") return;
      send({ jsonrpc: "2.0", method: "notifications/progress", params: { progress: 1, total: 2 } });
      send(rpcResult(message.id, { content: [{ type: "text", text: '{"a":"b"}' }] }));
    });
    try {
      const res = await callTool("system_status", { verbose: true }, {
        port: server.port,
        progressSink: (progress) => seen.push(progress.progress),
      });
      expect(seen).toEqual([1]);
      expect(res.result).toEqual({ content: [{ type: "text", text: '{"a":"b"}' }] });
    } finally {
      await server.close();
    }
  });

  test("JSON-RPC error 原样返回", async () => {
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      send(rpcError(message.id, -32000, "bad tool"));
    });
    try {
      const res = await callTool("nope", {}, { port: server.port });
      expect(res.error?.code).toBe(-32000);
      expect(res.error?.message).toBe("bad tool");
    } finally {
      await server.close();
    }
  });

  test("failed task 透传 task.error 结构化数据并经 extractPayload 展示", async () => {
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      if (message.method === "tools/call") {
        send(rpcResult(message.id, { resultType: "task", taskId: "t-fail" }));
        return;
      }
      if (message.method === "tasks/get") {
        send(rpcResult(message.id, {
          taskId: "t-fail",
          status: "failed",
          statusMessage: "boom",
          error: { code: -32603, message: "boom", data: { structured_error: { code: "backend_down" } } },
        }));
        return;
      }
      send(rpcError(message.id, -32600, "unexpected method"));
    });
    try {
      const res = await callTool("x", {}, { port: server.port });
      expect(res.error?.code).toBe(-32603);
      expect(res.error?.message).toBe("boom");
      const data = res.error?.data as { structured_error?: { code?: string } };
      expect(data.structured_error?.code).toBe("backend_down");
      const extracted = extractPayload(res);
      expect(extracted.toolFailed).toBe(true);
      expect(extracted.failureMessage).toContain("backend_down");
    } finally {
      await server.close();
    }
  });

  test("call_tool 包装结构", async () => {
    let seen: Record<string, unknown> | undefined;
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      if (message.method === "tools/call") {
        seen = message;
        send(rpcResult(message.id, { content: [] }));
        return;
      }
    });
    try {
      await callTool("move_agent", { x: 1 }, { port: server.port });
      expect(seen?.method).toBe("tools/call");
      expect(seen?.params).toEqual({ name: "call_tool", arguments: { name: "move_agent", arguments: { x: 1 } } });
    } finally {
      await server.close();
    }
  });

  test("initialize 声明的协议版本与客户端信息固定", async () => {
    const initParams: unknown[] = [];
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        initParams.push(message.params);
        send(rpcResult(message.id, { protocolVersion: "1", capabilities: {}, serverInfo: {} }));
        return;
      }
      send(rpcResult(message.id, { content: [] }));
    });
    try {
      await callTool("x", {}, { port: server.port });
      expect(initParams[0]).toEqual({
        protocolVersion: "1",
        capabilities: { elicitation: {} },
        clientInfo: { name: "gda-cli", version: CLI_VERSION },
      });
    } finally {
      await server.close();
    }
  });

  test("超时", async () => {
    const server = await startNativeServer(async (message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      await Bun.sleep(400);
      send(rpcResult(message.id, { content: [] }));
    });
    try {
      const err = await catchError(callTool("x", {}, { port: server.port, timeoutSec: 0.05 }));
      expect(err.code).toBe(ExitCode.Transport);
      expect(err.message).toContain("timed out");
    } finally {
      await server.close();
    }
  });

  test("非法 JSON 帧报传输错误", async () => {
    const payload = Buffer.from("{not-json}", "utf8");
    const header = Buffer.alloc(4);
    header.writeUInt32BE(payload.length, 0);
    const invalid = Buffer.concat([header, payload]);
    const server = await startNativeServer((message, socket, send) => {
      if (message.method === "initialize") {
        socket.write(invalid);
        return;
      }
      send(rpcResult(message.id, { content: [] }));
    });
    try {
      const err = await catchError(callTool("x", {}, { port: server.port }));
      expect(err.code).toBe(ExitCode.Transport);
      expect(err.message).toContain("not valid JSON");
    } finally {
      await server.close();
    }
  });

  test("请求体超过 4 MiB 在发送前报错", async () => {
    const err = await catchError(callTool("x", { blob: "x".repeat(4 * 1024 * 1024 + 16) }, { port: 1 }));
    expect(err.code).toBe(ExitCode.Transport);
    expect(err.message).toContain("request body exceeds 4 MiB");
  });

  test("IPC token 失败时不调用工具且不改投其它实例", async () => {
    let presentedToken: unknown;
    let toolCalls = 0;
    const ipc = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        const params = message.params as Record<string, unknown> | undefined;
        presentedToken = (params?._meta as Record<string, unknown> | undefined)?.gda_token;
        send(rpcError(message.id, -32001, "IPC token rejected"));
        return;
      }
      toolCalls += 1;
      send(rpcResult(message.id, { content: [] }));
    });
    const alternate = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      send(rpcResult(message.id, { content: [] }));
    });
    try {
      const err = await catchError(request("resources/list", {}, {
        port: alternate.port,
        timeoutSec: 5,
        ipcEndpoint: { kind: "tcp", host: "127.0.0.1", port: ipc.port, token: "registry-token" },
      }));
      expect(err.code).toBe(ExitCode.Transport);
      expect(err.message).toContain("IPC authentication failed");
      expect(presentedToken).toBe("registry-token");
      expect(toolCalls).toBe(0);
    } finally {
      await ipc.close();
      await alternate.close();
    }
  });

  test("长度前缀帧严格按声明长度切分，多个连续帧顺序解析", async () => {
    const methods: string[] = [];
    const server = await startNativeServer((message, _socket, send) => {
      methods.push(String(message.method));
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      if (message.method === "notifications/initialized") return;
      send(rpcResult(message.id, { content: [{ type: "text", text: "{}" }] }));
    });
    try {
      const res = await callTool("system_status", {}, { port: server.port });
      expect(res.result).toEqual({ content: [{ type: "text", text: "{}" }] });
      expect(methods).toEqual(["initialize", "notifications/initialized", "tools/call"]);
    } finally {
      await server.close();
    }
  });
});
