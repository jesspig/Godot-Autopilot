import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { parseArgv, usageText } from "../src/args";
import { apiRequest, mapStatusToExitCode } from "../src/api/client";
import { run as apiRun } from "../src/commands/api";
import { COMMANDS } from "../src/commands/index";
import { CliError, ExitCode } from "../src/errors";
import {
  findPluginRecordByPort,
  findPluginRecordByProject,
  readPluginInstanceIndex,
} from "../src/instances";
import { probeEndpoint } from "../src/probe";
import { findById, findLiveByProject, saveRegistry } from "../src/registry";
import { resolveTarget, type TargetContext } from "../src/target";

const servers: Bun.Server[] = [];
const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

let work: string;
let prevHome: string | undefined;
let prevCliHome: string | undefined;
let prevUserData: string | undefined;
let prevPort: string | undefined;

beforeEach(() => {
  work = fs.mkdtempSync(path.join(os.tmpdir(), "gda-api-test-"));
  prevHome = process.env.GODOT_AUTOPILOT_HOME;
  prevCliHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  prevUserData = process.env.GODOT_AUTOPILOT_USERDATA;
  prevPort = process.env.GODOT_AUTOPILOT_PORT;
  process.env.GODOT_AUTOPILOT_HOME = work;
  process.env.GODOT_AUTOPILOT_CLI_HOME = path.join(work, "cli");
  process.env.GODOT_AUTOPILOT_USERDATA = path.join(work, "userdata");
  delete process.env.GODOT_AUTOPILOT_PORT;
});

afterEach(() => {
  while (servers.length > 0) {
    servers.pop()!.stop();
  }
  restoreEnv("GODOT_AUTOPILOT_HOME", prevHome);
  restoreEnv("GODOT_AUTOPILOT_CLI_HOME", prevCliHome);
  restoreEnv("GODOT_AUTOPILOT_USERDATA", prevUserData);
  restoreEnv("GODOT_AUTOPILOT_PORT", prevPort);
  fs.rmSync(work, { recursive: true, force: true });
});

function restoreEnv(key: string, value: string | undefined): void {
  if (value === undefined) {
    delete process.env[key];
  } else {
    process.env[key] = value;
  }
}

function rootDir(): string {
  return path.join(work, ".godot-autopilot");
}

function writeIndex(doc: unknown): void {
  fs.mkdirSync(rootDir(), { recursive: true });
  fs.writeFileSync(path.join(rootDir(), "instances.json"), JSON.stringify(doc, null, 2));
}

function writeRawIndex(text: string): void {
  fs.mkdirSync(rootDir(), { recursive: true });
  fs.writeFileSync(path.join(rootDir(), "instances.json"), text);
}

function pluginRecord(overrides: Record<string, unknown> = {}): Record<string, unknown> {
  const port = typeof overrides["port"] === "number" ? (overrides["port"] as number) : 9600;
  return {
    instance_id: "i1",
    port,
    pid: process.pid,
    project_path: process.cwd(),
    api_token: "token-abc",
    endpoints: {
      mcp: { host: "127.0.0.1", port, path: "/mcp", protocol: "mcp", auth: "none" },
      api: { host: "127.0.0.1", port, path: "/api", protocol: "http", auth: "bearer" },
    },
    gda_home: work,
    started_at: 1,
    version: "0.0.0",
    ...overrides,
  };
}

interface CapturedRequest {
  method: string;
  url: string;
  auth: string | null;
  body: string;
}

function serve(
  handler: (req: Request) => Response | Promise<Response>,
): { port: number; requests: CapturedRequest[] } {
  const requests: CapturedRequest[] = [];
  const server = Bun.serve({
    port: 0,
    fetch: async (req) => {
      requests.push({
        method: req.method,
        url: new URL(req.url).pathname + new URL(req.url).search,
        auth: req.headers.get("authorization"),
        body: await req.text(),
      });
      return handler(req);
    },
  });
  servers.push(server);
  return { port: server.port, requests };
}

function jsonResponse(payload: unknown, status = 200): Response {
  return new Response(JSON.stringify(payload), {
    status,
    headers: { "content-type": "application/json" },
  });
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

describe("命令注册", () => {
  test("api 与 endpoints 已注册", () => {
    expect(Object.keys(COMMANDS)).toContain("api");
    expect(Object.keys(COMMANDS)).toContain("endpoints");
  });

  test("帮助文案含 api 子命令与新选项", () => {
    const text = usageText();
    expect(text).toContain("api list");
    expect(text).toContain("api describe");
    expect(text).toContain("api call");
    expect(text).toContain("endpoints");
    expect(text).toContain("--query");
    expect(text).toContain("--probe");
  });
});

describe("参数解析", () => {
  test("--query 与 --probe 被识别", () => {
    const { opts, positionals } = parseArgv(["--query", "spawn", "--probe", "strict", "api", "list"]);
    expect(opts.query).toBe("spawn");
    expect(opts.probe).toBe("strict");
    expect(positionals).toEqual(["api", "list"]);
  });

  test("--probe 非法值报用法错误", async () => {
    const e = await catchCliError(apiRun(["list"], { probe: "deep" }));
    expect(e.code).toBe(ExitCode.Usage);
    expect(e.message).toContain("--probe");
  });

  test("api 缺子命令报用法错误", async () => {
    const e = await catchCliError(apiRun([], {}));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("api 未知子命令报用法错误", async () => {
    const e = await catchCliError(apiRun(["bogus"], {}));
    expect(e.code).toBe(ExitCode.Usage);
    expect(e.message).toContain("bogus");
  });

  test("api list 位置参数与 --query 互斥", async () => {
    const e = await catchCliError(apiRun(["list", "spawn"], { query: "move" }));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("api describe 多参数报用法错误", async () => {
    const e = await catchCliError(apiRun(["describe", "a", "b"], {}));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("api describe 使用 --args 报用法错误", async () => {
    const e = await catchCliError(apiRun(["describe", "a"], { argsJson: "{}" }));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("api call 使用 --category 报用法错误", async () => {
    const e = await catchCliError(apiRun(["call", "a"], { category: "scene" }));
    expect(e.code).toBe(ExitCode.Usage);
  });
});

describe("发现链", () => {
  test("--port 优先级最高且来源为 cli", () => {
    const t = resolveTarget({ portText: "9527" }, TARGET_CONTEXT);
    expect(t.port).toBe(9527);
    expect(t.source).toBe("cli");
    expect(t.chain).toBe("port");
  });

  test("--id 命中自有注册表条目", () => {
    const ctx: TargetContext = {
      findById: () => ({
        id: "p1",
        project: process.cwd(),
        port: 9611,
        pid: process.pid,
        startedAt: "2026-10-20T00:00:00.000Z",
        headless: false,
        godot: "godot",
        log: "x.log",
      }),
      findLiveByProject: () => undefined,
    };
    const t = resolveTarget({ id: "p1" }, ctx);
    expect(t.port).toBe(9611);
    expect(t.chain).toBe("id");
  });

  test("环境变量优先于登记与默认", () => {
    process.env.GODOT_AUTOPILOT_PORT = "3000";
    const t = resolveTarget({}, TARGET_CONTEXT);
    expect(t.port).toBe(3000);
    expect(t.source).toBe("env");
    expect(t.chain).toBe("env");
  });

  test("CLI 本地注册表命中活跃条目", () => {
    saveRegistry({
      version: 1,
      instances: [
        {
          id: "p1",
          project: process.cwd(),
          port: 9622,
          pid: process.pid,
          startedAt: "2026-10-20T00:00:00.000Z",
          headless: false,
          godot: "godot",
          log: "x.log",
        },
      ],
    });
    const t = resolveTarget({}, TARGET_CONTEXT);
    expect(t.port).toBe(9622);
    expect(t.source).toBe("cwd");
    expect(t.chain).toBe("cli-registry");
  });

  test("插件全局实例索引命中工程路径", () => {
    writeIndex({ instances: [pluginRecord({ port: 9633 })] });
    const t = resolveTarget({}, TARGET_CONTEXT);
    expect(t.port).toBe(9633);
    expect(t.source).toBe("registry");
    expect(t.chain).toBe("plugin-index");
    expect(t.pluginRecord?.instanceId).toBe("i1");
  });

  test("旧路径回退链保留 trace 命中", () => {
    const projectDir = path.join(work, "legacy-proj");
    fs.mkdirSync(projectDir, { recursive: true });
    fs.writeFileSync(path.join(projectDir, "project.godot"), '[application]\nconfig/name = "LegacyProj"\n');
    const traceDir = path.join(work, "userdata", "LegacyProj", "godot_autopilot", "traces");
    fs.mkdirSync(traceDir, { recursive: true });
    fs.writeFileSync(
      path.join(traceDir, "trace-20240101.jsonl"),
      '{"type":"server_ready","port":1234,"ts":"2024-01-01T00:00:00Z"}\n',
    );
    const t = resolveTarget({ project: projectDir }, TARGET_CONTEXT);
    expect(t.port).toBe(1234);
    expect(t.source).toBe("trace");
    expect(t.chain).toBe("legacy");
  });

  test("全部缺失回落默认端口", () => {
    const t = resolveTarget({}, TARGET_CONTEXT);
    expect(t.port).toBe(9527);
    expect(t.source).toBe("default");
    expect(t.chain).toBe("default");
  });
});

describe("端点回溯", () => {
  test("登记含完整端点时按登记取值", () => {
    writeIndex({ instances: [pluginRecord()] });
    const record = findPluginRecordByPort(9600);
    expect(record?.mcpEndpoint?.host).toBe("127.0.0.1");
    expect(record?.mcpEndpoint?.port).toBe(9600);
    expect(record?.mcpEndpoint?.path).toBe("/mcp");
    expect(record?.mcpEndpoint?.protocol).toBe("mcp");
    expect(record?.mcpEndpoint?.auth).toBe("none");
    expect(record?.apiEndpoint?.path).toBe("/api");
    expect(record?.apiEndpoint?.protocol).toBe("http");
    expect(record?.apiEndpoint?.auth).toBe("bearer");
    expect(record?.apiToken).toBe("token-abc");
  });

  test("端点字段不全时按约定降级", () => {
    writeIndex({
      instances: [
        pluginRecord({
          endpoints: { api: { port: 9700 } },
        }),
      ],
    });
    const record = findPluginRecordByPort(9700);
    expect(record?.apiEndpoint?.port).toBe(9700);
    expect(record?.apiEndpoint?.host).toBe("127.0.0.1");
    expect(record?.apiEndpoint?.path).toBe("/api");
    expect(record?.apiEndpoint?.protocol).toBe("http");
    expect(record?.apiEndpoint?.auth).toBe("bearer");
    expect(record?.mcpEndpoint?.port).toBe(9700);
    expect(record?.mcpEndpoint?.path).toBe("/mcp");
  });

  test("只有扁平 port 时推导两个端点", () => {
    writeIndex({ instances: [{ port: 9710, project_path: "/p" }] });
    const record = findPluginRecordByPort(9710);
    expect(record?.mcpEndpoint?.port).toBe(9710);
    expect(record?.mcpEndpoint?.path).toBe("/mcp");
    expect(record?.apiEndpoint?.port).toBe(9710);
    expect(record?.apiEndpoint?.path).toBe("/api");
  });

  test("按工程路径匹配登记条目", () => {
    writeIndex({ instances: [pluginRecord({ project_path: process.cwd() })] });
    expect(findPluginRecordByProject(process.cwd())?.instanceId).toBe("i1");
  });

  test("索引损坏不抛异常且返回空列表", () => {
    writeRawIndex("{ not json");
    expect(readPluginInstanceIndex()).toEqual([]);
  });

  test("条目字段类型不符不抛异常", () => {
    writeIndex({ instances: [null, 42, { project_path: 5 }] });
    const records = readPluginInstanceIndex();
    expect(records).toHaveLength(1);
    expect(records[0].projectPath).toBe("");
    expect(records[0].port).toBeNull();
    expect(records[0].mcpEndpoint).toBeNull();
    expect(records[0].apiEndpoint).toBeNull();
  });
});

describe("存活校验", () => {
  test("轻量校验对运行中的实例判定存活", async () => {
    const { port } = serve(() => new Response("ok"));
    const alive = await probeEndpoint({ host: "127.0.0.1", port, url: `http://127.0.0.1:${port}/api` }, "light");
    expect(alive).toBe(true);
  });

  test("轻量校验对关闭端口判定不存活", async () => {
    const probe = serve(() => new Response("ok"));
    const port = probe.port;
    servers.pop()!.stop();
    const alive = await probeEndpoint({ host: "127.0.0.1", port, url: `http://127.0.0.1:${port}/api` }, "light");
    expect(alive).toBe(false);
  });

  test("严格校验附带应用级探活", async () => {
    const { port } = serve(() => new Response("missing", { status: 404 }));
    const alive = await probeEndpoint({ host: "127.0.0.1", port, url: `http://127.0.0.1:${port}/api` }, "strict");
    expect(alive).toBe(true);
  });
});

describe("状态码到退出码映射", () => {
  test("2xx 映射为成功", () => {
    expect(mapStatusToExitCode(200)).toBe(ExitCode.Success);
    expect(mapStatusToExitCode(204)).toBe(ExitCode.Success);
  });

  test("4xx 请求类映射为用法错误", () => {
    expect(mapStatusToExitCode(400)).toBe(ExitCode.Usage);
    expect(mapStatusToExitCode(404)).toBe(ExitCode.Usage);
    expect(mapStatusToExitCode(405)).toBe(ExitCode.Usage);
  });

  test("鉴权与服务端错误映射为传输失败", () => {
    expect(mapStatusToExitCode(401)).toBe(ExitCode.Transport);
    expect(mapStatusToExitCode(403)).toBe(ExitCode.Transport);
    expect(mapStatusToExitCode(500)).toBe(ExitCode.Transport);
    expect(mapStatusToExitCode(503)).toBe(ExitCode.Transport);
  });
});

describe("api 子命令", () => {
  test("api list 走 REST 端点并携带 Bearer 令牌", async () => {
    const { port, requests } = serve(() => jsonResponse({ tools: [{ name: "move_agent" }] }));
    writeIndex({ instances: [pluginRecord({ port, api_token: "token-abc" })] });
    const cap = captureOutput();
    const code = await apiRun(["list"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].method).toBe("GET");
    expect(requests[0].url).toBe("/api/tools");
    expect(requests[0].auth).toBe("Bearer token-abc");
    expect(JSON.parse(cap.stdout())).toEqual({ tools: [{ name: "move_agent" }] });
  });

  test("api list 透传关键字与类别过滤", async () => {
    const { port, requests } = serve(() => jsonResponse({ tools: [] }));
    writeIndex({ instances: [pluginRecord({ port })] });
    const cap = captureOutput();
    const code = await apiRun(["list", "spawn"], { portText: String(port), category: "scene" });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].url).toContain("query=spawn");
    expect(requests[0].url).toContain("category=scene");
  });

  test("api list 透传分页参数", async () => {
    const { port, requests } = serve(() => jsonResponse({ tools: [] }));
    writeIndex({ instances: [pluginRecord({ port })] });
    const cap = captureOutput();
    const code = await apiRun(["list"], { portText: String(port), limitText: "5", offsetText: "10" });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].url).toContain("limit=5");
    expect(requests[0].url).toContain("offset=10");
  });

  test("api list 拒绝非法分页参数", async () => {
    await expect(apiRun(["list"], { limitText: "0" })).rejects.toThrow("--limit");
    await expect(apiRun(["list"], { offsetText: "abc" })).rejects.toThrow("--offset");
  });

  test("api list 按关键字在本地过滤结果", async () => {
    const { port } = serve(() =>
      jsonResponse({
        tools: [
          { name: "move_agent", description: "moves the agent" },
          { name: "spawn_node", description: "spawns a node" },
        ],
      }),
    );
    writeIndex({ instances: [pluginRecord({ port })] });
    const cap = captureOutput();
    const code = await apiRun(["list"], { portText: String(port), query: "move" });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(JSON.parse(cap.stdout())).toEqual({ tools: [{ name: "move_agent", description: "moves the agent" }] });
  });

  test("api describe 请求单工具路径", async () => {
    const { port, requests } = serve(() => jsonResponse({ name: "move_agent", inputSchema: { type: "object" } }));
    writeIndex({ instances: [pluginRecord({ port })] });
    const cap = captureOutput();
    const code = await apiRun(["describe", "move_agent"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].method).toBe("GET");
    expect(requests[0].url).toBe("/api/tools/move_agent");
    expect(JSON.parse(cap.stdout())).toEqual({ name: "move_agent", inputSchema: { type: "object" } });
  });

  test("api call 提交工具参数", async () => {
    const { port, requests } = serve(() => jsonResponse({ ok: true }));
    writeIndex({ instances: [pluginRecord({ port })] });
    const cap = captureOutput();
    const code = await apiRun(["call", "move_agent"], { portText: String(port), argsJson: '{"x":1}' });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(requests[0].method).toBe("POST");
    expect(requests[0].url).toBe("/api/tools/move_agent");
    expect(JSON.parse(requests[0].body)).toEqual({ x: 1 });
    expect(JSON.parse(cap.stdout())).toEqual({ ok: true });
  });

  test("api call 缺参数报用法错误", async () => {
    const e = await catchCliError(apiRun(["call"], {}));
    expect(e.code).toBe(ExitCode.Usage);
  });

  test("服务端 404 映射为用法错误并原样呈现错误文本", async () => {
    const { port } = serve(() => jsonResponse({ error: "unknown tool: nope" }, 404));
    writeIndex({ instances: [pluginRecord({ port })] });
    const e = await catchCliError(apiRun(["describe", "nope"], { portText: String(port) }));
    expect(e.code).toBe(ExitCode.Usage);
    expect(e.message).toContain("404");
    expect(e.message).toContain("unknown tool: nope");
  });

  test("服务端 500 映射为传输失败", async () => {
    const { port } = serve(() => jsonResponse({ error: "editor busy" }, 500));
    writeIndex({ instances: [pluginRecord({ port })] });
    const e = await catchCliError(apiRun(["list"], { portText: String(port) }));
    expect(e.code).toBe(ExitCode.Transport);
    expect(e.message).toContain("editor busy");
  });

  test("200 响应含 error 字段按工具失败处理", async () => {
    const { port } = serve(() => jsonResponse({ error: "boom" }));
    writeIndex({ instances: [pluginRecord({ port })] });
    const cap = captureOutput();
    const code = await apiRun(["call", "move_agent"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.ToolFailure);
    expect(cap.stderr()).toContain("boom");
  });

  test("令牌缺失时报错并说明令牌来源", async () => {
    const { port } = serve(() => jsonResponse({ tools: [] }));
    writeIndex({ instances: [pluginRecord({ port, api_token: "" })] });
    const e = await catchCliError(apiRun(["list"], { portText: String(port) }));
    expect(e.code).toBe(ExitCode.Transport);
    expect(e.message).toContain("no API token");
    expect(e.suggestion).toBeDefined();
    expect(e.suggestion).toContain("api_token");
    expect(e.message).not.toContain("token-abc");
  });

  test("端点不可达时提示包含端口与来源", async () => {
    const probe = serve(() => jsonResponse({}));
    const port = probe.port;
    servers.pop()!.stop();
    const e = await catchCliError(apiRun(["list"], { portText: String(port) }));
    expect(e.code).toBe(ExitCode.Transport);
    expect(e.message).toContain(String(port));
    expect(e.message).toContain("source cli");
  });

  test("apiRequest 缺少令牌时不发送 Authorization 头", async () => {
    const { port, requests } = serve(() => jsonResponse({ tools: [] }));
    const response = await apiRequest("GET", "/tools", undefined, {
      endpoint: { host: "127.0.0.1", port, path: "/api", protocol: "http", auth: "none" },
      timeoutSec: 5,
      origin: "port 1, source test",
    });
    expect(response.status).toBe(200);
    expect(requests[0].auth).toBeNull();
  });
});

