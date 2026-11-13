import { afterEach, describe, expect, test } from "bun:test";

import { run as call } from "../src/commands/call";
import { run as status } from "../src/commands/status";
import { ExitCode } from "../src/errors";

const servers: ReturnType<typeof Bun.serve>[] = [];

afterEach(() => {
  while (servers.length > 0) servers.pop()!.stop();
});

function sseRaw(frames: unknown[]): Response {
  const body = frames
    .map((frame) => `event: message\ndata: ${JSON.stringify(frame)}\n\n`)
    .join("");
  return new Response(body, { headers: { "content-type": "text/event-stream" } });
}

function sse(result: unknown): Response {
  return sseRaw([{ jsonrpc: "2.0", id: 1, result }]);
}

function start(fetch: (body: { method: string; id: number; params?: unknown }) => Response) {
  const instance = Bun.serve({
    port: 0,
    fetch: async (req) => {
      const body = (await req.json()) as { method: string; id: number; params?: unknown };
      return fetch(body);
    },
  });
  servers.push(instance);
  return instance.port!;
}

function capture(): { stdout(): string; stderr(): string; restore(): void } {
  const out = process.stdout.write;
  const errw = process.stderr.write;
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
      process.stdout.write = out;
      process.stderr.write = errw;
    },
  };
}

describe("handshake capabilities", () => {
  test("status 暴露 mcp 协议版本与 capabilities", async () => {
    const port = start((body) => {
      if (body.method === "initialize") {
        return sse({
          protocolVersion: "2026-07-28",
          capabilities: {
            tools: { listChanged: true },
            extensions: { "io.modelcontextprotocol/tasks": {} },
          },
          serverInfo: { name: "gda", version: "0.3.3" },
        });
      }
      if (body.method === "notifications/initialized") return new Response(null, { status: 202 });
      return sse({
        content: [{ type: "text", text: JSON.stringify({ version: "0.3.3", tool_count: 459 }) }],
      });
    });
    const cap = capture();
    const code = await status([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const payload = JSON.parse(cap.stdout()) as {
      mcp: { protocol_version: string; capabilities: Record<string, unknown>; server_info: Record<string, unknown> };
    };
    expect(payload.mcp.protocol_version).toBe("2026-07-28");
    expect(payload.mcp.capabilities).toEqual({
      tools: { listChanged: true },
      extensions: { "io.modelcontextprotocol/tasks": {} },
    });
    expect(payload.mcp.server_info).toEqual({ name: "gda", version: "0.3.3" });
  });

  test("input_required 无 --input 时退出 3 并提示", async () => {
    const port = start((body) => {
      if (body.method === "initialize") return sse({ protocolVersion: "2025-11-25", capabilities: {} });
      if (body.method === "notifications/initialized") return new Response(null, { status: 202 });
      return sse({ input_required: { input_requests: { confirm: { method: "elicitation/create", params: {} } } } });
    });
    const cap = capture();
    const code = await call(["do_risky_thing"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Usage);
    expect(cap.stderr()).toContain("--input");
    expect(cap.stdout()).toBe("");
  });

  test("input_required 带 --input 时重试", async () => {
    let calls = 0;
    const port = start((body) => {
      if (body.method === "initialize") return sse({ protocolVersion: "2025-11-25", capabilities: {} });
      if (body.method === "notifications/initialized") return new Response(null, { status: 202 });
      calls += 1;
      if (calls === 1) {
        return sse({ input_required: { input_requests: { confirm: { method: "elicitation/create", params: {} } } } });
      }
      return sse({ content: [{ type: "text", text: JSON.stringify({ ok: true }) }] });
    });
    const cap = capture();
    const code = await call(["do_risky_thing"], {
      portText: String(port),
      inputText: '{"confirm":{"action":"accept"}}',
    });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(calls).toBe(2);
    expect(JSON.parse(cap.stdout())).toEqual({ ok: true });
  });

  test("call --progress 写 stderr，stdout 保持 JSON", async () => {
    const port = start((body) => {
      if (body.method === "initialize") return sse({ protocolVersion: "2025-11-25", capabilities: {} });
      if (body.method === "notifications/initialized") return new Response(null, { status: 202 });
      return sseRaw([
        {
          jsonrpc: "2.0",
          method: "notifications/progress",
          params: { progressToken: "p-1", progress: 2, total: 5, message: "step" },
        },
        {
          jsonrpc: "2.0",
          id: 2,
          result: { content: [{ type: "text", text: JSON.stringify({ ok: true }) }] },
        },
      ]);
    });
    const cap = capture();
    const code = await call(["batch_execute"], {
      portText: String(port),
      progress: true,
      argsJson: '{"operations":[]}',
    });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(cap.stderr()).toContain("progress 2/5 step");
    expect(JSON.parse(cap.stdout())).toEqual({ ok: true });
  });
});
