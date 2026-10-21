import { afterEach, afterAll, beforeAll, beforeEach, describe, expect, test } from "bun:test";
import { spawn, spawnSync } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { run as runList } from "../src/commands/list";
import { run as runLogs } from "../src/commands/logs";
import { run as runStart } from "../src/commands/start";
import { run as runStop } from "../src/commands/stop";
import { ExitCode } from "../src/errors";
import { isAlive, stopForce } from "../src/process";
import { findById, findLiveByProject, loadRegistry, stateDir, type InstanceEntry } from "../src/registry";
import { resolveTarget } from "../src/target";
import { killImage, killPid, removeDirWithRetry, waitDead, waitFor } from "./support/cleanup";

// ping 探测与编译产物冷启动引入固定延迟，2s 阈值在系统负载高时不足
// 首次执行的编译产物冷启动实测 3.5-3.8s（缓存命中后 ~1s），3s 就绪预算必然偶发超时
const START_READY_MAX_MS = 10_000;
const START_WAIT_TEXT = "10";
const TEST_TIMEOUT_MS = 60_000;
const HOOK_TIMEOUT_MS = 60_000;

const RUN_TAG = `gda-lifecycle-${Math.random().toString(36).slice(2, 8)}`;

let tmp: string;
let prevHome: string | undefined;
let prevGdaHome: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), `${RUN_TAG}-`));
  prevHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  prevGdaHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmp;
  // 插件实例索引落在集中根下，隔离到临时目录，避免读到本机真实实例
  process.env.GODOT_AUTOPILOT_HOME = tmp;
});

afterEach(async () => {
  try {
    await killAllRegistered();
    await killOrphanFakeGodot();
    await removeDirWithRetry(tmp);
  } catch (error) {
    process.stderr.write(`[cleanup] afterEach failed: ${String(error)}\n`);
  } finally {
    if (prevHome === undefined) {
      delete process.env.GODOT_AUTOPILOT_CLI_HOME;
    } else {
      process.env.GODOT_AUTOPILOT_CLI_HOME = prevHome;
    }
    if (prevGdaHome === undefined) {
      delete process.env.GODOT_AUTOPILOT_HOME;
    } else {
      process.env.GODOT_AUTOPILOT_HOME = prevGdaHome;
    }
  }
}, HOOK_TIMEOUT_MS);

async function killAllRegistered(): Promise<void> {
  try {
    for (const entry of loadRegistry().instances) {
      await killPid(entry.pid);
    }
  } catch {
  }
}

async function killOrphanFakeGodot(): Promise<void> {
  if (fakeGodotExe === undefined) return;
  if (process.platform !== "win32") return;
  await killImage(path.basename(fakeGodotExe));
}

function writeEntry(entry: InstanceEntry): void {
  let instances: InstanceEntry[] = [];
  try {
    const raw = fs.readFileSync(path.join(stateDir(), "instances.json"), "utf8");
    instances = (JSON.parse(raw) as { instances?: InstanceEntry[] }).instances ?? [];
  } catch {
  }
  fs.writeFileSync(
    path.join(stateDir(), "instances.json"),
    JSON.stringify({ version: 1, instances: [...instances, entry] }, null, 2),
  );
}

function makeEntry(overrides: Partial<InstanceEntry> = {}): InstanceEntry {
  return {
    id: "p999999",
    project: tmp,
    port: 9527,
    pid: 999999,
    startedAt: "2026-10-17T00:00:00.000Z",
    headless: true,
    godot: "godot",
    log: path.join(tmp, "editor.log"),
    ...overrides,
  };
}

function makeProjectDir(): string {
  const dir = path.join(tmp, "proj");
  fs.mkdirSync(dir, { recursive: true });
  return dir;
}

function registeredInstances(): InstanceEntry[] {
  return loadRegistry().instances;
}

async function captureStdout(fn: () => Promise<number>): Promise<{ code: number; stdout: string; stderr: string }> {
  const outChunks: string[] = [];
  const errChunks: string[] = [];
  const origOut = process.stdout.write;
  const origErr = process.stderr.write;
  process.stdout.write = ((chunk: unknown) => {
    outChunks.push(String(chunk));
    return true;
  }) as typeof process.stdout.write;
  process.stderr.write = ((chunk: unknown) => {
    errChunks.push(String(chunk));
    return true;
  }) as typeof process.stderr.write;
  try {
    const code = await fn();
    return { code, stdout: outChunks.join(""), stderr: errChunks.join("") };
  } finally {
    process.stdout.write = origOut;
    process.stderr.write = origErr;
  }
}

let fakeGodotDir: string;
let fakeGodotExe: string | undefined;
let fakeReportPath: string;

const FAKE_GODOT_SOURCE = `
const fs = require("node:fs");
const reportPath = process.env.GDA_FAKE_GODOT_REPORT;
function loadReport() {
  if (!reportPath) return null;
  try { return JSON.parse(fs.readFileSync(reportPath, "utf8")); } catch { return null; }
}
function saveReport(r) {
  if (reportPath) fs.writeFileSync(reportPath, JSON.stringify(r, null, 2));
}
const argv = process.argv.slice(2);
const noListen = process.env.GDA_FAKE_GODOT_NO_LISTEN === "1";
const prevReport = loadReport();
const report = {
  pid: process.pid,
  argv,
  env: {
    GODOT_AUTOPILOT_PORT: process.env.GODOT_AUTOPILOT_PORT ?? null,
    GDA_FORCE_HEADLESS: process.env.GDA_FORCE_HEADLESS ?? null,
  },
  import: prevReport?.import ?? null,
  calls: [],
  at: new Date().toISOString(),
};
if (argv.includes("--import")) {
  saveReport({ ...report, import: { at: new Date().toISOString(), path: argv[argv.indexOf("--path") + 1] ?? null } });
  process.exit(0);
}
saveReport(report);
if (process.env.GDA_FAKE_GODOT_EXIT === "1") {
  process.exit(3);
}
if (!noListen) {
  const port = Number(process.env.GODOT_AUTOPILOT_PORT ?? "0");
  Bun.serve({
    port,
    hostname: "127.0.0.1",
    fetch: async (req) => {
      const url = new URL(req.url);
      if (url.pathname !== "/mcp") return new Response("not found", { status: 404 });
      let body = {};
      try { body = await req.json(); } catch {}
      const params = body.params ?? {};
      const isDirect = params.name !== "call_tool";
      const toolName = isDirect ? (params.name ?? null) : (params.arguments?.name ?? null);
      report.calls.push({ method: body.method, name: toolName, direct: isDirect });
      saveReport(report);
      if (body?.method === "notifications/initialized") return new Response("{}", { status: 200 });
      if (toolName === "ping" && isDirect && process.env.GDA_FAKE_GODOT_PING_FAIL === "1") {
        return new Response(JSON.stringify({
          jsonrpc: "2.0",
          id: body?.id ?? 1,
          error: { code: -32602, message: "mock ping failure" },
        }), { status: 400, headers: { "content-type": "application/json" } });
      }
      if (toolName === "ping" && !isDirect) {
        return new Response(JSON.stringify({
          jsonrpc: "2.0",
          id: body?.id ?? 1,
          error: { code: -32602, message: "ping is a meta tool and must be called directly" },
        }), { status: 400, headers: { "content-type": "application/json" } });
      }
      const payload = {
        jsonrpc: "2.0",
        id: body?.id ?? 1,
        result: { content: [{ type: "text", text: JSON.stringify({ ok: true }) }] },
      };
      return new Response("event: message\\ndata: " + JSON.stringify(payload) + "\\n\\n", {
        status: 200,
        headers: { "content-type": "text/event-stream" },
      });
    },
  });
} else {
  setTimeout(() => {}, 30000);
}
`;

beforeAll(async () => {
  fakeGodotDir = fs.mkdtempSync(path.join(os.tmpdir(), `${RUN_TAG}-fake-godot-`));
  const scriptPath = path.join(fakeGodotDir, `${RUN_TAG}.js`);
  fs.writeFileSync(scriptPath, FAKE_GODOT_SOURCE);
  const result = await Bun.build({
    entrypoints: [scriptPath],
    outdir: path.join(fakeGodotDir, "bin"),
    target: "bun",
    compile: true,
  });
  if (!result.success) {
    throw new Error(`fake godot build failed: ${result.logs.join("\n")}`);
  }
  fakeGodotExe = result.outputs[0].path;
  if (process.platform !== "win32") {
    fs.chmodSync(fakeGodotExe, 0o755);
  }
  fakeReportPath = path.join(fakeGodotDir, "report.json");
  spawnSync(fakeGodotExe, ["--headless", "--import", "--path", fakeGodotDir], {
    env: { ...process.env, GDA_FAKE_GODOT_REPORT: fakeReportPath },
    stdio: "ignore",
    timeout: 30_000,
  });
}, HOOK_TIMEOUT_MS);

afterAll(async () => {
  const prevCliHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  const cleanupHome = fs.mkdtempSync(path.join(os.tmpdir(), `${RUN_TAG}-cleanup-`));
  try {
    process.env.GODOT_AUTOPILOT_CLI_HOME = cleanupHome;
    await killAllRegistered();
    await killOrphanFakeGodot();
    await removeDirWithRetry(fakeGodotDir);
  } catch (error) {
    process.stderr.write(`[cleanup] afterAll failed: ${String(error)}\n`);
  } finally {
    if (prevCliHome === undefined) {
      delete process.env.GODOT_AUTOPILOT_CLI_HOME;
    } else {
      process.env.GODOT_AUTOPILOT_CLI_HOME = prevCliHome;
    }
    await removeDirWithRetry(cleanupHome);
  }
}, HOOK_TIMEOUT_MS);

type FakeReport = NonNullable<ReturnType<typeof readReport>>;

function readReport(): { pid: number; argv: string[]; env: Record<string, string | null>; import: unknown; calls: { method: string; name: string | null; direct: boolean }[] } | null {
  if (!fs.existsSync(fakeReportPath)) return null;
  return JSON.parse(fs.readFileSync(fakeReportPath, "utf8"));
}

async function reportWhen(probe: (report: FakeReport) => boolean, timeoutMs = 2000): Promise<boolean> {
  return waitFor(() => {
    const report = readReport();
    return report !== null && probe(report);
  }, timeoutMs);
}

async function waitForInstances(count: number): Promise<InstanceEntry[]> {
  await waitFor(() => registeredInstances().length === count);
  return registeredInstances();
}

async function startFake(opts: { headless?: boolean; play?: boolean; noImport?: boolean; portText?: string; waitText?: string; projectDir?: string; noListen?: boolean; resetReport?: boolean; pingFail?: boolean }) {
  process.env.GDA_FAKE_GODOT_REPORT = fakeReportPath;
  if (opts.noListen === true) {
    process.env.GDA_FAKE_GODOT_NO_LISTEN = "1";
  }
  if (opts.pingFail === true) {
    process.env.GDA_FAKE_GODOT_PING_FAIL = "1";
  } else {
    delete process.env.GDA_FAKE_GODOT_PING_FAIL;
  }
  if (opts.resetReport !== false) {
    fs.rmSync(fakeReportPath, { force: true });
  }
  const projectDir = opts.projectDir ?? makeProjectDir();
  return captureStdout(() =>
    runStart([], {
      project: projectDir,
      godot: fakeGodotExe!,
      noImport: opts.noImport ?? true,
      headless: opts.headless ?? true,
      play: opts.play ?? false,
      portText: opts.portText,
      waitText: opts.waitText ?? START_WAIT_TEXT,
    }),
  );
}

describe.serial("list", () => {
  test("空登记输出 [] 且 exit 0", async () => {
    const { code, stdout } = await captureStdout(() => runList([], {}));
    expect(code).toBe(ExitCode.Success);
    expect(stdout.trim()).toBe("[]");
  });

  test("只输出存活实例并标注 live", async () => {
    const runner = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
    try {
      writeEntry(makeEntry({ id: `p${runner.pid}`, pid: runner.pid, project: path.join(tmp, "a") }));
      writeEntry(makeEntry({ id: "pdead", pid: 999999, project: path.join(tmp, "b") }));
      const { code, stdout } = await captureStdout(() => runList([], {}));
      expect(code).toBe(ExitCode.Success);
      const parsed = JSON.parse(stdout);
      expect(parsed).toHaveLength(1);
      expect(parsed[0].id).toBe(`p${runner.pid}`);
      expect(parsed[0].live).toBe(true);
      for (const key of ["id", "project", "port", "pid", "headless", "startedAt", "live"]) {
        expect(key in parsed[0]).toBe(true);
      }
    } finally {
      stopForce(runner.pid);
      await waitDead(runner.pid);
    }
  });

  test("--pretty 输出缩进多行", async () => {
    const runner = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
    try {
      writeEntry(makeEntry({ id: `p${runner.pid}`, pid: runner.pid, project: tmp }));
      const { code, stdout } = await captureStdout(() => runList([], { pretty: true }));
      expect(code).toBe(ExitCode.Success);
      expect(stdout).toContain("\n  ");
      expect(stdout).toContain(`"id": "p${runner.pid}"`);
    } finally {
      stopForce(runner.pid);
      await waitDead(runner.pid);
    }
  });
});

describe.serial("start", () => {
  test("headless 启动写入登记并探测就绪", async () => {
    const projectDir = makeProjectDir();
    const { code } = await startFake({ projectDir, headless: true, noImport: true });
    expect(code).toBe(ExitCode.Success);

    const instances = await waitForInstances(1);
    expect(instances).toHaveLength(1);
    const entry = instances[0];
    expect(entry.id).toMatch(/^p\d+$/);
    expect(entry.pid).toBeGreaterThan(0);
    expect(entry.port).toBeGreaterThanOrEqual(9530);
    expect(entry.port).toBeLessThanOrEqual(9599);
    expect(entry.headless).toBe(true);
    expect(entry.project).toBe(path.resolve(projectDir));
    expect(entry.godot).toBe(fakeGodotExe);
    expect(entry.log).toBe(path.join(stateDir(), "logs", `${entry.id}.log`));
    expect(fs.existsSync(entry.log)).toBe(true);

    expect(await reportWhen((r) => r.calls.some((c) => c.name === "ping" && c.direct === true))).toBe(true);
    const report = readReport();
    expect(report).not.toBeNull();
    expect(report!.env.GODOT_AUTOPILOT_PORT).toBe(String(entry.port));
    expect(report!.env.GDA_FORCE_HEADLESS).toBe("1");
    expect(report!.argv).toContain("--headless");
    expect(report!.argv).toContain("--editor");
    expect(report!.argv).toContain("--path");
    expect(report!.calls.some((c) => c.name === "ping" && c.direct === true)).toBe(true);
  }, TEST_TIMEOUT_MS);

  test("ping 直达探测 1s 内就绪", async () => {
    const projectDir = makeProjectDir();
    const start = Date.now();
    const { code } = await startFake({ projectDir, headless: true, noImport: true, waitText: "1" });
    const elapsed = Date.now() - start;
    expect(code).toBe(ExitCode.Success);
    expect(elapsed).toBeLessThan(START_READY_MAX_MS);
  }, TEST_TIMEOUT_MS);

  test("同工程已有活跃实例时拒绝启动并给出端口、进程号与可操作提示", async () => {
    const projectDir = makeProjectDir();
    const first = await startFake({ projectDir, headless: true });
    expect(first.code).toBe(ExitCode.Success);
    const instances = await waitForInstances(1);
    expect(instances).toHaveLength(1);
    const running = instances[0];

    const second = await startFake({ projectDir, headless: true, resetReport: false });
    expect(second.code).toBe(ExitCode.Transport);
    expect(second.stderr).toContain("already has a live Godot Autopilot instance");
    expect(second.stderr).toContain(`port ${running.port}`);
    expect(second.stderr).toContain(`pid ${running.pid}`);
    expect(second.stderr).toContain("gda stop --project");
    expect(second.stderr).toContain("--port");
    expect(second.stderr).toContain("GODOT_AUTOPILOT_ALLOW_MULTI_INSTANCE");
    expect((await waitForInstances(1))).toHaveLength(1);
  }, TEST_TIMEOUT_MS);

  test("无活跃实例时正常启动且不给出冲突提示", async () => {
    const projectDir = makeProjectDir();
    const { code, stderr } = await startFake({ projectDir, headless: true });
    expect(code).toBe(ExitCode.Success);
    expect(stderr).not.toContain("already has a live Godot Autopilot instance");
    const instances = await waitForInstances(1);
    expect(instances).toHaveLength(1);
  }, TEST_TIMEOUT_MS);

  test("显式指定端口时按用户意图启动并给出冲突提示", async () => {
    const projectDir = makeProjectDir();
    const first = await startFake({ projectDir, headless: true });
    expect(first.code).toBe(ExitCode.Success);
    const running = (await waitForInstances(1))[0];

    const second = await startFake({ projectDir, headless: true, portText: "9571", resetReport: false });
    expect(second.code).toBe(ExitCode.Success);
    expect(second.stderr).toContain("already has a live Godot Autopilot instance");
    expect(second.stderr).toContain(`port ${running.port}`);
    expect(second.stderr).toContain("starting on port 9571 anyway as requested");
    expect(second.stderr).not.toContain("refusing to start");
    const instances = await waitForInstances(2);
    expect(instances).toHaveLength(2);
    expect(instances.map((e) => e.port)).toContain(9571);
  }, TEST_TIMEOUT_MS);

  test("显式指定端口与既有实例同端口时提示端口已被占用", async () => {
    const projectDir = makeProjectDir();
    const first = await startFake({ projectDir, headless: true, portText: "9572" });
    expect(first.code).toBe(ExitCode.Success);
    const running = (await waitForInstances(1))[0];
    expect(running.port).toBe(9572);

    const second = await startFake({ projectDir, headless: true, portText: "9572", resetReport: false });
    expect(second.stderr).toContain("already has a live Godot Autopilot instance");
    expect(second.stderr).toContain("is the port that instance is already using");
    expect(second.code).toBe(ExitCode.Transport);
    expect((await waitForInstances(1))).toHaveLength(1);
  }, TEST_TIMEOUT_MS);

  test("默认执行 import 预步，--no-import 跳过", async () => {
    const withImport = await startFake({ headless: true, noImport: false });
    expect(withImport.code).toBe(ExitCode.Success);
    expect(await reportWhen((r) => r.import !== null)).toBe(true);
    expect(readReport()!.import).not.toBeNull();
    await killAllRegistered();

    const withoutImport = await startFake({ headless: true, noImport: true });
    expect(withoutImport.code).toBe(ExitCode.Success);
    expect(await reportWhen((r) => r.import === null)).toBe(true);
    expect(readReport()!.import).toBeNull();
  }, TEST_TIMEOUT_MS);

  test("--play 就绪后调用 play_editor_current_scene", async () => {
    const { code } = await startFake({ headless: true, play: true });
    expect(code).toBe(ExitCode.Success);
    expect(await reportWhen((r) => r.calls.some((c) => c.name === "play_editor_current_scene"))).toBe(true);
    const report = readReport();
    expect(report!.calls.some((c) => c.name === "play_editor_current_scene")).toBe(true);
  }, TEST_TIMEOUT_MS);

  test("端口被占用时自动跳过选择下一个", async () => {
    const blocker = Bun.serve({ port: 9530, hostname: "127.0.0.1", fetch: () => new Response("x") });
    try {
      const { code } = await startFake({ headless: true });
      expect(code).toBe(ExitCode.Success);
      const instances = await waitForInstances(1);
      expect(instances).toHaveLength(1);
      expect(instances[0].port).not.toBe(9530);
    } finally {
      blocker.stop();
    }
  }, TEST_TIMEOUT_MS);

  test("显式 --port 直接使用", async () => {
    const { code } = await startFake({ headless: true, portText: "9567" });
    expect(code).toBe(ExitCode.Success);
    expect((await waitForInstances(1))[0].port).toBe(9567);
  }, TEST_TIMEOUT_MS);

  test("非 headless 不注入无头环境", async () => {
    const { code } = await startFake({ headless: false });
    expect(code).toBe(ExitCode.Success);
    expect(await waitFor(() => readReport() !== null)).toBe(true);
    const report = readReport();
    expect(report!.env.GDA_FORCE_HEADLESS).toBeNull();
    expect(report!.argv).not.toContain("--headless");
  }, TEST_TIMEOUT_MS);

  test("就绪超时返回 exit 2、终止编辑器、清 pending 日志且不登记", async () => {
    process.env.GDA_FAKE_GODOT_REPORT = fakeReportPath;
    process.env.GDA_FAKE_GODOT_NO_LISTEN = "1";
    fs.rmSync(fakeReportPath, { force: true });
    try {
      const { code, stderr } = await captureStdout(() =>
        runStart([], {
          project: makeProjectDir(),
          godot: fakeGodotExe!,
          noImport: true,
          headless: true,
          waitText: "0.1",
        }),
      );
      expect(code).toBe(ExitCode.Transport);
      expect(registeredInstances()).toHaveLength(0);

      const report = readReport();
      expect(report).not.toBeNull();
      expect(stderr).toContain(`the editor process (pid ${report!.pid})`);
      expect(stderr).toContain("was terminated");
      expect(await waitFor(() => !isAlive(report!.pid), 5000)).toBe(true);

      const logsDir = path.join(stateDir(), "logs");
      const pending = fs.existsSync(logsDir) ? fs.readdirSync(logsDir).filter((n) => n.startsWith(".pending-")) : [];
      expect(pending).toHaveLength(0);
    } finally {
      delete process.env.GDA_FAKE_GODOT_NO_LISTEN;
      const report = readReport();
      if (report !== null) {
        await killPid(report.pid);
      }
    }
  }, TEST_TIMEOUT_MS);

  test("编辑器进程立即退出时不登记、清 pending 日志且不留残留", async () => {
    process.env.GDA_FAKE_GODOT_REPORT = fakeReportPath;
    process.env.GDA_FAKE_GODOT_EXIT = "1";
    fs.rmSync(fakeReportPath, { force: true });
    try {
      const { code, stderr } = await captureStdout(() =>
        runStart([], {
          project: makeProjectDir(),
          godot: fakeGodotExe!,
          noImport: true,
          headless: true,
          waitText: "0.2",
        }),
      );
      expect(code).toBe(ExitCode.Transport);
      expect(registeredInstances()).toHaveLength(0);

      const report = readReport();
      expect(report).not.toBeNull();
      expect(stderr).toContain(`the editor process (pid ${report!.pid})`);
      expect(await waitFor(() => !isAlive(report!.pid), 5000)).toBe(true);

      const logsDir = path.join(stateDir(), "logs");
      const pending = fs.existsSync(logsDir) ? fs.readdirSync(logsDir).filter((n) => n.startsWith(".pending-")) : [];
      expect(pending).toHaveLength(0);
    } finally {
      delete process.env.GDA_FAKE_GODOT_EXIT;
      const report = readReport();
      if (report !== null) {
        await killPid(report.pid);
      }
    }
  }, TEST_TIMEOUT_MS);

  test("就绪探测失败时终止进程、清理 pending 日志且不登记", async () => {
    const { code, stderr } = await startFake({
      headless: true,
      noImport: true,
      waitText: "0.2",
      pingFail: true,
    });
    expect(code).toBe(ExitCode.Transport);
    expect(stderr).toContain("was terminated");
    expect(registeredInstances()).toHaveLength(0);

    const report = readReport();
    expect(report).not.toBeNull();
    expect(stderr).toContain(`the editor process (pid ${report!.pid})`);
    expect(await waitFor(() => !isAlive(report!.pid), 5000)).toBe(true);

    const logsDir = path.join(stateDir(), "logs");
    const pending = fs.existsSync(logsDir) ? fs.readdirSync(logsDir).filter((n) => n.startsWith(".pending-")) : [];
    expect(pending).toHaveLength(0);
  }, TEST_TIMEOUT_MS);
});

describe.serial("stop", () => {
  test("未命中提示 gda list 且 exit 2", async () => {
    const { code, stderr } = await captureStdout(() => runStop(["nope"], {}));
    expect(code).toBe(ExitCode.Transport);
    expect(stderr).toContain("gda list");
  });

  test("缺少 id 与 project 报用法错误", async () => {
    const { code } = await captureStdout(() => runStop([], {}));
    expect(code).toBe(ExitCode.Usage);
  });

  test("优雅停止失败不静默升级，提示 --force 且保留登记", async () => {
    // 平台无关的「杀不掉」进程：POSIX 上显式忽略 SIGTERM（Windows 上 taskkill 无 /F
    // 本就不终止控制台进程），两平台均稳定落入优雅停止失败分支
    const runner = spawn(process.execPath, ["-e", "process.on('SIGTERM',()=>{});setTimeout(()=>{},20000)"], { stdio: "ignore" });
    try {
      writeEntry(makeEntry({ id: `p${runner.pid}`, pid: runner.pid, project: makeProjectDir() }));
      const { code, stderr, stdout } = await captureStdout(() => runStop([`p${runner.pid}`], { waitText: "1" }));
      expect(code).toBe(ExitCode.Transport);
      expect(stderr).toContain("--force");
      expect(JSON.parse(stdout).ok).toBe(false);
      expect(isAlive(runner.pid)).toBe(true);
      expect(registeredInstances()).toHaveLength(1);
    } finally {
      stopForce(runner.pid);
      await waitDead(runner.pid);
    }
  }, TEST_TIMEOUT_MS);

  test("--force 终止进程并清理登记", async () => {
    const runner = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
    const pid = runner.pid;
    writeEntry(makeEntry({ id: `p${pid}`, pid, project: makeProjectDir() }));
    const { code, stdout } = await captureStdout(() => runStop([`p${pid}`], { force: true }));
    expect(code).toBe(ExitCode.Success);
    expect(JSON.parse(stdout).ok).toBe(true);
    expect(isAlive(pid)).toBe(false);
    expect(registeredInstances()).toHaveLength(0);
  }, TEST_TIMEOUT_MS);

  test("进程已死时直接清理登记", async () => {
    const runner = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
    const pid = runner.pid;
    stopForce(pid);
    await waitDead(pid);
    writeEntry(makeEntry({ id: `p${pid}`, pid, project: makeProjectDir() }));
    const { code } = await captureStdout(() => runStop([`p${pid}`], {}));
    expect(code).toBe(ExitCode.Success);
    expect(registeredInstances()).toHaveLength(0);
  }, TEST_TIMEOUT_MS);

  test("--project 按工程路径定位实例", async () => {
    const runner = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
    const projectDir = makeProjectDir();
    writeEntry(makeEntry({ id: `p${runner.pid}`, pid: runner.pid, project: path.resolve(projectDir) }));
    const { code } = await captureStdout(() => runStop([], { project: projectDir, force: true }));
    expect(code).toBe(ExitCode.Success);
    expect(isAlive(runner.pid)).toBe(false);
    expect(registeredInstances()).toHaveLength(0);
  }, TEST_TIMEOUT_MS);

  test("--id 终止登记实例并清理登记", async () => {
    const runner = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
    const pid = runner.pid;
    writeEntry(makeEntry({ id: `p${pid}`, pid, port: 9601, project: makeProjectDir() }));
    const { code, stdout } = await captureStdout(() => runStop([], { id: `p${pid}`, force: true }));
    expect(code).toBe(ExitCode.Success);
    expect(JSON.parse(stdout).ok).toBe(true);
    expect(isAlive(pid)).toBe(false);
    expect(registeredInstances()).toHaveLength(0);
  }, TEST_TIMEOUT_MS);

  test("--port 终止该端口上的登记实例", async () => {
    const runner = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
    const pid = runner.pid;
    writeEntry(makeEntry({ id: `p${pid}`, pid, port: 9602, project: makeProjectDir() }));
    const { code, stdout } = await captureStdout(() => runStop([], { portText: "9602", force: true }));
    expect(code).toBe(ExitCode.Success);
    expect(JSON.parse(stdout).ok).toBe(true);
    expect(isAlive(pid)).toBe(false);
    expect(registeredInstances()).toHaveLength(0);
  }, TEST_TIMEOUT_MS);

  test("--port 无登记实例时报传输错误且不输出 ok:true", async () => {
    const { code, stdout, stderr } = await captureStdout(() => runStop([], { portText: "9603" }));
    expect(code).toBe(ExitCode.Transport);
    expect(stdout.trim()).toBe("");
    expect(stderr).toContain("no registered instance on port 9603");
  }, TEST_TIMEOUT_MS);

  test("--id 与 --port 互斥报用法错误", async () => {
    const { code, stderr } = await captureStdout(() => runStop([], { id: "p1", portText: "9604" }));
    expect(code).toBe(ExitCode.Usage);
    expect(stderr).toContain("use only one of");
  }, TEST_TIMEOUT_MS);

  test("位置 id 与 --id 互斥报用法错误", async () => {
    const { code, stderr } = await captureStdout(() => runStop(["p1"], { id: "p1" }));
    expect(code).toBe(ExitCode.Usage);
    expect(stderr).toContain("use only one of");
  }, TEST_TIMEOUT_MS);

  test("含 .. 的相对 --project 在寻址链与 stop 下解析到同一实例", async () => {
    const runner = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
    const pid = runner.pid;
    const projectDir = path.join(tmp, "proj", "nested");
    fs.mkdirSync(projectDir, { recursive: true });
    writeEntry(makeEntry({ id: `p${pid}`, pid, port: 9605, project: path.resolve(projectDir) }));
    const messy = `sub${path.sep}..${path.sep}${path.relative(process.cwd(), projectDir)}`;

    const target = resolveTarget({ project: messy }, { findById, findLiveByProject });
    expect(target.source).toBe("project");
    expect(target.instance?.id).toBe(`p${pid}`);

    const { code } = await captureStdout(() => runStop([], { project: messy, force: true }));
    expect(code).toBe(ExitCode.Success);
    expect(isAlive(pid)).toBe(false);
    expect(registeredInstances()).toHaveLength(0);
  }, TEST_TIMEOUT_MS);
});

describe.serial("logs", () => {
  test("默认输出全部行，--tail 截取末 N 行", async () => {
    const logPath = path.join(tmp, "editor.log");
    fs.writeFileSync(logPath, "l1\nl2\nl3\nl4\nl5\n");
    writeEntry(makeEntry({ id: "plogs", log: logPath }));

    const all = await captureStdout(() => runLogs(["plogs"], {}));
    expect(all.code).toBe(ExitCode.Success);
    expect(all.stdout).toBe("l1\nl2\nl3\nl4\nl5\n");

    const tail = await captureStdout(() => runLogs(["plogs"], { tailText: "2" }));
    expect(tail.code).toBe(ExitCode.Success);
    expect(tail.stdout).toBe("l4\nl5\n");
  });

  test("实例未登记与日志文件缺失均报传输错误", async () => {
    const missing = await captureStdout(() => runLogs(["pnope"], {}));
    expect(missing.code).toBe(ExitCode.Transport);
    expect(missing.stderr).toContain("gda list");

    const logPath = path.join(tmp, "editor.log");
    writeEntry(makeEntry({ id: "pnolog", log: logPath }));
    const noFile = await captureStdout(() => runLogs(["pnolog"], {}));
    expect(noFile.code).toBe(ExitCode.Transport);
    expect(noFile.stderr).toContain("log file missing");
  });

  test("缺少 id 报用法错误", async () => {
    const { code } = await captureStdout(() => runLogs([], {}));
    expect(code).toBe(ExitCode.Usage);
  });
});

describe.serial("CLI 端到端", () => {
  function cliRoot(): string {
    return path.resolve(import.meta.dir, "..", "..");
  }

  test("gda list 空登记输出 [] 且 exit 0", () => {
    const result = spawnSync(process.execPath, ["cli/bin/gda.ts", "list"], {
      cwd: cliRoot(),
      env: { ...process.env, GODOT_AUTOPILOT_CLI_HOME: tmp },
      encoding: "utf8",
    });
    expect(result.status).toBe(ExitCode.Success);
    expect(result.stdout.trim()).toBe("[]");
  });

  test("gda stop nope 未命中 exit 2", () => {
    const result = spawnSync(process.execPath, ["cli/bin/gda.ts", "stop", "nope"], {
      cwd: cliRoot(),
      env: { ...process.env, GODOT_AUTOPILOT_CLI_HOME: tmp },
      encoding: "utf8",
    });
    expect(result.status).toBe(ExitCode.Transport);
    expect(result.stderr).toContain("gda list");
  });
});
