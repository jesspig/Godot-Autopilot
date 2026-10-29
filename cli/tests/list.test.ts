import { spawn } from "node:child_process";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import { afterEach, beforeEach, describe, expect, test } from "bun:test";

import { parseArgv, parseProbePort, type GlobalOptions } from "../src/args";
import { run as runList } from "../src/commands/list";
import { CliError, ExitCode } from "../src/errors";
import { isAlive, stopForce } from "../src/process";
import { saveRegistry, type InstanceEntry } from "../src/registry";

const INVALID_PID = 2147483647;
const CLI_STARTED_AT = "2026-10-29T00:00:00.000Z";
const deadPidDetectionAvailable = !isAlive(INVALID_PID);

interface Capture {
  code: number;
  stdout: string;
  stderr: string;
}

let tmpHome: string;
let prevGdaHome: string | undefined;
let prevCliHome: string | undefined;
const runners: number[] = [];

beforeEach(() => {
  tmpHome = fs.mkdtempSync(path.join(os.tmpdir(), "gda-list-test-"));
  prevGdaHome = process.env.GODOT_AUTOPILOT_HOME;
  prevCliHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmpHome;
  delete process.env.GODOT_AUTOPILOT_CLI_HOME;
});

afterEach(async () => {
  for (const pid of runners.splice(0)) {
    if (isAlive(pid)) {
      stopForce(pid);
    }
  }
  restoreEnv("GODOT_AUTOPILOT_HOME", prevGdaHome);
  restoreEnv("GODOT_AUTOPILOT_CLI_HOME", prevCliHome);
  fs.rmSync(tmpHome, { recursive: true, force: true });
});

function restoreEnv(name: string, value: string | undefined): void {
  if (value === undefined) {
    delete process.env[name];
  } else {
    process.env[name] = value;
  }
}

function pluginIndexFile(): string {
  return path.join(tmpHome, ".godot-autopilot", "instances.json");
}

function writePluginIndex(items: unknown[]): void {
  fs.mkdirSync(path.dirname(pluginIndexFile()), { recursive: true });
  fs.writeFileSync(pluginIndexFile(), JSON.stringify({ instances: items }));
}

function pluginRecord(port: number, overrides: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    instance_id: `gda-test-${port}`,
    port,
    pid: INVALID_PID,
    project_path: path.join(tmpHome, "plugin-project"),
    api_token: "plugin-token",
    endpoints: {
      mcp: { host: "127.0.0.1", port, path: "/mcp", protocol: "mcp", auth: "none" },
      api: { host: "127.0.0.1", port, path: "/api", protocol: "http", auth: "bearer" },
    },
    gda_home: tmpHome,
    started_at: 1700000000000,
    version: "0.3.0",
    ...overrides,
  };
}

function cliEntry(overrides: Partial<InstanceEntry> = {}): InstanceEntry {
  return {
    id: `p${process.pid}`,
    project: path.join(tmpHome, "cli-project"),
    port: 9530,
    pid: process.pid,
    startedAt: CLI_STARTED_AT,
    headless: false,
    godot: "godot",
    log: path.join(tmpHome, "editor.log"),
    ...overrides,
  };
}

function spawnRunner(): number {
  const child = spawn(process.execPath, ["-e", "setTimeout(()=>{},20000)"], { stdio: "ignore" });
  const pid = child.pid;
  if (pid === undefined) {
    throw new Error("failed to spawn the long-running helper process");
  }
  runners.push(pid);
  return pid;
}

async function listCaptured(opts: GlobalOptions = {}): Promise<Capture> {
  const origOut = process.stdout.write;
  const origErr = process.stderr.write;
  let stdout = "";
  let stderr = "";
  process.stdout.write = ((chunk: unknown) => {
    stdout += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  process.stderr.write = ((chunk: unknown) => {
    stderr += String(chunk);
    return true;
  }) as typeof process.stderr.write;
  try {
    const code = await runList([], opts);
    return { code, stdout, stderr };
  } finally {
    process.stdout.write = origOut;
    process.stderr.write = origErr;
  }
}

function parseList(stdout: string): Array<Record<string, unknown>> {
  return JSON.parse(stdout) as Array<Record<string, unknown>>;
}

async function catchCliError(p: Promise<number>): Promise<CliError> {
  try {
    await p;
  } catch (error) {
    expect(error).toBeInstanceOf(CliError);
    return error as CliError;
  }
  throw new Error("expected the list command to fail");
}

function listenLoopback(): Promise<{ server: net.Server; port: number }> {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () => {
      const address = server.address();
      if (address === null || typeof address === "string") {
        reject(new Error("the loopback server did not expose a port"));
        return;
      }
      resolve({ server, port: address.port });
    });
  });
}

function closeServer(server: net.Server): Promise<void> {
  return new Promise((resolve) => {
    server.close(() => resolve());
  });
}

describe("list 合并插件索引", () => {
  test("合并 CLI registry 与插件索引并标注 source/live", async () => {
    const runner = spawnRunner();
    saveRegistry({ version: 1, instances: [cliEntry()] });
    writePluginIndex([pluginRecord(9601, { pid: runner, instance_id: "gda-plugin-1" })]);
    const { code, stdout, stderr } = await listCaptured();
    expect(code).toBe(ExitCode.Success);
    expect(stderr).toBe("");
    const parsed = parseList(stdout);
    expect(parsed).toHaveLength(2);
    expect(parsed[0]).toEqual({
      id: `p${process.pid}`,
      source: "cli",
      project: path.join(tmpHome, "cli-project"),
      port: 9530,
      pid: process.pid,
      headless: false,
      startedAt: CLI_STARTED_AT,
      live: true,
    });
    expect(parsed[1]).toEqual({
      id: "gda-plugin-1",
      source: "plugin",
      project: path.join(tmpHome, "plugin-project"),
      port: 9601,
      pid: runner,
      headless: false,
      startedAt: new Date(1700000000000).toISOString(),
      live: true,
    });
  });

  test("pid 或 port 与 CLI 条目重复时保留 CLI 条目", async () => {
    const runner = spawnRunner();
    saveRegistry({ version: 1, instances: [cliEntry()] });
    writePluginIndex([
      pluginRecord(9601, { pid: process.pid, instance_id: "gda-same-pid" }),
      pluginRecord(9530, { pid: runner, instance_id: "gda-same-port" }),
    ]);
    const { code, stdout } = await listCaptured();
    expect(code).toBe(ExitCode.Success);
    const parsed = parseList(stdout);
    expect(parsed).toHaveLength(1);
    expect(parsed[0].id).toBe(`p${process.pid}`);
    expect(parsed[0].source).toBe("cli");
  });

  test("按 startedAt 降序排列在 CLI 条目之后", async () => {
    const older = spawnRunner();
    const newer = spawnRunner();
    writePluginIndex([
      pluginRecord(9601, { pid: older, instance_id: "gda-older", started_at: 1600000000000 }),
      pluginRecord(9603, { pid: newer, instance_id: "gda-newer", started_at: 1800000000000 }),
    ]);
    const { code, stdout } = await listCaptured();
    expect(code).toBe(ExitCode.Success);
    const parsed = parseList(stdout);
    expect(parsed.map((entry) => entry.id)).toEqual(["gda-newer", "gda-older"]);
    expect(parsed.every((entry) => entry.live === true)).toBe(true);
  });

  test.skipIf(!deadPidDetectionAvailable)("过滤进程已死的插件条目", async () => {
    const older = spawnRunner();
    writePluginIndex([
      pluginRecord(9601, { pid: older, instance_id: "gda-alive", started_at: 1600000000000 }),
      pluginRecord(9602, { instance_id: "gda-dead", started_at: 1900000000000 }),
    ]);
    const { code, stdout } = await listCaptured();
    expect(code).toBe(ExitCode.Success);
    const parsed = parseList(stdout);
    expect(parsed.map((entry) => entry.id)).toEqual(["gda-alive"]);
  });

  test("插件索引损坏时跳过且不影响 CLI 条目", async () => {
    saveRegistry({ version: 1, instances: [cliEntry()] });
    fs.mkdirSync(path.dirname(pluginIndexFile()), { recursive: true });
    fs.writeFileSync(pluginIndexFile(), "{ not json");
    const { code, stdout } = await listCaptured();
    expect(code).toBe(ExitCode.Success);
    const parsed = parseList(stdout);
    expect(parsed).toHaveLength(1);
    expect(parsed[0].source).toBe("cli");
  });

  test("缺端口或进程号的插件条目被跳过", async () => {
    writePluginIndex([{ instance_id: "gda-incomplete", project_path: path.join(tmpHome, "x") }]);
    const { code, stdout } = await listCaptured();
    expect(code).toBe(ExitCode.Success);
    expect(parseList(stdout)).toEqual([]);
  });

  test("空索引提示可加 --probe 探测端口", async () => {
    const { code, stdout, stderr } = await listCaptured();
    expect(code).toBe(ExitCode.Success);
    expect(stdout.trim()).toBe("[]");
    expect(stderr).toContain("no CLI-started or plugin-indexed instances");
    expect(stderr).toContain("--probe");
  });
});

describe("list --probe 端口探测", () => {
  test("命中监听端口时输出 source=probe 条目", async () => {
    const { server, port } = await listenLoopback();
    try {
      const { code, stdout } = await listCaptured({ probe: String(port) });
      expect(code).toBe(ExitCode.Success);
      const parsed = parseList(stdout);
      expect(parsed).toHaveLength(1);
      expect(parsed[0].source).toBe("probe");
      expect(parsed[0].id).toBe(`probe:${port}`);
      expect(parsed[0].port).toBe(port);
      expect(parsed[0].pid).toBeNull();
      expect(parsed[0].live).toBe(true);
      expect(typeof parsed[0].startedAt).toBe("string");
    } finally {
      await closeServer(server);
    }
  });

  test("端口无人监听时省略条目且不报错", async () => {
    const { server, port } = await listenLoopback();
    await closeServer(server);
    const { code, stdout, stderr } = await listCaptured({ probe: String(port) });
    expect(code).toBe(ExitCode.Success);
    expect(stdout.trim()).toBe("[]");
    expect(stderr).toContain("probed port");
  });

  test("非法 --probe 值报用法错误", async () => {
    const notNumber = await catchCliError(runList([], { probe: "abc" }));
    expect(notNumber.code).toBe(ExitCode.Usage);
    expect(notNumber.message).toContain("--probe");
    const outOfRange = await catchCliError(runList([], { probe: "70000" }));
    expect(outOfRange.code).toBe(ExitCode.Usage);
    const zero = await catchCliError(runList([], { probe: "0" }));
    expect(zero.code).toBe(ExitCode.Usage);
  });

  test("parseProbePort 接受 1-65535 的整数", () => {
    expect(parseProbePort("9527")).toBe(9527);
    expect(parseProbePort(" 1 ")).toBe(1);
    expect(parseProbePort("65535")).toBe(65535);
    expect(() => parseProbePort("9527.5")).toThrow("--probe");
  });
});

describe("list 兼容性", () => {
  test("仅 CLI registry 时旧字段与值不变", async () => {
    const entry = cliEntry();
    saveRegistry({ version: 1, instances: [entry] });
    const { code, stdout } = await listCaptured();
    expect(code).toBe(ExitCode.Success);
    const parsed = parseList(stdout);
    expect(parsed).toHaveLength(1);
    expect(parsed[0]).toEqual({
      id: entry.id,
      source: "cli",
      project: entry.project,
      port: entry.port,
      pid: entry.pid,
      headless: entry.headless,
      startedAt: entry.startedAt,
      live: true,
    });
  });

  test("空注册表输出空数组且退出码 0", async () => {
    const { code, stdout } = await listCaptured();
    expect(code).toBe(ExitCode.Success);
    expect(stdout.trim()).toBe("[]");
    expect(JSON.parse(stdout)).toEqual([]);
  });

  test("parseArgv 识别 list 的 --probe 端口值", () => {
    const { opts, positionals } = parseArgv(["--probe", "9527", "list"]);
    expect(opts.probe).toBe("9527");
    expect(positionals).toEqual(["list"]);
  });
});
