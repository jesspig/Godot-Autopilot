import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { spawn } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { CliError, ExitCode } from "../src/errors";
import { isAlive, stopForce } from "../src/process";
import {
  findById,
  findLiveByProject,
  loadRegistry,
  saveRegistry,
  stateDir,
  withLock,
  type InstanceEntry,
} from "../src/registry";

let tmp: string;
let prevHome: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-registry-test-"));
  prevHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmp;
});

afterEach(() => {
  if (prevHome === undefined) {
    delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  } else {
    process.env.GODOT_AUTOPILOT_CLI_HOME = prevHome;
  }
  fs.rmSync(tmp, { recursive: true, force: true });
});

function makeEntry(overrides: Partial<InstanceEntry> = {}): InstanceEntry {
  return {
    id: "p999999",
    project: tmp,
    port: 9527,
    pid: 999999,
    startedAt: "2026-10-17T00:00:00.000Z",
    headless: false,
    godot: "godot",
    log: path.join(tmp, "editor.log"),
    ...overrides,
  };
}

function spawnLongRunner(): number {
  const child = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
  return child.pid!;
}

async function waitForDead(pid: number, timeoutMs = 12000): Promise<void> {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    if (!isAlive(pid)) return;
    await new Promise((r) => setTimeout(r, 100));
  }
  throw new Error(`process ${pid} still alive after ${timeoutMs}ms`);
}

describe("stateDir", () => {
  test("返回 GODOT_AUTOPILOT_CLI_HOME 环境变量", () => {
    expect(stateDir()).toBe(tmp);
  });
});

describe("loadRegistry 三态", () => {
  test("文件不存在返回空数据", () => {
    expect(loadRegistry()).toEqual({ version: 1, instances: [] });
  });

  test("损坏 JSON 抛 CliError(Transport)", () => {
    fs.writeFileSync(path.join(tmp, "instances.json"), "{ not json");
    try {
      loadRegistry();
      expect.unreachable();
    } catch (e) {
      expect(e).toBeInstanceOf(CliError);
      expect((e as CliError).code).toBe(ExitCode.Transport);
      expect((e as CliError).message).toContain("cannot parse instances.json");
    }
  });

  test("正常数据经 saveRegistry 往返", async () => {
    const pid = spawnLongRunner();
    try {
      saveRegistry({ version: 1, instances: [makeEntry({ id: `p${pid}`, pid })] });
      const loaded = loadRegistry();
      expect(loaded.instances).toHaveLength(1);
      expect(loaded.instances[0].id).toBe(`p${pid}`);
      expect(loaded.instances[0].pid).toBe(pid);
    } finally {
      stopForce(pid);
      await waitForDead(pid);
    }
  });
});

describe("死 pid 清理", () => {
  test("加载时剔除死 pid 条目并原子回写", () => {
    saveRegistry({ version: 1, instances: [makeEntry()] });
    const loaded = loadRegistry();
    expect(loaded.instances).toHaveLength(0);
    const raw = JSON.parse(fs.readFileSync(path.join(tmp, "instances.json"), "utf8"));
    expect(raw.instances).toHaveLength(0);
  });
});

describe("withLock 并发竞速", () => {
  test("跨进程后进入者等待前者释放且都成功", async () => {
    const registryAbs = path.join(import.meta.dir, "..", "src", "registry.ts");
    const child = spawn(process.execPath, ["-e", `
      const { withLock } = await import(${JSON.stringify(registryAbs)});
      const t0 = Date.now();
      await new Promise((r) => setTimeout(r, 200));
      const r = withLock(() => "second");
      console.log(JSON.stringify({ r, ms: Date.now() - t0 }));
    `], {
      env: { ...process.env, GODOT_AUTOPILOT_CLI_HOME: tmp },
      stdio: ["ignore", "pipe", "pipe"],
    });
    const holdResult = withLock(() => {
      const start = Date.now();
      while (Date.now() - start < 800) {
        Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 50);
      }
      return "first";
    });
    const out = await new Promise<string>((resolve, reject) => {
      let buf = "";
      child.stdout.on("data", (d) => {
        buf += d.toString();
      });
      child.on("exit", (code) => {
        if (code === 0) resolve(buf);
        else reject(new Error(`子进程退出码 ${code}`));
      });
    });
    const result = JSON.parse(out.trim());
    expect(result.r).toBe("second");
    expect(result.ms).toBeGreaterThanOrEqual(400);
    expect(holdResult).toBe("first");
  });
});

describe("findById", () => {
  test("命中与未命中", async () => {
    const pid = spawnLongRunner();
    try {
      saveRegistry({ version: 1, instances: [makeEntry({ id: `p${pid}`, pid })] });
      expect(findById(`p${pid}`)?.pid).toBe(pid);
      expect(findById("pnope")).toBeUndefined();
    } finally {
      stopForce(pid);
      await waitForDead(pid);
    }
  });
});

describe("findLiveByProject", () => {
  test("归一化路径比较并复核存活", async () => {
    const pid = spawnLongRunner();
    try {
      const project = path.join(tmp, "MyProj");
      saveRegistry({ version: 1, instances: [makeEntry({ id: `p${pid}`, pid, project })] });
      const query = process.platform === "win32"
        ? project.toLowerCase().replace(/\\/g, "/")
        : project;
      expect(findLiveByProject(query)?.id).toBe(`p${pid}`);
      expect(findLiveByProject(path.join(tmp, "AbsentProj"))).toBeUndefined();
      saveRegistry({ version: 1, instances: [makeEntry({ id: "pdead", pid: 999999, project })] });
      expect(findLiveByProject(query)).toBeUndefined();
    } finally {
      stopForce(pid);
      await waitForDead(pid);
    }
  });
});
