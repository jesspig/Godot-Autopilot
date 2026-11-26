import { afterEach, describe, expect, test } from "bun:test";

import type { GlobalOptions } from "../src/args";
import { run as call } from "../src/commands/call";
import { run as status } from "../src/commands/status";
import { ConfirmationRequiredError, ExitCode } from "../src/errors";

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
  test("status 输出统一 envelope（MCP 专属字段已删除）", async () => {
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
    const envelope = JSON.parse(cap.stdout()) as {
      schema_version: number;
      ok: boolean;
      command: string;
      data: { port: number; server: Record<string, unknown> };
    };
    expect(envelope.schema_version).toBe(1);
    expect(envelope.ok).toBe(true);
    expect(envelope.command).toBe("editor.status");
    expect(envelope.data.port).toBe(port);
    expect(envelope.data.server).toEqual({ version: "0.3.3", tool_count: 459 });
    // protocol_version / capabilities / server_info 等 MCP 专属字段已随 T-11 删除
  });

  test("input_required 抛 ConfirmationRequiredError（exit 4，提示 --confirm）且不重发", async () => {
    let calls = 0;
    const port = start((body) => {
      if (body.method === "initialize") return sse({ protocolVersion: "2025-11-25", capabilities: {} });
      if (body.method === "notifications/initialized") return new Response(null, { status: 202 });
      calls += 1;
      return sse({ input_required: { input_requests: { confirm: { method: "elicitation/create", params: {} } } } });
    });
    const cap = capture();
    let caught: unknown;
    try {
      await call(["do_risky_thing"], { portText: String(port) });
    } catch (error) {
      caught = error;
    } finally {
      cap.restore();
    }
    expect(caught).toBeInstanceOf(ConfirmationRequiredError);
    const failure = caught as ConfirmationRequiredError;
    expect(failure.code).toBe(ExitCode.ConfirmationRequired);
    expect(failure.suggestion).toContain("--confirm");
    expect(calls).toBe(1);
    expect(cap.stdout()).toBe("");
  });

  test("input_required 下旧 --input 不再触发重试（calls 保持 1）", async () => {
    let calls = 0;
    const port = start((body) => {
      if (body.method === "initialize") return sse({ protocolVersion: "2025-11-25", capabilities: {} });
      if (body.method === "notifications/initialized") return new Response(null, { status: 202 });
      calls += 1;
      return sse({ input_required: { input_requests: { confirm: { method: "elicitation/create", params: {} } } } });
    });
    const cap = capture();
    let caught: unknown;
    try {
      await call(["do_risky_thing"], {
        portText: String(port),
        inputText: '{"confirm":{"action":"accept"}}',
      } as unknown as GlobalOptions);
    } catch (error) {
      caught = error;
    } finally {
      cap.restore();
    }
    expect(caught).toBeInstanceOf(ConfirmationRequiredError);
    expect((caught as ConfirmationRequiredError).code).toBe(ExitCode.ConfirmationRequired);
    expect(calls).toBe(1);
    expect(cap.stdout()).toBe("");
  });

  test("input_required 带 --confirm 仍不重发，确认摘要写入 stderr", async () => {
    let calls = 0;
    const port = start((body) => {
      if (body.method === "initialize") return sse({ protocolVersion: "2025-11-25", capabilities: {} });
      if (body.method === "notifications/initialized") return new Response(null, { status: 202 });
      calls += 1;
      return sse({ input_required: { input_requests: { confirm: { method: "elicitation/create", params: {} } } } });
    });
    const cap = capture();
    let caught: unknown;
    try {
      await call(["do_risky_thing"], { portText: String(port), confirm: true });
    } catch (error) {
      caught = error;
    } finally {
      cap.restore();
    }
    expect(caught).toBeInstanceOf(ConfirmationRequiredError);
    expect((caught as ConfirmationRequiredError).code).toBe(ExitCode.ConfirmationRequired);
    expect(calls).toBe(1);
    expect(cap.stderr()).toContain("confirm tool.call");
    expect(cap.stdout()).toBe("");
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
    expect(JSON.parse(cap.stdout()).data).toEqual({ ok: true });
  });
});
