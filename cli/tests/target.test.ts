import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { AmbiguousTargetError, resolveNativeTarget, resolveTarget, type InstanceEntry, type TargetContext } from "../src/target";
import { CliError, ExitCode } from "../src/errors";

let work: string;
let userData: string;
let originalUserData: string | undefined;

beforeEach(() => {
  work = fs.mkdtempSync(path.join(os.tmpdir(), "gda-target-"));
  userData = path.join(work, "userdata");
  fs.mkdirSync(userData, { recursive: true });
  // 经 GODOT_AUTOPILOT_USERDATA 覆写注入，跨平台一致（不再依赖 APPDATA 的 Windows 专属语义）
  originalUserData = process.env.GODOT_AUTOPILOT_USERDATA;
  process.env.GODOT_AUTOPILOT_USERDATA = userData;
  delete process.env.GODOT_AUTOPILOT_PORT;
});

afterEach(() => {
  if (originalUserData === undefined) {
    delete process.env.GODOT_AUTOPILOT_USERDATA;
  } else {
    process.env.GODOT_AUTOPILOT_USERDATA = originalUserData;
  }
  delete process.env.GODOT_AUTOPILOT_PORT;
  fs.rmSync(work, { recursive: true, force: true });
});

function makeEntry(overrides: Partial<InstanceEntry> = {}): InstanceEntry {
  return {
    id: "a",
    project: "/proj",
    port: 5555,
    pid: process.pid,
    startedAt: "2024-01-01T00:00:00Z",
    headless: false,
    godot: "4.3",
    log: "instance.log",
    ...overrides,
  };
}

function mockCtx(entries: InstanceEntry[]): TargetContext {
  return {
    findById: (id) => entries.find((e) => e.id === id),
    findLiveByProject: (project) => entries.find((e) => e.project === project),
  };
}

function writeProject(dir: string, name: string): string {
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, "project.godot"), `[application]\nconfig/name = "${name}"\n`);
  return dir;
}

function gdaDir(): string {
  // GODOT_AUTOPILOT_USERDATA 覆写值即 app_userdata 基本身（等价于各平台原生解析结果），
  // 故此处直接以其为根，不再拼 Godot/app_userdata 两级
  return path.join(userData, "TestProj", "godot_autopilot");
}

function writeTrace(port: number): void {
  const dir = path.join(gdaDir(), "traces");
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, "trace-20240101.jsonl"), `{"type":"server_ready","port":${port},"ts":"2024-01-01T00:00:00Z"}\n`);
}

function writeConfig(port: number): void {
  const dir = gdaDir();
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, "config.json"), JSON.stringify({ port }));
}

function catchUsage(fn: () => unknown): CliError {
  try {
    fn();
  } catch (e) {
    expect(e).toBeInstanceOf(CliError);
    return e as CliError;
  }
  throw new Error("expected a CliError but none was thrown");
}

describe("resolveTarget", () => {
  test("--id 命中返回实例端口", () => {
    const ctx = mockCtx([makeEntry({ id: "a", port: 5555 })]);
    const t = resolveTarget({ id: "a" }, ctx);
    expect(t.port).toBe(5555);
    expect(t.source).toBe("id");
    expect(t.instance?.id).toBe("a");
  });

  test("--id 未命中按传输失败报错并附 list 提示", () => {
    const err = catchUsage(() => resolveTarget({ id: "nope" }, mockCtx([])));
    expect(err.code).toBe(ExitCode.Transport);
    expect(err.message).toContain("unknown instance: nope");
    expect(err.message).toContain("gda list");
  });

  test("--id 指向已死进程按传输失败报错", () => {
    const ctx = mockCtx([makeEntry({ id: "dead", pid: 999999 })]);
    const err = catchUsage(() => resolveTarget({ id: "dead" }, ctx));
    expect(err.code).toBe(ExitCode.Transport);
    expect(err.message).toContain("unknown instance: dead");
  });

  test("--port 合法值直连", () => {
    const t = resolveTarget({ portText: "9527" }, mockCtx([]));
    expect(t.port).toBe(9527);
    expect(t.source).toBe("cli");
  });

  test("--port 非法值报用法错误", () => {
    for (const bad of ["abc", "0", "65536", "-1", "1.5"]) {
      const err = catchUsage(() => resolveTarget({ portText: bad }, mockCtx([])));
      expect(err.code).toBe(ExitCode.Usage);
    }
  });

  test("显式 flag 互斥报用法错误", () => {
    const err = catchUsage(() => resolveTarget({ id: "a", portText: "1" }, mockCtx([])));
    expect(err.code).toBe(ExitCode.Usage);
    expect(err.message).toBe("use only one of --id/--port/--project");
  });

  test("--project 登记命中优先返回实例端口", () => {
    const projDir = writeProject(path.join(work, "proj"), "TestProj");
    const ctx = mockCtx([makeEntry({ id: "a", project: path.resolve(projDir), port: 7777 })]);
    const t = resolveTarget({ project: projDir }, ctx);
    expect(t.port).toBe(7777);
    expect(t.source).toBe("project");
    expect(t.instance?.id).toBe("a");
  });

  test("--project 未命中不再走 trace", () => {
    const projDir = writeProject(path.join(work, "proj"), "TestProj");
    writeTrace(1234);
    expect(() => resolveTarget({ project: projDir }, mockCtx([]))).toThrow(/no running Godot Autopilot instance/);
  });

  test("--project 未命中不再走 config", () => {
    const projDir = writeProject(path.join(work, "proj"), "TestProj");
    writeConfig(5678);
    expect(() => resolveTarget({ project: projDir }, mockCtx([]))).toThrow(/no running Godot Autopilot instance/);
  });

  test("--project 未命中且 trace/config 均缺失时报错而非 9527", () => {
    const projDir = writeProject(path.join(work, "proj"), "TestProj");
    expect(() => resolveTarget({ project: projDir }, mockCtx([]))).toThrow(/no running Godot Autopilot instance/);
  });

  test("--project 从子目录向上找到 project.godot 但仍需实例", () => {
    const projDir = writeProject(path.join(work, "proj"), "TestProj");
    const deep = path.join(projDir, "deep", "sub");
    fs.mkdirSync(deep, { recursive: true });
    writeTrace(2345);
    expect(() => resolveTarget({ project: deep }, mockCtx([]))).toThrow(/no running Godot Autopilot instance/);
  });

  test("环境变量端口优先于 cwd 登记与默认", () => {
    process.env.GODOT_AUTOPILOT_PORT = "3000";
    const ctx = mockCtx([makeEntry({ id: "c", project: process.cwd(), port: 8888 })]);
    const t = resolveTarget({}, ctx);
    expect(t.port).toBe(3000);
    expect(t.source).toBe("env");
  });

  test("环境变量不合法时静默回退", () => {
    process.env.GODOT_AUTOPILOT_PORT = "abc";
    const ctx = mockCtx([makeEntry({ id: "c", project: process.cwd(), port: 8888 })]);
    const t = resolveTarget({}, ctx);
    expect(t.port).toBe(8888);
  });

  test("无显式 flag 时 cwd 登记命中", () => {
    const ctx = mockCtx([makeEntry({ id: "c", project: process.cwd(), port: 8888 })]);
    const t = resolveTarget({}, ctx);
    expect(t.port).toBe(8888);
    expect(t.source).toBe("cwd");
  });

  test("无显式 flag 且全部缺失时报错而非 9527", () => {
    expect(() => resolveTarget({}, mockCtx([]))).toThrow(/no running Godot Autopilot instance/);
  });
});

describe("resolveNativeTarget", () => {
  let prevHome: string | undefined;
  let nativeHome: string;

  beforeEach(() => {
    prevHome = process.env.GODOT_AUTOPILOT_HOME;
    nativeHome = path.join(work, "native-home");
    fs.mkdirSync(nativeHome, { recursive: true });
    process.env.GODOT_AUTOPILOT_HOME = nativeHome;
  });

  afterEach(() => {
    if (prevHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
    else process.env.GODOT_AUTOPILOT_HOME = prevHome;
  });

  function writeIndex(records: unknown[]): void {
    const dir = path.join(nativeHome, ".godot-autopilot");
    fs.mkdirSync(dir, { recursive: true });
    fs.writeFileSync(path.join(dir, "instances.json"), JSON.stringify({ instances: records }));
  }

  function pluginRecord(instanceId: string, project: string, port: number, pid: number): unknown {
    return {
      instance_id: instanceId,
      port,
      pid,
      project_path: project,
      endpoints: {
        mcp: { host: "127.0.0.1", port, path: "/mcp", protocol: "mcp", auth: "none" },
        ipc: { kind: "unix", path: path.join(nativeHome, `${instanceId}.sock`), protocol: "mcp-jsonrpc", framing: "length-prefixed-json", auth: "token", version: 1 },
      },
      started_at: Date.now(),
      version: "0.3.5",
      port_source: "persisted",
    };
  }

  test("显式 selector 优先级 --id > --socket > --port > --project", () => {
    const err = catchUsage(() => resolveNativeTarget({ id: "a", socketText: "/tmp/s.sock" }, mockCtx([makeEntry({ id: "a" })])));
    expect(err.code).toBe(ExitCode.Usage);
    expect(err.message).toBe("use only one of --id/--socket/--port/--project");
  });

  test("--socket 返回 socket 链并附带 IPC endpoint", () => {
    const socket = path.join(nativeHome, "native-1.sock");
    writeIndex([pluginRecord("native-1", path.join(nativeHome, "proj"), 6101, process.pid)]);
    const target = resolveNativeTarget({ socketText: socket }, mockCtx([]));
    expect(target.chain).toBe("socket");
    expect(target.source).toBe("socket");
    expect(target.pinned).toBe(true);
    expect(target.ipcEndpoint?.path).toBe(socket);
  });

  test("--port 返回 port 链并保留原生 endpoint", () => {
    writeIndex([pluginRecord("native-1", path.join(nativeHome, "proj"), 6102, process.pid)]);
    const target = resolveNativeTarget({ portText: "6102" }, mockCtx([]));
    expect(target.chain).toBe("port");
    expect(target.port).toBe(6102);
    expect(target.instanceId).toBe("native-1");
  });

  test("--project 多实例产生 candidates 歧义错误", () => {
    const project = path.join(nativeHome, "proj");
    writeIndex([
      pluginRecord("native-1", project, 6103, process.pid),
      pluginRecord("native-2", project, 6104, process.pid),
    ]);
    let caught: unknown;
    try {
      resolveNativeTarget({ project }, mockCtx([makeEntry({ id: "native-1", project, port: 6103 })]));
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(AmbiguousTargetError);
    expect((caught as AmbiguousTargetError).candidates).toHaveLength(2);
    expect((caught as AmbiguousTargetError).message).toContain("select one with --id");
  });

  test("--id 指向陈旧实例时拒绝固定", () => {
    const entry = makeEntry({ id: "dead", pid: 999999 });
    const err = catchUsage(() => resolveNativeTarget({ id: "dead" }, mockCtx([entry])));
    expect(err.code).toBe(ExitCode.Transport);
    expect(err.message).toContain("stale instance");
  });

  test("--id 命中活实例时固定实例 id", () => {
    const entry = makeEntry({ id: "pinned", pid: process.pid, port: 6105, startedAt: "" });
    const target = resolveNativeTarget({ id: "pinned" }, mockCtx([entry]));
    expect(target.pinned).toBe(true);
    expect(target.instanceId).toBe("pinned");
    expect(target.chain).toBe("id");
  });
});
