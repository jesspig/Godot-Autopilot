import { afterAll, afterEach, beforeAll, beforeEach, describe, expect, test } from "bun:test";
import { spawn } from "node:child_process";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import { run as runRestart } from "../src/commands/restart";
import { ExitCode } from "../src/errors";
import { isAlive, spawnEditor } from "../src/process";
import { loadRegistry, saveRegistry, type InstanceEntry } from "../src/registry";
import { killImage, killPid, removeDirWithRetry, waitDead } from "./support/cleanup";

const RUN_TAG = `gda-restart-${Math.random().toString(36).slice(2, 8)}`;
const TEST_TIMEOUT_MS = 60_000;

const FAKE_GODOT_SOURCE = `
const port = Number(process.env.GODOT_AUTOPILOT_PORT ?? "0");
if (process.argv.includes("--import")) process.exit(0);
if (process.env.GDA_FAKE_GODOT_MODE === "exit") process.exit(3);
Bun.serve({
  port,
  hostname: "127.0.0.1",
  fetch: async (req) => {
    const url = new URL(req.url);
    if (url.pathname !== "/mcp") return new Response("not found", { status: 404 });
    let body = {};
    try { body = await req.json(); } catch {}
    if (body?.method === "notifications/initialized") return new Response("{}", { status: 200 });
    const payload = { jsonrpc: "2.0", id: body?.id ?? 1, result: { content: [{ type: "text", text: "{\\"ok\\":true}" }] } };
    return new Response("event: message\\ndata: " + JSON.stringify(payload) + "\\n\\n", { status: 200, headers: { "content-type": "text/event-stream" } });
  },
});
`;

let tmp: string;
let fakeGodotDir: string;
let fakeGodotExe: string | undefined;
let prevCliHome: string | undefined;
let prevHome: string | undefined;
let prevMulti: string | undefined;
let prevMode: string | undefined;
let editorPids: number[] = [];
let strayPids: number[] = [];

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
}, TEST_TIMEOUT_MS);

afterAll(async () => {
  if (process.platform === "win32" && fakeGodotExe !== undefined) {
    await killImage(path.basename(fakeGodotExe));
  }
  if (fakeGodotDir !== undefined) {
    await removeDirWithRetry(fakeGodotDir);
  }
}, TEST_TIMEOUT_MS);

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), `${RUN_TAG}-`));
  prevCliHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  prevHome = process.env.GODOT_AUTOPILOT_HOME;
  prevMulti = process.env.GODOT_AUTOPILOT_ALLOW_MULTI_INSTANCE;
  prevMode = process.env.GDA_FAKE_GODOT_MODE;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmp;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
  delete process.env.GODOT_AUTOPILOT_ALLOW_MULTI_INSTANCE;
  delete process.env.GDA_FAKE_GODOT_MODE;
  editorPids = [];
  strayPids = [];
});

afterEach(async () => {
  try {
    for (const entry of registeredInstances()) {
      await killPid(entry.pid);
    }
    for (const pid of [...editorPids, ...strayPids]) {
      await killPid(pid);
    }
  } catch (error) {
    process.stderr.write(`[cleanup] restart cleanup failed: ${String(error)}\n`);
  } finally {
    restoreEnv("GODOT_AUTOPILOT_CLI_HOME", prevCliHome);
    restoreEnv("GODOT_AUTOPILOT_HOME", prevHome);
    restoreEnv("GODOT_AUTOPILOT_ALLOW_MULTI_INSTANCE", prevMulti);
    restoreEnv("GDA_FAKE_GODOT_MODE", prevMode);
    await removeDirWithRetry(tmp);
  }
}, TEST_TIMEOUT_MS);

function restoreEnv(name: string, value: string | undefined): void {
  if (value === undefined) delete process.env[name];
  else process.env[name] = value;
}

function registeredInstances(): InstanceEntry[] {
  try {
    return loadRegistry().instances;
  } catch {
    return [];
  }
}

function makeProjectDir(): string {
  const dir = path.join(tmp, "proj");
  fs.mkdirSync(dir, { recursive: true });
  return dir;
}

function findFreePort(): Promise<number> {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () => {
      const address = server.address();
      if (address === null || typeof address === "string") {
        server.close(() => reject(new Error("no address")));
        return;
      }
      server.close(() => resolve(address.port));
    });
  });
}

function makeEntry(params: { project: string; port: number; pid: number; godot?: string; headless?: boolean }): InstanceEntry {
  return {
    id: `p${params.pid}`,
    project: params.project,
    port: params.port,
    pid: params.pid,
    startedAt: new Date().toISOString(),
    headless: params.headless ?? true,
    godot: params.godot ?? fakeGodotExe!,
    log: path.join(tmp, `editor-${params.pid}.log`),
  };
}

function writeEntry(entry: InstanceEntry): void {
  saveRegistry({ version: 1, instances: [entry] });
}

async function startEditor(projectDir: string, port: number): Promise<number> {
  const result = await spawnEditor({
    projectDir,
    port,
    headless: true,
    godot: fakeGodotExe!,
    logPath: path.join(tmp, `old-${Date.now()}.log`),
  });
  editorPids.push(result.pid);
  return result.pid;
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

async function stubbornProcess(): Promise<number> {
  const child = spawn(
    process.execPath,
    ["-e", "process.on('SIGTERM',()=>{});console.log('ready');setTimeout(()=>{},30000)"],
    { stdio: ["ignore", "pipe", "ignore"] },
  );
  const pid = child.pid;
  if (pid === undefined) throw new Error("failed to spawn the stubborn helper");
  await new Promise<void>((resolve, reject) => {
    if (child.stdout === null) {
      reject(new Error("stubborn helper stdout is unavailable"));
      return;
    }
    child.stdout.once("data", () => resolve());
    child.once("error", (error) => reject(error));
    child.once("exit", (code) => reject(new Error(`stubborn helper exited early: ${code}`)));
  });
  strayPids.push(pid);
  return pid;
}

describe.serial("restart", () => {
  test("重启成功：停止旧实例并按原端口、原 headless、原 godot 重新启动", async () => {
    const projectDir = makeProjectDir();
    const port = await findFreePort();
    const oldPid = await startEditor(projectDir, port);
    writeEntry(makeEntry({ project: projectDir, port, pid: oldPid }));

    const { code, stdout } = await captureStdout(() =>
      runRestart([`p${oldPid}`], {
        noImport: true,
        waitText: "10",
      }),
    );

    expect(code).toBe(ExitCode.Success);
    const result = JSON.parse(stdout);
    expect(result.ok).toBe(true);
    expect(result.restarted).toBe(true);
    expect(result.previous).toEqual({ id: `p${oldPid}`, pid: oldPid, port });
    expect(result.port).toBe(port);
    expect(result.project).toBe(path.resolve(projectDir));
    expect(result.headless).toBe(true);
    expect(result.id).toBe(`p${result.pid}`);
    expect(result.id).not.toBe(`p${oldPid}`);
    expect(result.live).toBe(true);
    editorPids.push(result.pid);

    expect(isAlive(oldPid)).toBe(false);
    const instances = loadRegistry().instances;
    expect(instances).toHaveLength(1);
    expect(instances[0].id).toBe(result.id);
    expect(instances[0].port).toBe(port);
    expect(instances[0].headless).toBe(true);
    expect(instances[0].godot).toBe(fakeGodotExe);
  }, TEST_TIMEOUT_MS);

  test("停止超时：不启动新实例并保留旧登记", async () => {
    const projectDir = makeProjectDir();
    const port = await findFreePort();
    const pid = await stubbornProcess();
    writeEntry(makeEntry({ project: projectDir, port, pid, headless: false }));

    const { code, stdout, stderr } = await captureStdout(() =>
      runRestart([`p${pid}`], { noImport: true, waitText: "1" }),
    );

    expect(code).toBe(ExitCode.Transport);
    expect(stdout.trim()).toBe("");
    expect(stderr).toContain(`instance p${pid} (pid ${pid}) is still running after 10s`);
    expect(stderr).toContain("refusing to start a replacement");
    expect(isAlive(pid)).toBe(true);
    expect(loadRegistry().instances).toHaveLength(1);
  }, 30_000);

  test("启动失败：回滚新进程并提示原实例已停止", async () => {
    const projectDir = makeProjectDir();
    const port = await findFreePort();
    const oldPid = await startEditor(projectDir, port);
    writeEntry(makeEntry({ project: projectDir, port, pid: oldPid }));
    process.env.GDA_FAKE_GODOT_MODE = "exit";

    const { code, stdout, stderr } = await captureStdout(() =>
      runRestart([`p${oldPid}`], { force: true, noImport: true, waitText: "3" }),
    );

    expect(code).toBe(ExitCode.Transport);
    expect(stdout.trim()).toBe("");
    expect(stderr).toMatch(/before becoming ready|not ready within/);
    expect(stderr).toContain(`the original instance p${oldPid} (pid ${oldPid}) was already stopped`);
    expect(isAlive(oldPid)).toBe(false);
    expect(loadRegistry().instances).toHaveLength(0);
  }, TEST_TIMEOUT_MS);

  test("--force 强杀忽略优雅信号的旧实例后重启", async () => {
    const projectDir = makeProjectDir();
    const port = await findFreePort();
    const pid = await stubbornProcess();
    writeEntry(makeEntry({ project: projectDir, port, pid }));

    const { code, stdout } = await captureStdout(() =>
      runRestart([`p${pid}`], { force: true, noImport: true, waitText: "10" }),
    );

    expect(code).toBe(ExitCode.Success);
    const result = JSON.parse(stdout);
    expect(result.previous.pid).toBe(pid);
    expect(result.pid).not.toBe(pid);
    expect(result.port).toBe(port);
    editorPids.push(result.pid);
    expect(isAlive(pid)).toBe(false);
    expect(loadRegistry().instances).toHaveLength(1);
  }, TEST_TIMEOUT_MS);
});
