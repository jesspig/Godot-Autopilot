import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { resolveTarget, type InstanceEntry, type TargetContext } from "../src/target";
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

  test("--project 未命中走 trace", () => {
    const projDir = writeProject(path.join(work, "proj"), "TestProj");
    writeTrace(1234);
    const t = resolveTarget({ project: projDir }, mockCtx([]));
    expect(t.port).toBe(1234);
    expect(t.source).toBe("trace");
  });

  test("--project 未命中且 trace 缺失走 config", () => {
    const projDir = writeProject(path.join(work, "proj"), "TestProj");
    writeConfig(5678);
    const t = resolveTarget({ project: projDir }, mockCtx([]));
    expect(t.port).toBe(5678);
    expect(t.source).toBe("config");
  });

  test("--project 未命中且 trace/config 均缺失回落默认", () => {
    const projDir = writeProject(path.join(work, "proj"), "TestProj");
    const t = resolveTarget({ project: projDir }, mockCtx([]));
    expect(t.port).toBe(9527);
    expect(t.source).toBe("default");
  });

  test("--project 从子目录向上找到 project.godot", () => {
    const projDir = writeProject(path.join(work, "proj"), "TestProj");
    const deep = path.join(projDir, "deep", "sub");
    fs.mkdirSync(deep, { recursive: true });
    writeTrace(2345);
    const t = resolveTarget({ project: deep }, mockCtx([]));
    expect(t.port).toBe(2345);
    expect(t.source).toBe("trace");
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

  test("无显式 flag 且全部缺失回落默认 9527", () => {
    const t = resolveTarget({}, mockCtx([]));
    expect(t.port).toBe(9527);
    expect(t.source).toBe("default");
  });
});
