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
import { loadRegistry, stateDir, type InstanceEntry } from "../src/registry";

let tmp: string;
let prevHome: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-lifecycle-"));
  prevHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmp;
});

afterEach(async () => {
  await killAllRegistered();
  await killOrphanFakeGodot();
  if (prevHome === undefined) {
    delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  } else {
    process.env.GODOT_AUTOPILOT_CLI_HOME = prevHome;
  }
  fs.rmSync(tmp, { recursive: true, force: true });
});

async function waitDead(pid: number, timeoutMs = 500): Promise<void> {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    if (!isAlive(pid)) return;
    await Bun.sleep(100);
  }
}

async function killAllRegistered(): Promise<void> {
  try {
    for (const entry of loadRegistry().instances) {
      try {
        stopForce(entry.pid);
      } catch {
      }
      await waitDead(entry.pid);
    }
  } catch {
  }
}

async function killOrphanFakeGodot(): Promise<void> {
  if (fakeGodotExe === undefined) return;
  if (process.platform !== "win32") return;
  const exeName = path.basename(fakeGodotExe);
  spawnSync("taskkill", ["/F", "/IM", exeName], { stdio: "ignore" });
  await Bun.sleep(200);
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
  fakeGodotDir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-lifecycle-fake-godot-"));
  const scriptPath = path.join(fakeGodotDir, "fake_godot.js");
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
});

afterAll(async () => {
  await killAllRegistered();
  await killOrphanFakeGodot();
  fs.rmSync(fakeGodotDir, { recursive: true, force: true });
});

function readReport(): { pid: number; argv: string[]; env: Record<string, string | null>; import: unknown; calls: { method: string; name: string | null; direct: boolean }[] } | null {
  if (!fs.existsSync(fakeReportPath)) return null;
  return JSON.parse(fs.readFileSync(fakeReportPath, "utf8"));
}

// 端口反查（netstat 轮询）+ ping 探测引入固定延迟，2s 阈值在系统负载高时不足
const START_READY_MAX_MS = 10_000;

async function startFake(opts: { headless?: boolean; play?: boolean; noImport?: boolean; portText?: string; waitText?: string; projectDir?: string; noListen?: boolean; resetReport?: boolean }) {
  process.env.GDA_FAKE_GODOT_REPORT = fakeReportPath;
  if (opts.noListen === true) {
    process.env.GDA_FAKE_GODOT_NO_LISTEN = "1";
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
      waitText: opts.waitText ?? "3",
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

    const instances = registeredInstances();
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

    const report = readReport();
    expect(report).not.toBeNull();
    expect(report!.env.GODOT_AUTOPILOT_PORT).toBe(String(entry.port));
    expect(report!.env.GDA_FORCE_HEADLESS).toBe("1");
    expect(report!.argv).toContain("--headless");
    expect(report!.argv).toContain("--editor");
    expect(report!.argv).toContain("--path");
    expect(report!.calls.some((c) => c.name === "ping" && c.direct === true)).toBe(true);
  }, 30000);

  test("ping 直达探测 1s 内就绪", async () => {
    const projectDir = makeProjectDir();
    const start = Date.now();
    const { code } = await startFake({ projectDir, headless: true, noImport: true, waitText: "1" });
    const elapsed = Date.now() - start;
    expect(code).toBe(ExitCode.Success);
    expect(elapsed).toBeLessThan(START_READY_MAX_MS);
  }, 30000);

  test("幂等：同 project 二次 start 不重复启动", async () => {
    const projectDir = makeProjectDir();
    const first = await startFake({ projectDir, headless: true });
    expect(first.code).toBe(ExitCode.Success);
    const instances = registeredInstances();
    expect(instances).toHaveLength(1);
    expect(readReport()!.calls.filter((c) => c.name === "ping" && c.direct === true)).toHaveLength(1);

    const second = await startFake({ projectDir, headless: true, resetReport: false });
    expect(second.code).toBe(ExitCode.Success);
    const parsed = JSON.parse(second.stdout);
    expect(parsed.id).toBe(instances[0].id);
    expect(registeredInstances()).toHaveLength(1);
    expect(readReport()!.calls.filter((c) => c.name === "ping" && c.direct === true)).toHaveLength(1);
  }, 30000);

  test("默认执行 import 预步，--no-import 跳过", async () => {
    const withImport = await startFake({ headless: true, noImport: false });
    expect(withImport.code).toBe(ExitCode.Success);
    expect(readReport()!.import).not.toBeNull();
    await killAllRegistered();

    const withoutImport = await startFake({ headless: true, noImport: true });
    expect(withoutImport.code).toBe(ExitCode.Success);
    expect(readReport()!.import).toBeNull();
  }, 30000);

  test("--play 就绪后调用 play_editor_current_scene", async () => {
    const { code } = await startFake({ headless: true, play: true });
    expect(code).toBe(ExitCode.Success);
    const report = readReport();
    expect(report!.calls.some((c) => c.name === "play_editor_current_scene")).toBe(true);
  }, 30000);

  test("端口被占用时自动跳过选择下一个", async () => {
    const blocker = Bun.serve({ port: 9530, hostname: "127.0.0.1", fetch: () => new Response("x") });
    try {
      const { code } = await startFake({ headless: true });
      expect(code).toBe(ExitCode.Success);
      const instances = registeredInstances();
      expect(instances).toHaveLength(1);
      expect(instances[0].port).not.toBe(9530);
    } finally {
      blocker.stop();
    }
  }, 30000);

  test("显式 --port 直接使用", async () => {
    const { code } = await startFake({ headless: true, portText: "9567" });
    expect(code).toBe(ExitCode.Success);
    expect(registeredInstances()[0].port).toBe(9567);
  }, 30000);

  test("非 headless 不注入无头环境", async () => {
    const { code } = await startFake({ headless: false });
    expect(code).toBe(ExitCode.Success);
    const report = readReport();
    expect(report!.env.GDA_FORCE_HEADLESS).toBeNull();
    expect(report!.argv).not.toContain("--headless");
  }, 30000);

  test("就绪超时返回 exit 2 且不登记", async () => {
    process.env.GDA_FAKE_GODOT_REPORT = fakeReportPath;
    process.env.GDA_FAKE_GODOT_NO_LISTEN = "1";
    fs.rmSync(fakeReportPath, { force: true });
    try {
      const { code } = await captureStdout(() =>
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
      if (report !== null) {
        stopForce(report.pid);
        await waitDead(report.pid);
      }
    } finally {
      delete process.env.GDA_FAKE_GODOT_NO_LISTEN;
    }
  }, 30000);
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
    const runner = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
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
  }, 30000);

  test("--force 终止进程并清理登记", async () => {
    const runner = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
    const pid = runner.pid;
    writeEntry(makeEntry({ id: `p${pid}`, pid, project: makeProjectDir() }));
    const { code, stdout } = await captureStdout(() => runStop([`p${pid}`], { force: true }));
    expect(code).toBe(ExitCode.Success);
    expect(JSON.parse(stdout).ok).toBe(true);
    expect(isAlive(pid)).toBe(false);
    expect(registeredInstances()).toHaveLength(0);
  }, 30000);

  test("进程已死时直接清理登记", async () => {
    const runner = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
    const pid = runner.pid;
    stopForce(pid);
    await waitDead(pid);
    writeEntry(makeEntry({ id: `p${pid}`, pid, project: makeProjectDir() }));
    const { code } = await captureStdout(() => runStop([`p${pid}`], {}));
    expect(code).toBe(ExitCode.Success);
    expect(registeredInstances()).toHaveLength(0);
  }, 30000);

  test("--project 按工程路径定位实例", async () => {
    const runner = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
    const projectDir = makeProjectDir();
    writeEntry(makeEntry({ id: `p${runner.pid}`, pid: runner.pid, project: path.resolve(projectDir) }));
    const { code } = await captureStdout(() => runStop([], { project: projectDir, force: true }));
    expect(code).toBe(ExitCode.Success);
    expect(isAlive(runner.pid)).toBe(false);
    expect(registeredInstances()).toHaveLength(0);
  }, 30000);
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
