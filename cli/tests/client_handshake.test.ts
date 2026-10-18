import { afterEach, describe, expect, test } from "bun:test";
import { callTool } from "../src/mcp/client";
import { CliError, ExitCode } from "../src/errors";
import { CLI_VERSION } from "../src/version";

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

function sseResponse(payload: unknown): Response {
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

interface Recorded {
  method?: string;
  version: string | null;
}

function serveLazyHandshakeServer(
  initResult: Record<string, unknown> | null,
): { port: number; calls: Recorded[] } {
  const calls: Recorded[] = [];
  let initialized = false;
  const port = serve(async (req) => {
    const body = (await req.json()) as { method?: string; id?: number };
    calls.push({ method: body.method, version: req.headers.get("mcp-protocol-version") });
    if (body.method === "initialize") {
      initialized = true;
      return sseResponse({
        jsonrpc: "2.0",
        id: body.id,
        result: initResult ?? { capabilities: {}, serverInfo: { name: "gda-testbed", version: "1.0.0" } },
      });
    }
    if (body.method === "notifications/initialized") {
      return new Response("", { status: 202 });
    }
    if (!initialized) {
      return jsonResponse({ error: { code: -32600, message: "invalid request: tools/call" } }, 400);
    }
    return sseResponse({ jsonrpc: "2.0", id: body.id, result: { content: [{ type: "text", text: "{}" }] } });
  });
  return { port, calls };
}

const SERVER_INFO = { name: "gda-testbed", version: "1.0.0" };

describe("callTool 协议版本协商", () => {
  test("initialize 声明 2025-11-25，握手后按响应学习值携带 MCP-Protocol-Version 头", async () => {
    const seen: { params?: unknown }[] = [];
    let initialized = false;
    const port = serve(async (req) => {
      const body = (await req.json()) as { method?: string; params?: unknown; id?: number };
      if (body.method === "initialize") {
        seen.push({ params: body.params });
        initialized = true;
        return sseResponse({
          jsonrpc: "2.0",
          id: body.id,
          result: { protocolVersion: "2025-11-25", capabilities: {}, serverInfo: SERVER_INFO },
        });
      }
      if (body.method === "notifications/initialized") {
        return new Response("", { status: 202 });
      }
      if (!initialized) {
        return jsonResponse({ error: { code: -32600, message: "invalid request: tools/call" } }, 400);
      }
      return sseResponse({ jsonrpc: "2.0", id: body.id, result: { content: [{ type: "text", text: "{}" }] } });
    });
    const res = await callTool("system_status", {}, { port });
    expect(res.result).toEqual({ content: [{ type: "text", text: "{}" }] });
    expect(seen[0].params).toEqual({
      protocolVersion: "2025-11-25",
      capabilities: {},
      clientInfo: { name: "gda-cli", version: CLI_VERSION },
    });
  });

  test("握手后从 initialize 响应学习协商版本，后续请求携带 MCP-Protocol-Version 头", async () => {
    const { port, calls } = serveLazyHandshakeServer({
      protocolVersion: "2025-11-25",
      capabilities: {},
      serverInfo: SERVER_INFO,
    });
    const res = await callTool("system_status", {}, { port });
    expect(res.result).toEqual({ content: [{ type: "text", text: "{}" }] });
    expect(calls.map((c) => c.method)).toEqual(["tools/call", "initialize", "notifications/initialized", "tools/call"]);
    expect(calls[0].version).toBeNull();
    expect(calls[1].version).toBeNull();
    expect(calls[2].version).toBe("2025-11-25");
    expect(calls[3].version).toBe("2025-11-25");
  });

  test("initialize 响应缺 protocolVersion 时不发送版本头", async () => {
    const { port, calls } = serveLazyHandshakeServer(null);
    await callTool("system_status", {}, { port });
    expect(calls.every((c) => c.version === null)).toBe(true);
  });

  test("modern 协商版本（2026-07-28）不发送 MCP-Protocol-Version 头", async () => {
    const { port, calls } = serveLazyHandshakeServer({
      protocolVersion: "2026-07-28",
      capabilities: {},
      serverInfo: SERVER_INFO,
    });
    await callTool("system_status", {}, { port });
    expect(calls.every((c) => c.version === null)).toBe(true);
  });

  test("协商版本不在已知五版本表内时不发送版本头", async () => {
    const { port, calls } = serveLazyHandshakeServer({
      protocolVersion: "2099-01-01",
      capabilities: {},
      serverInfo: SERVER_INFO,
    });
    await callTool("system_status", {}, { port });
    expect(calls.every((c) => c.version === null)).toBe(true);
  });

  test("initialize 失败不学习版本，保持无头行为", async () => {
    let n = 0;
    const port = serve(async () => {
      n += 1;
      if (n === 1) return jsonResponse({ error: { code: -32600, message: "x" } }, 400);
      return new Response("{}", { status: 500 });
    });
    const err = await catchError(callTool("x", {}, { port }));
    expect(err.code).toBe(ExitCode.Transport);
    expect(err.message).toBe("initialize failed with HTTP 500.");
  });
});
