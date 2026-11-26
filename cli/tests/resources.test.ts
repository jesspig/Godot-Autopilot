import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import { afterEach, beforeEach, describe, expect, test } from "bun:test";

import { run as runPrompts } from "../src/commands/prompts";
import { run as runResources } from "../src/commands/resources";
import { parseArgv, type GlobalOptions } from "../src/args";
import { CliError, ExitCode } from "../src/errors";
import { pluginIndexPath } from "../src/instances";

const servers: ReturnType<typeof Bun.serve>[] = [];

afterEach(() => {
  while (servers.length > 0) {
    servers.pop()!.stop();
  }
  if (previousHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = previousHome;
  if (tmpHome !== undefined) fs.rmSync(tmpHome, { recursive: true, force: true });
});

let tmpHome: string | undefined;
let previousHome: string | undefined;

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmpHome = fs.mkdtempSync(path.join(base, "gda-resources-ipc-"));
  previousHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmpHome;
});

function writeIndexWithIpc(httpPort: number, ipcPort: number, token: string): void {
  const file = pluginIndexPath();
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(
    file,
    JSON.stringify([
      {
        instance_id: "gda-res-ipc",
        port: httpPort,
        pid: process.pid,
        project_path: process.cwd(),
        api_token: "token",
        endpoints: {
          mcp: { host: "127.0.0.1", port: httpPort, path: "/mcp", protocol: "mcp", auth: "none" },
          ipc: {
            kind: "tcp",
            host: "127.0.0.1",
            port: ipcPort,
            token,
            protocol: "mcp-jsonrpc",
            framing: "length-prefixed-json",
            auth: "token",
            version: 1,
          },
        },
        gda_home: tmpHome,
        started_at: Date.now(),
        version: "0.3.4",
      },
    ]),
    "utf8",
  );
}

function frame(value: unknown): Buffer {
  const payload = Buffer.from(JSON.stringify(value), "utf8");
  const header = Buffer.alloc(4);
  header.writeUInt32BE(payload.length, 0);
  return Buffer.concat([header, payload]);
}

function startIpcServer(
  onRequest: (message: Record<string, unknown>) => Record<string, unknown>,
): Promise<{ server: net.Server; port: number }> {
  return new Promise((resolve, reject) => {
    const server = net.createServer((socket) => {
      let buffer = Buffer.alloc(0);
      socket.on("data", (chunk: Buffer) => {
        buffer = Buffer.concat([buffer, chunk]);
        while (buffer.length >= 4) {
          const size = buffer.readUInt32BE(0);
          if (buffer.length < 4 + size) break;
          const text = buffer.subarray(4, 4 + size).toString("utf8");
          buffer = buffer.subarray(4 + size);
          const message = JSON.parse(text) as Record<string, unknown>;
          socket.write(frame(onRequest(message)));
        }
      });
    });
    server.listen(0, "127.0.0.1", () => {
      const address = server.address();
      const port = typeof address === "object" && address !== null ? address.port : 0;
      resolve({ server, port });
    });
    server.once("error", reject);
  });
}

function closeIpcServer(server: net.Server): Promise<void> {
  return new Promise((resolve) => server.close(() => resolve()));
}

async function rejectWith(fn: () => Promise<number>): Promise<CliError> {
  try {
    await fn();
  } catch (error) {
    if (error instanceof CliError) return error;
    throw error;
  }
  throw new Error("expected CliError but the command resolved");
}

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
    expect((JSON.parse(result.stdout) as { data: unknown }).data).toEqual({
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
    const payload = (JSON.parse(saved.stdout) as { data: { saved: Array<{ path: string; bytes: number }> } }).data;
    expect(payload.saved[0].bytes).toBe(5);
    expect(fs.readFileSync(payload.saved[0].path, "utf8")).toBe("hello");
    fs.rmSync(tmp, { recursive: true, force: true });
  });

  test("已删除的 --transport 被解析为 usage 错误", () => {
    let caught: unknown;
    try {
      parseArgv(["--transport", "bogus", "resources", "list"]);
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Usage);
    expect((caught as CliError).message).toContain("--transport");
  });

  test("IPC 鉴权失败不静默降级到无认证 HTTP", async () => {
    const httpPort = serve(() => new Response("should not be reached", { status: 500 }));
    const seen: string[] = [];
    const presentedTokens: unknown[] = [];
    const { server, port: ipcPort } = await startIpcServer((message) => {
      seen.push(String(message.method));
      if (message.method === "initialize") {
        const meta = (message.params as Record<string, unknown> | undefined)?._meta as Record<string, unknown> | undefined;
        presentedTokens.push(meta?.gda_token);
        return { jsonrpc: "2.0", id: message.id, error: { code: -32001, message: "IPC token rejected" } };
      }
      return { jsonrpc: "2.0", id: message.id, result: { resources: [] } };
    });
    writeIndexWithIpc(httpPort, ipcPort, "correct-token");
    try {
      const error = await rejectWith(() =>
        runResources(["list"], { transportText: "ipc", noFallback: false } as unknown as GlobalOptions),
      );
      expect(error.code).toBe(ExitCode.Transport);
      expect(error.message).toContain("IPC authentication failed");
      expect(error.suggestion).toContain("Refusing to fall back");
      expect(presentedTokens).toEqual(["correct-token"]);
      expect(seen).toEqual(["initialize"]);
    } finally {
      await closeIpcServer(server);
    }
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
    expect((JSON.parse(result.stdout) as { data: unknown }).data).toEqual({ prompts: [{ name: "tool-usage", description: "usage" }] });
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

  test("IPC 鉴权失败不静默降级到无认证 HTTP", async () => {
    const httpPort = serve(() => new Response("should not be reached", { status: 500 }));
    const seen: string[] = [];
    const presentedTokens: unknown[] = [];
    const { server, port: ipcPort } = await startIpcServer((message) => {
      seen.push(String(message.method));
      if (message.method === "initialize") {
        const meta = (message.params as Record<string, unknown> | undefined)?._meta as Record<string, unknown> | undefined;
        presentedTokens.push(meta?.gda_token);
        return { jsonrpc: "2.0", id: message.id, error: { code: -32002, message: "IPC initialize required" } };
      }
      return { jsonrpc: "2.0", id: message.id, result: { prompts: [] } };
    });
    writeIndexWithIpc(httpPort, ipcPort, "correct-token");
    try {
      const error = await rejectWith(() =>
        runPrompts(["list"], { transportText: "ipc", noFallback: false } as unknown as GlobalOptions),
      );
      expect(error.code).toBe(ExitCode.Transport);
      expect(error.message).toContain("IPC authentication failed");
      expect(error.suggestion).toContain("Refusing to fall back");
      expect(presentedTokens).toEqual(["correct-token"]);
      expect(seen).toEqual(["initialize"]);
    } finally {
      await closeIpcServer(server);
    }
  });
});
