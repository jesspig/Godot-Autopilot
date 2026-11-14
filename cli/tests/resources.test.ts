import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { afterEach, describe, expect, test } from "bun:test";

import { run as runPrompts } from "../src/commands/prompts";
import { run as runResources } from "../src/commands/resources";
import { CliError, ExitCode } from "../src/errors";

const servers: ReturnType<typeof Bun.serve>[] = [];

afterEach(() => {
  while (servers.length > 0) {
    servers.pop()!.stop();
  }
});

function serve(handler: (req: Request) => Response | Promise<Response>): number {
  const server = Bun.serve({ port: 0, fetch: handler });
  servers.push(server);
  return server.port!;
}

function sse(result: unknown): Response {
  return new Response(`event: message\ndata: ${JSON.stringify({ jsonrpc: "2.0", id: 1, result })}\n\n`, {
    headers: { "content-type": "text/event-stream" },
  });
}

function sseError(code: number, message: string): Response {
  return new Response(`event: message\ndata: ${JSON.stringify({ jsonrpc: "2.0", id: 1, error: { code, message } })}\n\n`, {
    headers: { "content-type": "text/event-stream" },
  });
}

function recordingServer(handler: (method: string, params: unknown) => Response | Promise<Response>) {
  const requests: Array<{ method: string; params: unknown }> = [];
  const port = serve(async (req) => {
    const body = (await req.json()) as { method: string; params?: unknown };
    if (body.method === "initialize") {
      return sse({ protocolVersion: "2025-11-25", capabilities: {} });
    }
    if (body.method === "notifications/initialized") {
      return new Response(null, { status: 202 });
    }
    requests.push({ method: body.method, params: body.params });
    return handler(body.method, body.params);
  });
  return { port, requests };
}

function captureOutput(): { stdout(): string; restore(): void } {
  const origOut = process.stdout.write;
  let outBuf = "";
  process.stdout.write = ((chunk: unknown) => {
    outBuf += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  return {
    stdout: () => outBuf,
    restore: () => {
      process.stdout.write = origOut;
    },
  };
}

async function captured(fn: () => Promise<number>): Promise<{ code: number; stdout: string }> {
  const cap = captureOutput();
  try {
    const code = await fn();
    return { code, stdout: cap.stdout() };
  } finally {
    cap.restore();
  }
}

describe("gda resources", () => {
  test("list --templates 请求两个方法", async () => {
    const { port, requests } = recordingServer((method) =>
      method === "resources/list"
        ? sse({ resources: [{ uri: "godot://editor/output-log", name: "editor" }] })
        : sse({ resourceTemplates: [{ uriTemplate: "godot://scene/{path}", name: "scene" }] }),
    );
    const result = await captured(() =>
      runResources(["list"], { portText: String(port), templates: true }),
    );
    expect(result.code).toBe(ExitCode.Success);
    expect(requests.map((request) => request.method)).toEqual([
      "resources/list",
      "resources/templates/list",
    ]);
    expect(JSON.parse(result.stdout)).toEqual({
      resources: [{ uri: "godot://editor/output-log", name: "editor" }],
      templates: [{ uriTemplate: "godot://scene/{path}", name: "scene" }],
    });
  });

  test("read text 直出", async () => {
    const { port, requests } = recordingServer(() =>
      sse({ contents: [{ uri: "godot://debugger/output", mimeType: "text/plain", text: "line one" }] }),
    );
    const result = await captured(() =>
      runResources(["read", "godot://debugger/output"], { portText: String(port) }),
    );
    expect(result.code).toBe(ExitCode.Success);
    expect(result.stdout).toBe("line one\n");
    expect(requests[0].params).toEqual({ uri: "godot://debugger/output" });
  });

  test("read json 美化输出", async () => {
    const { port } = recordingServer(() =>
      sse({ contents: [{ uri: "godot://debugger/errors", mimeType: "application/json", text: '{"a":1}' }] }),
    );
    const result = await captured(() =>
      runResources(["read", "godot://debugger/errors"], { portText: String(port) }),
    );
    expect(result.stdout).toBe('{\n  "a": 1\n}\n');
  });

  test("blob 默认只打印摘要，--save 落盘", async () => {
    const tmp = fs.mkdtempSync(path.join(process.env.GDA_TEST_TMPDIR ?? os.tmpdir(), "gda-res-"));
    const { port } = recordingServer(() =>
      sse({
        contents: [
          {
            uri: "godot://capture/frame.png",
            mimeType: "image/png",
            blob: Buffer.from("hello").toString("base64"),
          },
        ],
      }),
    );
    const summary = await captured(() =>
      runResources(["read", "godot://capture/frame.png"], { portText: String(port) }),
    );
    expect(summary.stdout).toBe("<blob 5 bytes, image/png>\n");

    const saved = await captured(() =>
      runResources(["read", "godot://capture/frame.png"], { portText: String(port), saveText: tmp }),
    );
    const payload = JSON.parse(saved.stdout) as { saved: Array<{ path: string; bytes: number }> };
    expect(payload.saved[0].bytes).toBe(5);
    expect(fs.readFileSync(payload.saved[0].path, "utf8")).toBe("hello");
    fs.rmSync(tmp, { recursive: true, force: true });
  });

  test("未知 URI 返回工具失败", async () => {
    const { port } = recordingServer(() => sseError(-32602, "resource not found"));
    let caught: unknown;
    try {
      await runResources(["read", "godot://nope"], { portText: String(port) });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.ToolFailure);
  });
});

describe("gda prompts", () => {
  test("list 输出 prompts", async () => {
    const { port } = recordingServer(() =>
      sse({ prompts: [{ name: "tool-usage", description: "usage" }] }),
    );
    const result = await captured(() => runPrompts(["list"], { portText: String(port) }));
    expect(JSON.parse(result.stdout)).toEqual({ prompts: [{ name: "tool-usage", description: "usage" }] });
  });

  test("get 按 role 渲染并转换参数", async () => {
    const { port, requests } = recordingServer(() =>
      sse({
        messages: [
          { role: "user", content: { type: "text", text: "hello" } },
          { role: "assistant", content: { type: "text", text: "world" } },
        ],
      }),
    );
    const result = await captured(() =>
      runPrompts(["get", "debug-analyze-error"], {
        portText: String(port),
        argsJson: '{"since":0,"verbose":true}',
      }),
    );
    expect(result.stdout).toBe("[user] hello\n\n[assistant] world\n");
    expect(requests[0].params).toEqual({
      name: "debug-analyze-error",
      arguments: { since: "0", verbose: "true" },
    });
  });

  test("get 未知 prompt 返回工具失败", async () => {
    const { port } = recordingServer(() => sseError(-32602, "prompt not found"));
    let caught: unknown;
    try {
      await runPrompts(["get", "nope"], { portText: String(port) });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.ToolFailure);
  });
});
