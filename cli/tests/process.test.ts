import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { spawn } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { isAlive, resolveGodotBin, spawnEditor, stopForce, terminateProcess, terminationNote } from "../src/process";
import { waitFor } from "./support/cleanup";

let tempDirs: string[] = [];

afterEach(() => {
  for (const dir of tempDirs) fs.rmSync(dir, { recursive: true, force: true });
  tempDirs = [];
});

function spawnSleeper(): number {
  const child = spawn(process.execPath, ["-e", "setTimeout(()=>{},60000)"], { stdio: "ignore" });
  if (child.pid === undefined) throw new Error("failed to spawn the sleeper process");
  return child.pid;
}

function spawnExiting(): Promise<number> {
  return new Promise((resolve) => {
    const child = spawn(process.execPath, ["-e", "process.exit(0)"], { stdio: "ignore" });
    const pid = child.pid ?? 0;
    child.on("exit", () => resolve(pid));
  });
}

describe("resolveGodotBin", () => {
  let savedPath: string | undefined;
  let savedGodotPath: string | undefined;

  beforeEach(() => {
    savedPath = process.env.PATH;
    savedGodotPath = process.env.GODOT_PATH;
    process.env.PATH = "";
    delete process.env.GODOT_PATH;
  });

  afterEach(() => {
    if (savedPath === undefined) delete process.env.PATH;
    else process.env.PATH = savedPath;
    if (savedGodotPath === undefined) delete process.env.GODOT_PATH;
    else process.env.GODOT_PATH = savedGodotPath;
  });

  test("ignores GODOT_PATH which is reserved for the L2 runner", () => {
    process.env.GODOT_PATH = path.join(os.tmpdir(), "gda-missing-godot", "godot.exe");
    expect(() => resolveGodotBin({})).toThrow(/--godot/);
  });

  test("explicit --godot wins without touching the environment", () => {
    process.env.GODOT_PATH = path.join(os.tmpdir(), "gda-missing-godot", "godot.exe");
    const r = resolveGodotBin({ explicit: "custom-godot" });
    expect(r.source).toBe("explicit");
    expect(r.bin.endsWith("custom-godot")).toBe(true);
  });

  test("falls back to PATH lookup", () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-godot-path-"));
    tempDirs.push(dir);
    const name = process.platform === "win32" ? "godot.exe" : "godot";
    fs.writeFileSync(path.join(dir, name), "");
    process.env.PATH = dir;
    const r = resolveGodotBin({});
    expect(r.source).toBe("path");
    expect(r.bin.endsWith(name)).toBe(true);
  });
});

describe("isAlive", () => {
  test("存活进程为 true、已退出进程为 false（与 tasklist 本地化文案无关）", async () => {
    const livePid = spawnSleeper();
    try {
      expect(isAlive(livePid)).toBe(true);
    } finally {
      stopForce(livePid);
    }
    const deadPid = await spawnExiting();
    expect(await waitFor(() => !isAlive(deadPid), 5000)).toBe(true);
  });
});

describe("terminateProcess", () => {
  test("已退出进程返回 already-exited 且不误杀", async () => {
    const pid = await spawnExiting();
    expect(await waitFor(() => !isAlive(pid), 5000)).toBe(true);
    expect(await terminateProcess(pid)).toBe("already-exited");
  });

  test("存活进程被终止且确认消失", async () => {
    const pid = spawnSleeper();
    expect(await terminateProcess(pid)).toBe("terminated");
    expect(isAlive(pid)).toBe(false);
  });
});

describe("terminationNote", () => {
  test("三种结局都带 pid，失败结局带可执行补救指令", () => {
    expect(terminationNote("terminated", 4242)).toBe("the editor process (pid 4242) was terminated");
    expect(terminationNote("already-exited", 4242)).toBe("the editor process (pid 4242) had already exited");
    const failed = terminationNote("failed", 4242);
    expect(failed).toContain("pid 4242");
    expect(failed).toContain("could not be terminated");
    expect(failed).toContain(process.platform === "win32" ? "taskkill /F /T /PID 4242" : "kill -9 4242");
  });
});

describe("spawnEditor", () => {
  test("引擎不可执行时报可诊断错误而非静默失败", async () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-missing-bin-"));
    tempDirs.push(dir);
    const missing = path.join(dir, process.platform === "win32" ? "godot.exe" : "godot");
    await expect(
      spawnEditor({
        projectDir: dir,
        port: 9599,
        headless: true,
        godot: missing,
        logPath: path.join(dir, "editor.log"),
      }),
    ).rejects.toThrow(/failed to launch the editor executable/);
  });
});
