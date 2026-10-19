import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { afterEach, describe, expect, test } from "bun:test";
import { run as ping } from "../src/commands/ping";
import { run as status } from "../src/commands/status";
import { run as categories } from "../src/commands/categories";
import { run as search } from "../src/commands/search";
import { run as runDescribe } from "../src/commands/describe";
import { run as call } from "../src/commands/call";
import { CliError, ExitCode } from "../src/errors";
import type { GlobalOptions } from "../src/args";

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

interface CapturedRequest {
  method: string;
  params: unknown;
  id: number;
}

function recordingServer(handler: (req: CapturedRequest) => Response | Promise<Response>) {
  const requests: CapturedRequest[] = [];
  const port = serve(async (req) => {
    const body = (await req.json()) as CapturedRequest;
    requests.push(body);
    return handler(body);
  });
  return { port, requests };
}

function okEnvelope(content: unknown[]): unknown {
  return { jsonrpc: "2.0", id: 1, result: { content } };
}

function textContent(text: string): unknown {
  return { type: "text", text };
}

function captureOutput(): { stdout(): string; stderr(): string; restore(): void } {
  const origOut = process.stdout.write;
  const origErr = process.stderr.write;
  let outBuf = "";
  let errBuf = "";
  process.stdout.write = ((chunk: unknown) => {
    outBuf += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  process.stderr.write = ((chunk: unknown) => {
    errBuf += String(chunk);
    return true;
  }) as typeof process.stderr.write;
  return {
    stdout: () => outBuf,
    stderr: () => errBuf,
    restore: () => {
      process.stdout.write = origOut;
      process.stderr.write = origErr;
    },
  };
}

async function catchCliError(p: Promise<number>): Promise<CliError> {
  try {
    await p;
  } catch (e) {
    expect(e).toBeInstanceOf(CliError);
    return e as CliError;
  }
  throw new Error("expected rejection but the promise resolved");
}

describe("请求形状", () => {
  test("ping 直达 ping 工具", async () => {
    const { port, requests } = recordingServer(() => sseResponse(okEnvelope([textContent('{"ok":true}')])));
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].method).toBe("tools/call");
    expect(requests[0].params).toEqual({ name: "ping", arguments: {} });
    expect(cap.stdout()).toBe('{"ok":true}\n');
  });

  test("status 经 call_tool 包装调 system_status", async () => {
    const { port, requests } = recordingServer(() => sseResponse(okEnvelope([textContent('{"status":"ready"}')])));
    const cap = captureOutput();
    const code = await status([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({ name: "call_tool", arguments: { name: "system_status", arguments: {} } });
    expect(cap.stdout()).toBe('{"status":"ready"}\n');
  });

  test("categories 直达 list_categories", async () => {
    const { port, requests } = recordingServer(() => sseResponse(okEnvelope([textContent('{"categories":["scene"]}')])));
    const cap = captureOutput();
    const code = await categories([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({ name: "list_categories", arguments: {} });
  });

  test("search 直达 search_tools，args 含 query/category/tags", async () => {
    const { port, requests } = recordingServer(() => sseResponse(okEnvelope([textContent('{"results":[]}')])));
    const cap = captureOutput();
    const code = await search(["spawn"], { portText: String(port), category: "scene", tagsText: "a,,b" });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({
      name: "search_tools",
      arguments: { query: "spawn", category: "scene", tags: ["a", "b"] },
    });
  });

  test("search 无 query 时 args 不含 query 键", async () => {
    const { port, requests } = recordingServer(() => sseResponse(okEnvelope([textContent('{"results":[]}')])));
    const cap = captureOutput();
    const code = await search([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({ name: "search_tools", arguments: {} });
  });

  test("describe 直达 get_tool_detail", async () => {
    const { port, requests } = recordingServer(() => sseResponse(okEnvelope([textContent('{"name":"move_agent"}')])));
    const cap = captureOutput();
    const code = await runDescribe(["move_agent"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({ name: "get_tool_detail", arguments: { name: "move_agent" } });
  });

  test("call 元工具直达", async () => {
    const { port, requests } = recordingServer(() => sseResponse(okEnvelope([textContent("{}")])));
    const cap = captureOutput();
    const code = await call(["ping"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({ name: "ping", arguments: {} });
  });

  test("call 领域工具经 call_tool 包装", async () => {
    const { port, requests } = recordingServer(() => sseResponse(okEnvelope([textContent("{}")])));
    const cap = captureOutput();
    const code = await call(["move_agent"], { portText: String(port), argsJson: '{"x":1}' });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({ name: "call_tool", arguments: { name: "move_agent", arguments: { x: 1 } } });
  });

  test("call 默认 args 为 {}", async () => {
    const { port, requests } = recordingServer(() => sseResponse(okEnvelope([textContent("{}")])));
    const cap = captureOutput();
    const code = await call(["move_agent"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({ name: "call_tool", arguments: { name: "move_agent", arguments: {} } });
  });

  test("call --args-file 读取文件作为参数", async () => {
    const file = path.join(os.tmpdir(), `gda-args-${process.pid}-${Date.now()}.json`);
    fs.writeFileSync(file, JSON.stringify({ x: 2 }));
    const { port, requests } = recordingServer(() => sseResponse(okEnvelope([textContent("{}")])));
    const cap = captureOutput();
    const code = await call(["move_agent"], { portText: String(port), argsFile: file });
    cap.restore();
    fs.rmSync(file, { force: true });
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({ name: "call_tool", arguments: { name: "move_agent", arguments: { x: 2 } } });
  });
});

describe("载荷提取", () => {
  test("多 text 载荷输出数组", async () => {
    const { port } = recordingServer(() => sseResponse(okEnvelope([textContent('{"a":1}'), textContent('{"b":2}')])));
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(cap.stdout()).toBe(JSON.stringify([{ a: 1 }, { b: 2 }]) + "\n");
  });

  test("仅 image 载荷输出空对象并合并图片报告", async () => {
    const png = Buffer.from("89504e470d0a1a0a", "hex");
    const { port } = recordingServer(() =>
      sseResponse(okEnvelope([{ type: "image", mimeType: "image/png", data: png.toString("base64") }])),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const display = JSON.parse(cap.stdout());
    expect(display.images).toEqual([{ mimeType: "image/png", bytes: 8, saved: false }]);
  });

  test("text 与 image 合并到同一对象", async () => {
    const png = Buffer.from("89504e470d0a1a0a", "hex");
    const { port } = recordingServer(() =>
      sseResponse(okEnvelope([textContent('{"ok":true}'), { type: "image", mimeType: "image/png", data: png.toString("base64") }])),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const display = JSON.parse(cap.stdout());
    expect(display.ok).toBe(true);
    expect(display.images).toEqual([{ mimeType: "image/png", bytes: 8, saved: false }]);
  });

  test("--save-images 落盘 tempfile 目录", async () => {
    const png = Buffer.from("89504e470d0a1a0a", "hex");
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-test-"));
    const { port } = recordingServer(() =>
      sseResponse(okEnvelope([{ type: "image", mimeType: "image/png", data: png.toString("base64") }])),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port), saveImages: dir } as GlobalOptions);
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const display = JSON.parse(cap.stdout());
    expect(display.images[0].saved).toBe(true);
    expect(display.images[0].path).toBe(path.join(dir, path.basename(display.images[0].path!)));
    expect(fs.existsSync(display.images[0].path!)).toBe(true);
    expect(fs.readFileSync(display.images[0].path!)).toEqual(png);
    fs.rmSync(dir, { recursive: true, force: true });
  });

  test("--pretty 缩进 2 空格", async () => {
    const { port } = recordingServer(() => sseResponse(okEnvelope([textContent('{"a":1}')])));
    const cap = captureOutput();
    const code = await ping([], { portText: String(port), pretty: true });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(cap.stdout()).toBe(JSON.stringify({ a: 1 }, null, 2) + "\n");
  });

  test("--raw 输出脱敏信封", async () => {
    const png = Buffer.from("89504e470d0a1a0a", "hex");
    const { port } = recordingServer(() =>
      sseResponse({
        jsonrpc: "2.0",
        id: 42,
        result: { content: [textContent('{"ok":true}'), { type: "image", mimeType: "image/png", data: png.toString("base64") }] },
      }),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port), raw: true });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const display = JSON.parse(cap.stdout());
    expect(display.id).toBe(42);
    expect(display.result.content[1].data).toBe("<redacted 8 bytes>");
    expect(cap.stdout()).not.toContain(png.toString("base64"));
  });

  test("text 非 JSON 报传输错误", async () => {
    const { port } = recordingServer(() => sseResponse(okEnvelope([textContent("not-json")])));
    const e = await catchCliError(ping([], { portText: String(port) }));
    expect(e.code).toBe(ExitCode.Transport);
    expect(e.message).toContain("not valid JSON");
  });

  test("content 为空报传输错误", async () => {
    const { port } = recordingServer(() => sseResponse(okEnvelope([])));
    const e = await catchCliError(ping([], { portText: String(port) }));
    expect(e.code).toBe(ExitCode.Transport);
    expect(e.message).toContain("no result.content");
  });
});

describe("search 投影", () => {
  test("从 display.results 取数组", async () => {
    const { port } = recordingServer(() =>
      sseResponse(okEnvelope([textContent(JSON.stringify({ results: [{ name: "move_agent", score: 0.9, extra: 1 }] }))])),
    );
    const cap = captureOutput();
    const code = await search(["spawn"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(cap.stdout()).toBe(JSON.stringify([{ name: "move_agent", score: 0.9, extra: 1 }]) + "\n");
  });

  test("从 display.result.results 取数组", async () => {
    const { port } = recordingServer(() =>
      sseResponse(okEnvelope([textContent(JSON.stringify({ result: { results: [{ name: "a", score: 1 }] } }))])),
    );
    const cap = captureOutput();
    const code = await search([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(cap.stdout()).toBe(JSON.stringify([{ name: "a", score: 1 }]) + "\n");
  });

  test("从 display.data.results 取数组", async () => {
    const { port } = recordingServer(() =>
      sseResponse(okEnvelope([textContent(JSON.stringify({ data: { results: [{ name: "b", score: 2 }] } }))])),
    );
    const cap = captureOutput();
    const code = await search([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(cap.stdout()).toBe(JSON.stringify([{ name: "b", score: 2 }]) + "\n");
  });
});

describe("toolFailed", () => {
  test("result.isError 导致 exit 1", async () => {
    const { port } = recordingServer(() =>
      sseResponse({ jsonrpc: "2.0", id: 1, result: { isError: true, content: [textContent('{"error":"boom"}')] } }),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
    expect(cap.stderr()).toContain("tool-level failure");
  });

  test("载荷顶层含 error 键导致 exit 1", async () => {
    const { port } = recordingServer(() => sseResponse(okEnvelope([textContent('{"error":"boom"}')])));
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
    expect(cap.stderr()).toContain("tool-level failure");
  });

  test("载荷内层 result 含 error 键导致 exit 1", async () => {
    const { port } = recordingServer(() => sseResponse(okEnvelope([textContent('{"result":{"error":"x"}}')])));
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
  });

  test("results 数组项 status=error 导致 exit 1", async () => {
    const { port } = recordingServer(() => sseResponse(okEnvelope([textContent('{"results":[{"status":"error"}]}')])));
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
  });

  test("JSON-RPC 顶层 error 信封导致 exit 1 并透出服务端消息", async () => {
    const { port } = recordingServer(() =>
      sseResponse({ jsonrpc: "2.0", id: 1, error: { code: -32602, message: "mock top-level error" } }),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
    expect(cap.stderr()).toContain("mock top-level error");
    expect(cap.stderr()).toContain("-32602");
    expect(JSON.parse(cap.stdout())).toEqual({ error: { code: -32602, message: "mock top-level error" } });
  });

  test("JSON-RPC 顶层 error 信封在 --raw 下仍输出脱敏信封并 exit 1", async () => {
    const { port } = recordingServer(() =>
      sseResponse({ jsonrpc: "2.0", id: 7, error: { code: -32602, message: "mock top-level error" } }),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port), raw: true });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
    const display = JSON.parse(cap.stdout());
    expect(display.id).toBe(7);
    expect(display.error.code).toBe(-32602);
  });
});

describe("用法错误", () => {
  test("describe 缺参数 exit 3", async () => {
    const e = await catchCliError(runDescribe([], {}));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("describe 多参数 exit 3", async () => {
    const e = await catchCliError(runDescribe(["a", "b"], {}));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("call 缺参数 exit 3", async () => {
    const e = await catchCliError(call([], {}));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("search 多 query exit 3", async () => {
    const e = await catchCliError(search(["a", "b"], {}));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("ping 用 --category exit 3", async () => {
    const e = await catchCliError(ping([], { category: "scene" }));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("ping 用 --tags exit 3", async () => {
    const e = await catchCliError(ping([], { tagsText: "a" }));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("status 用 --args exit 3", async () => {
    const e = await catchCliError(status([], { argsJson: "{}" }));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("call --args 与 --args-file 互斥 exit 3", async () => {
    const e = await catchCliError(call(["ping"], { argsJson: "{}", argsFile: "x.json" }));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("call --args 非法 JSON exit 3", async () => {
    const e = await catchCliError(call(["ping"], { argsJson: "{bad" }));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("call --args 非对象 exit 3", async () => {
    const e = await catchCliError(call(["ping"], { argsJson: "[1,2]" }));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("call --args 为 null exit 3", async () => {
    const e = await catchCliError(call(["ping"], { argsJson: "null" }));
    expect(e.code).toBe(ExitCode.Usage);
  });
});

describe("连接与端口", () => {
  test("连接失败 exit 2 且 stderr 含 suggestion", async () => {
    const server = Bun.serve({ port: 0, fetch: () => new Response("x") });
    const port = server.port;
    server.stop();
    const e = await catchCliError(ping([], { portText: String(port) }));
    expect(e.code).toBe(ExitCode.Transport);
    expect(e.suggestion).toBeDefined();
    expect(e.suggestion).toContain("GODOT_AUTOPILOT_PORT");
  });

  test("GODOT_AUTOPILOT_PORT 指向 stub", async () => {
    const { port } = recordingServer(() => sseResponse(okEnvelope([textContent('{"ok":true}')])));
    const old = process.env.GODOT_AUTOPILOT_PORT;
    process.env.GODOT_AUTOPILOT_PORT = String(port);
    try {
      const cap = captureOutput();
      const code = await ping([], {});
      cap.restore();
      expect(code).toBe(ExitCode.Success);
      expect(cap.stdout()).toBe('{"ok":true}\n');
    } finally {
      if (old === undefined) delete process.env.GODOT_AUTOPILOT_PORT;
      else process.env.GODOT_AUTOPILOT_PORT = old;
    }
  });
});
