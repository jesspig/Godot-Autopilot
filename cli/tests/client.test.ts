import { afterEach, describe, expect, test } from "bun:test";
import { callTool } from "../src/mcp/client";
import { CliError, ExitCode } from "../src/errors";

const servers: Bun.Server[] = [];

afterEach(() => {
  while (servers.length > 0) {
    servers.pop()!.stop();
  }
});

function serve(handler: (req: Request) => Response | Promise<Response>): number {
  const server = Bun.serve({ port: 0, fetch: handler });
  servers.push(server);
  return server.port;
}

function sseResponse(payload: unknown, split = false): Response {
  if (split) {
    const lines = JSON.stringify(payload, null, 2).split("\n");
    return new Response(`event: message\n${lines.map((l) => `data: ${l}`).join("\n")}\n\n`, {
      headers: { "content-type": "text/event-stream" },
    });
  }
  return new Response(`event: message\ndata: ${JSON.stringify(payload)}\n\n`, {
    headers: { "content-type": "text/event-stream" },
  });
}

function jsonResponse(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json" } });
}

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
  test("SSE 成功帧", async () => {
    const port = serve(() =>
      sseResponse({ jsonrpc: "2.0", id: 1, result: { content: [{ type: "text", text: "{\"ok\":true}" }] } }),
    );
    const res = await callTool("system_status", {}, { port });
    expect(res.id).toBe(1);
    expect(res.result).toEqual({ content: [{ type: "text", text: "{\"ok\":true}" }] });
  });

  test("SSE 多行 data: 帧拼接后解析", async () => {
    const payload = { jsonrpc: "2.0", id: 7, result: { content: [{ type: "text", text: "{\"a\":\"b\",\"c\":[1,2,3]}" }] } };
    const port = serve(() => sseResponse(payload, true));
    const res = await callTool("system_status", { verbose: true }, { port });
    expect(res.id).toBe(7);
    expect(res.result).toEqual(payload.result);
  });

  test("纯 JSON 响应原样返回", async () => {
    const port = serve(() => jsonResponse({ jsonrpc: "2.0", id: 3, error: { code: -32000, message: "bad tool" } }));
    const res = await callTool("nope", {}, { port });
    expect(res.error?.code).toBe(-32000);
    expect(res.error?.message).toBe("bad tool");
  });

  test("call_tool 包装结构", async () => {
    let seen: { method?: string; params?: unknown; id?: number };
    const port = serve(async (req) => {
      seen = (await req.json()) as { method?: string; params?: unknown; id?: number };
      return sseResponse({ jsonrpc: "2.0", id: seen.id, result: { content: [] } });
    });
    await callTool("move_agent", { x: 1 }, { port });
    expect(seen!.method).toBe("tools/call");
    expect(seen!.params).toEqual({ name: "call_tool", arguments: { name: "move_agent", arguments: { x: 1 } } });
  });

  test("懒握手：400 → initialized → 递增 id 重发成功", async () => {
    const calls: { method?: string; params?: unknown; id?: number }[] = [];
    const port = serve(async (req) => {
      const body = (await req.json()) as { method?: string; params?: unknown; id?: number };
      calls.push(body);
      if (calls.length === 1) {
        return jsonResponse({ jsonrpc: "2.0", id: 1, error: { code: -32600, message: "not initialized" } }, 400);
      }
      if (calls.length === 2) {
        expect(body.method).toBe("notifications/initialized");
        return new Response("{}", { status: 200 });
      }
      return sseResponse({ jsonrpc: "2.0", id: body.id, result: { content: [{ type: "text", text: "{}" }] } });
    });
    const res = await callTool("system_status", {}, { port });
    expect(calls.length).toBe(3);
    expect(calls[2].id).toBeGreaterThan(calls[0].id!);
    expect(res.result).toEqual({ content: [{ type: "text", text: "{}" }] });
  });

  test("握手请求非 2xx 报传输错误", async () => {
    let n = 0;
    const port = serve(async () => {
      n += 1;
      if (n === 1) return jsonResponse({ error: { code: -32600, message: "x" } }, 400);
      return new Response("{}", { status: 500 });
    });
    const err = await catchError(callTool("x", {}, { port }));
    expect(err.code).toBe(ExitCode.Transport);
    expect(err.message).toBe("handshake failed with HTTP 500.");
  });

  test("超时", async () => {
    const port = serve(async () => {
      await Bun.sleep(400);
      return new Response("{}", { status: 200 });
    });
    const err = await catchError(callTool("x", {}, { port, timeoutSec: 0.05 }));
    expect(err.code).toBe(ExitCode.Transport);
    expect(err.message).toBe("request timed out.");
  });

  test("HTTP 非 2xx 纯文本附 server 消息", async () => {
    const port = serve(() => new Response("boom", { status: 500 }));
    const err = await catchError(callTool("x", {}, { port }));
    expect(err.message).toBe("HTTP 500. Server says: boom.");
  });

  test("HTTP 非 2xx JSON error 附 code", async () => {
    const port = serve(() => jsonResponse({ error: { code: -32001, message: "nope" } }, 403));
    const err = await catchError(callTool("x", {}, { port }));
    expect(err.message).toBe("HTTP 403. Server says: nope (code -32001).");
  });

  test("HTTP 非 2xx 顶层 message", async () => {
    const port = serve(() => jsonResponse({ message: "not found" }, 404));
    const err = await catchError(callTool("x", {}, { port }));
    expect(err.message).toBe("HTTP 404. Server says: not found.");
  });

  test("SSE 无 data: 行", async () => {
    const port = serve(() => new Response("event: message\n\n", { headers: { "content-type": "text/event-stream" } }));
    const err = await catchError(callTool("x", {}, { port }));
    expect(err.code).toBe(ExitCode.Transport);
    expect(err.message).toBe("could not parse the SSE response (no usable data: frame).");
  });

  test("SSE data 非 JSON", async () => {
    const port = serve(() => new Response("data: not-json\n\n", { headers: { "content-type": "text/event-stream" } }));
    const err = await catchError(callTool("x", {}, { port }));
    expect(err.message).toBe("could not parse the SSE response (no usable data: frame).");
  });

  test("纯 JSON 响应非 JSON", async () => {
    const port = serve(() => new Response("<html>oops</html>", { headers: { "content-type": "application/json" } }));
    const err = await catchError(callTool("x", {}, { port }));
    expect(err.message).toBe("could not parse the plain-JSON response.");
  });

  test("连不上服务", async () => {
    const server = Bun.serve({ port: 0, fetch: () => new Response("x") });
    const port = server.port;
    server.stop();
    const err = await catchError(callTool("x", {}, { port }));
    expect(err.code).toBe(ExitCode.Transport);
    expect(err.message).toContain("cannot reach the server (");
  });

  test("请求体超过 4 MiB 在发送前报错", async () => {
    const err = await catchError(callTool("x", { blob: "x".repeat(4 * 1024 * 1024 + 16) }, { port: 1 }));
    expect(err.code).toBe(ExitCode.Transport);
    expect(err.message).toContain("request body exceeds 4 MiB");
  });

  test("严格路径：请求打在 /mcp，/mcp/ 返回 404", async () => {
    const paths: string[] = [];
    const port = serve(async (req) => {
      const p = new URL(req.url).pathname;
      paths.push(p);
      if (p === "/mcp") {
        return sseResponse({ jsonrpc: "2.0", id: 1, result: { content: [{ type: "text", text: "{}" }] } });
      }
      return jsonResponse({ message: "not found" }, 404);
    });
    const res = await callTool("system_status", {}, { port });
    expect(res.result).toEqual({ content: [{ type: "text", text: "{}" }] });
    expect(paths.length).toBeGreaterThan(0);
    for (const p of paths) {
      expect(p).toBe("/mcp");
    }
  });
});
