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
import { startNativeServer, sendHandshake, type NativeServer } from "./support/native-ipc";

const servers: NativeServer[] = [];

afterEach(async () => {
  while (servers.length > 0) {
    await servers.pop()!.close();
  }
});

interface CapturedRequest {
  method: string;
  params: unknown;
  id: number;
}

function envelope(value: Record<string, unknown>): Record<string, unknown> {
  return value;
}

async function recordingServer(
  handler: (req: CapturedRequest) => Record<string, unknown>,
): Promise<{ port: number; requests: CapturedRequest[] }> {
  const requests: CapturedRequest[] = [];
  const server = await startNativeServer((message, _socket, send) => {
    if (message.method === "initialize") {
      sendHandshake(message, send);
      return;
    }
    if (message.method === "notifications/initialized") return;
    const params = message.params as Record<string, unknown> | undefined;
    if (params !== undefined && typeof params === "object" && params !== null) {
      const rest: Record<string, unknown> = { ...params };
      delete rest._meta;
      requests.push({ method: String(message.method), params: rest, id: Number(message.id) });
    } else {
      requests.push({ method: String(message.method), params: message.params ?? null, id: Number(message.id) });
    }
    const response = handler(requests[requests.length - 1]);
    send({ ...response, id: message.id });
  });
  servers.push(server);
  return { port: server.port, requests };
}

function okEnvelope(content: unknown[]): Record<string, unknown> {
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
  test("ping 直发 tools/call name=ping，由服务端别名兜底解析", async () => {
    const { port, requests } = await recordingServer(() => envelope(okEnvelope([textContent('{"ok":true}')])));
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const toolCall = requests.find((request) => request.method === "tools/call");
    expect(toolCall?.method).toBe("tools/call");
    expect(toolCall?.params).toEqual({ name: "ping", arguments: {} });
    expect(JSON.parse(cap.stdout()).data).toEqual({ ok: true });
  });

  test("status 经 call_tool 包装调 system_status", async () => {
    const { port, requests } = await recordingServer(() => envelope(okEnvelope([textContent('{"status":"ready"}')])));
    const cap = captureOutput();
    const code = await status([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const toolCall = requests.find((request) => request.method === "tools/call");
    expect(toolCall?.params).toEqual({ name: "call_tool", arguments: { name: "system_status", arguments: {} } });
    const statusJson = JSON.parse(cap.stdout()) as {
      schema_version: number;
      ok: boolean;
      command: string;
      data: { port: number; server: Record<string, unknown> };
    };
    expect(statusJson.schema_version).toBe(1);
    expect(statusJson.ok).toBe(true);
    expect(statusJson.command).toBe("editor.status");
    expect(statusJson.data.port).toBe(port);
    expect(statusJson.data.server).toEqual({ status: "ready" });
  });

  test("categories 直达 list_categories", async () => {
    const { port, requests } = await recordingServer(() => envelope(okEnvelope([textContent('{"categories":["scene"]}')])));
    const cap = captureOutput();
    const code = await categories([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({ name: "list_categories", arguments: {} });
  });

  test("search 直达 search_tools，args 含 query/category/tags", async () => {
    const { port, requests } = await recordingServer(() => envelope(okEnvelope([textContent('{"results":[]}')])));
    const cap = captureOutput();
    const code = await search(["spawn"], { portText: String(port), category: "scene", tagsText: "a,,b" });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({
      name: "search_tools",
      arguments: { query: "spawn", category: "scene", tags: ["a", "b"] },
    });
  });

  test("search 无 query 时改用 list_categories", async () => {
    const { port, requests } = await recordingServer(() =>
      envelope(okEnvelope([textContent('{"categories":[{"id":"Scene"}]}')])),
    );
    const cap = captureOutput();
    const code = await search([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({ name: "list_categories", arguments: {} });
    expect(JSON.parse(cap.stdout()).data).toEqual({ categories: [{ id: "Scene" }] });
  });

  test("call 忽略已删除的 --direct，仍经 call_tool 包装", async () => {
    const { port, requests } = await recordingServer(() => envelope(okEnvelope([textContent("{}")])));
    const cap = captureOutput();
    const code = await call(["delete_scene_node"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const toolCall = requests.find((request) => request.method === "tools/call");
    expect(toolCall?.params).toEqual({
      name: "call_tool",
      arguments: { name: "delete_scene_node", arguments: {} },
    });
  });

  test("describe 直达 get_tool_detail", async () => {
    const { port, requests } = await recordingServer(() => envelope(okEnvelope([textContent('{"name":"move_agent"}')])));
    const cap = captureOutput();
    const code = await runDescribe(["move_agent"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({ name: "get_tool_detail", arguments: { name: "move_agent" } });
  });

  test("call 元工具直达", async () => {
    const { port, requests } = await recordingServer(() => envelope(okEnvelope([textContent("{}")])));
    const cap = captureOutput();
    const code = await call(["ping"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const toolCall = requests.find((request) => request.method === "tools/call");
    expect(toolCall?.params).toEqual({ name: "ping", arguments: {} });
  });

  test("call 领域工具经 call_tool 包装", async () => {
    const { port, requests } = await recordingServer(() => envelope(okEnvelope([textContent("{}")])));
    const cap = captureOutput();
    const code = await call(["move_agent"], { portText: String(port), argsJson: '{"x":1}' });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const toolCall = requests.find((request) => request.method === "tools/call");
    expect(toolCall?.params).toEqual({ name: "call_tool", arguments: { name: "move_agent", arguments: { x: 1 } } });
  });

  test("call --confirm 将 confirm 写入工具参数", async () => {
    const { port, requests } = await recordingServer(() => envelope(okEnvelope([textContent("{}")])));
    const cap = captureOutput();
    const code = await call(["delete_scene_node"], { portText: String(port), confirm: true });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const toolCall = requests.find((request) => request.method === "tools/call");
    expect(toolCall?.params).toEqual({
      name: "call_tool",
      arguments: { name: "delete_scene_node", arguments: { confirm: true } },
    });
  });

  test("call 默认 args 为 {}", async () => {
    const { port, requests } = await recordingServer(() => envelope(okEnvelope([textContent("{}")])));
    const cap = captureOutput();
    const code = await call(["move_agent"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const toolCall = requests.find((request) => request.method === "tools/call");
    expect(toolCall?.params).toEqual({ name: "call_tool", arguments: { name: "move_agent", arguments: {} } });
  });

  test("call --args-file 读取文件作为参数", async () => {
    const file = path.join(os.tmpdir(), `gda-args-${process.pid}-${Date.now()}.json`);
    fs.writeFileSync(file, JSON.stringify({ x: 2 }));
    const { port, requests } = await recordingServer(() => envelope(okEnvelope([textContent("{}")])));
    const cap = captureOutput();
    const code = await call(["move_agent"], { portText: String(port), argsFile: file });
    cap.restore();
    fs.rmSync(file, { force: true });
    expect(code).toBe(ExitCode.Success);
    const toolCall = requests.find((request) => request.method === "tools/call");
    expect(toolCall?.params).toEqual({ name: "call_tool", arguments: { name: "move_agent", arguments: { x: 2 } } });
  });
});

describe("载荷提取", () => {
  test("多 text 载荷输出数组", async () => {
    const { port } = await recordingServer(() => envelope(okEnvelope([textContent('{"a":1}'), textContent('{"b":2}')])));
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(JSON.parse(cap.stdout()).data).toEqual([{ a: 1 }, { b: 2 }]);
  });

  test("仅 image 载荷输出空对象并合并图片报告", async () => {
    const png = Buffer.from("89504e470d0a1a0a", "hex");
    const { port } = await recordingServer(() =>
      envelope(okEnvelope([{ type: "image", mimeType: "image/png", data: png.toString("base64") }])),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const display = JSON.parse(cap.stdout()).data;
    expect(display.images).toEqual([{ mimeType: "image/png", bytes: 8, saved: false }]);
  });

  test("text 与 image 合并到同一对象", async () => {
    const png = Buffer.from("89504e470d0a1a0a", "hex");
    const { port } = await recordingServer(() =>
      envelope(okEnvelope([textContent('{"ok":true}'), { type: "image", mimeType: "image/png", data: png.toString("base64") }])),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const display = JSON.parse(cap.stdout()).data;
    expect(display.ok).toBe(true);
    expect(display.images).toEqual([{ mimeType: "image/png", bytes: 8, saved: false }]);
  });

  test("--save-images 落盘 tempfile 目录", async () => {
    const png = Buffer.from("89504e470d0a1a0a", "hex");
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-test-"));
    const { port } = await recordingServer(() =>
      envelope(okEnvelope([{ type: "image", mimeType: "image/png", data: png.toString("base64") }])),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port), saveImages: dir } as GlobalOptions);
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const display = JSON.parse(cap.stdout()).data;
    expect(display.images[0].saved).toBe(true);
    expect(display.images[0].path).toBe(path.join(dir, path.basename(display.images[0].path!)));
    expect(fs.existsSync(display.images[0].path!)).toBe(true);
    expect(fs.readFileSync(display.images[0].path!)).toEqual(png);
    fs.rmSync(dir, { recursive: true, force: true });
  });

  test("--pretty 缩进 2 空格", async () => {
    const { port } = await recordingServer(() => okEnvelope([textContent('{"a":1}')]));
    const cap = captureOutput();
    const code = await ping([], { portText: String(port), pretty: true });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const envelope = JSON.parse(cap.stdout());
    expect(cap.stdout()).toBe(JSON.stringify(envelope, null, 2) + "\n");
    expect(envelope.data).toEqual({ a: 1 });
  });

  test("--raw 输出脱敏信封", async () => {
    const png = Buffer.from("89504e470d0a1a0a", "hex");
    const { port } = await recordingServer(() =>
      envelope({
        jsonrpc: "2.0",
        id: 42,
        result: { content: [textContent('{"ok":true}'), { type: "image", mimeType: "image/png", data: png.toString("base64") }] },
      }),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port), raw: true });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    const display = JSON.parse(cap.stdout()).data;
    expect(typeof display.id).toBe("number");
    expect(display.result.content[1].data).toBe("<redacted 8 bytes>");
    expect(cap.stdout()).not.toContain(png.toString("base64"));
  });

  test("text 非 JSON 报传输错误", async () => {
    const { port } = await recordingServer(() => envelope(okEnvelope([textContent("not-json")])));
    const e = await catchCliError(ping([], { portText: String(port) }));
    expect(e.code).toBe(ExitCode.Transport);
    expect(e.message).toContain("not valid JSON");
  });

  test("content 为空报传输错误", async () => {
    const { port } = await recordingServer(() => envelope(okEnvelope([])));
    const e = await catchCliError(ping([], { portText: String(port) }));
    expect(e.code).toBe(ExitCode.Transport);
    expect(e.message).toContain("no result.content");
  });
});

describe("search 投影", () => {
  test("query 输出 results 并只保留 name/score", async () => {
    const { port } = await recordingServer(() =>
      envelope(
        okEnvelope([
          textContent(
            JSON.stringify({
              results: [
                { name: "move_agent", score: 0.9, extra: 1 },
                { name: "ping", score: 0.1 },
              ],
            }),
          ),
        ]),
      ),
    );
    const cap = captureOutput();
    const code = await search(["spawn"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(JSON.parse(cap.stdout()).data).toEqual({ results: [{ name: "move_agent", score: 0.9 }] });
  });
});

describe("toolFailed", () => {
  test("result.isError 导致 exit 1", async () => {
    const { port } = await recordingServer(() =>
      envelope({ jsonrpc: "2.0", id: 1, result: { isError: true, content: [textContent('{"error":"boom"}')] } }),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
    expect(cap.stderr()).toContain("tool-level failure");
  });

  test("载荷顶层含 error 键导致 exit 1", async () => {
    const { port } = await recordingServer(() => envelope(okEnvelope([textContent('{"error":"boom"}')])));
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
    expect(cap.stderr()).toContain("tool-level failure");
  });

  test("载荷内层 result 含 error 键导致 exit 1", async () => {
    const { port } = await recordingServer(() => envelope(okEnvelope([textContent('{"result":{"error":"x"}}')])));
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
  });

  test("results 数组项 status=error 导致 exit 1", async () => {
    const { port } = await recordingServer(() => envelope(okEnvelope([textContent('{"results":[{"status":"error"}]}')])));
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
  });

  test("载荷 runtime_error=true 导致 exit 1", async () => {
    const { port } = await recordingServer(() =>
      envelope(
        okEnvelope([textContent('{"result":{"value":null},"runtime_error":true,"error_details":"boom"}')]),
      ),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
    expect(cap.stderr()).toContain("tool-level failure");
  });

  test("载荷 structured_error.code 导致 exit 1", async () => {
    const { port } = await recordingServer(() =>
      envelope(okEnvelope([textContent('{"structured_error":{"code":"when_timeout","timeout_ms":4000}}')])),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
  });

  test("载荷 structured_error 无 code 不判失败", async () => {
    const { port } = await recordingServer(() =>
      envelope(okEnvelope([textContent('{"structured_error":{"action":"retry"},"result":{"value":1}}')])),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
  });

  test("runtime_error 非布尔 true 不判失败", async () => {
    const { port } = await recordingServer(() =>
      envelope(okEnvelope([textContent('{"runtime_error":"true","result":{"value":1}}')])),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
  });

  test("正常载荷不判失败并 exit 0", async () => {
    const { port } = await recordingServer(() =>
      envelope(okEnvelope([textContent('{"result":{"value":1}}')])),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
  });

  test("JSON-RPC 顶层 error 信封导致 exit 1 并透出服务端消息", async () => {
    const { port } = await recordingServer(() =>
      envelope({ jsonrpc: "2.0", id: 1, error: { code: -32602, message: "mock top-level error" } }),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
    expect(cap.stderr()).toContain("mock top-level error");
    expect(cap.stderr()).toContain("-32602");
    const parsed = JSON.parse(cap.stdout()) as {
      ok: boolean;
      data: unknown;
      error: { code: string; message: string; request_sent: boolean };
    };
    expect(parsed.ok).toBe(false);
    expect(parsed.data).toBeNull();
    expect(parsed.error.code).toBe("TOOL_FAILURE");
    expect(parsed.error.request_sent).toBe(false);
  });

  test("JSON-RPC 顶层 error 信封在 --raw 下 exit 1，服务端消息保留在 stderr（失败时 data 为空）", async () => {
    const { port } = await recordingServer(() =>
      envelope({ jsonrpc: "2.0", id: 7, error: { code: -32602, message: "mock top-level error" } }),
    );
    const cap = captureOutput();
    const code = await ping([], { portText: String(port), raw: true });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
    const parsed = JSON.parse(cap.stdout()) as {
      ok: boolean;
      data: unknown;
      error: { code: string };
    };
    expect(parsed.ok).toBe(false);
    expect(parsed.data).toBeNull();
    expect(parsed.error.code).toBe("TOOL_FAILURE");
    expect(cap.stderr()).toContain("mock top-level error");
    expect(cap.stderr()).toContain("-32602");
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

  test("call --args 非法 JSON 诊断含字节数摘要与 --args-file 提示", async () => {
    const payload = '{"a":';
    const e = await catchCliError(call(["ping"], { argsJson: payload }));
    expect(e.code).toBe(ExitCode.Usage);
    expect(e.message).toContain(`received ${Buffer.byteLength(payload, "utf8")} bytes`);
    expect(e.message).toMatch(/sha256:[0-9a-f]{8}/);
    expect(e.message).toContain("head:");
    expect(e.message).toContain("--args-file");
  });

  test("call --args-file 非法 JSON 诊断含字节数与 --args-file 提示", async () => {
    const file = path.join(os.tmpdir(), `gda-args-bad-${process.pid}-${Date.now()}.json`);
    fs.writeFileSync(file, '{"a":');
    const e = await catchCliError(call(["ping"], { argsFile: file }));
    fs.rmSync(file, { force: true });
    expect(e.code).toBe(ExitCode.Usage);
    expect(e.message).toContain("received 5 bytes");
    expect(e.message).toContain("--args-file PATH");
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
    const server = await startNativeServer(() => {});
    const port = server.port;
    await server.close();
    const e = await catchCliError(ping([], { portText: String(port) }));
    expect(e.code).toBe(ExitCode.Transport);
    expect(e.suggestion).toBeDefined();
    expect(e.suggestion).toContain("--port");
  });

  test("GODOT_AUTOPILOT_PORT 指向 stub", async () => {
    const { port } = await recordingServer(() => envelope(okEnvelope([textContent('{"ok":true}')])));
    const old = process.env.GODOT_AUTOPILOT_PORT;
    process.env.GODOT_AUTOPILOT_PORT = String(port);
    try {
      const cap = captureOutput();
      const code = await ping([], {});
      cap.restore();
      expect(code).toBe(ExitCode.Success);
      expect(JSON.parse(cap.stdout()).data).toEqual({ ok: true });
    } finally {
      if (old === undefined) delete process.env.GODOT_AUTOPILOT_PORT;
      else process.env.GODOT_AUTOPILOT_PORT = old;
    }
  });
});