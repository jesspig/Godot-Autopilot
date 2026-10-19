import { spawn, spawnSync } from "node:child_process";
import type { ChildProcess } from "node:child_process";
import fs from "node:fs";
import path from "node:path";

import { CliError, ExitCode } from "./errors";
import { joinPath, resolvePath } from "./platform";

const TERMINATE_CONFIRM_MS = 2000;
const TERMINATE_POLL_MS = 100;

export type TerminationOutcome = "already-exited" | "terminated" | "failed";

export function isAlive(pid: number): boolean {
  if (process.platform === "win32") {
    return windowsProcessExists(pid);
  }
  try {
    process.kill(pid, 0);
    return true;
  } catch {
    return false;
  }
}

function windowsProcessExists(pid: number): boolean {
  const r = spawnSync("tasklist", ["/FI", `PID eq ${pid}`, "/NH", "/FO", "CSV"]);
  if (r.error !== undefined || r.status !== 0) {
    // 进程表查不到时不能断言进程已退出，否则回滚会跳过终止动作而留下孤儿
    return true;
  }
  const out = r.stdout?.toString() ?? "";
  return out.split(/\r?\n/).some((line) => csvPidField(line) === pid);
}

function csvPidField(line: string): number | null {
  const m = /^"[^"]*","(\d+)"/.exec(line.trim());
  return m === null ? null : Number(m[1]);
}

export function resolveGodotBin(opts: { explicit?: string }): { bin: string; source: string } {
  if (opts.explicit) {
    return { bin: resolvePath(opts.explicit), source: "explicit" };
  }
  const fromPath = searchPath();
  if (fromPath !== null) {
    return { bin: fromPath, source: "path" };
  }
  throw new CliError(ExitCode.Transport, "cannot locate the Godot executable (pass --godot or put godot on PATH)");
}

function searchPath(): string | null {
  const names = process.platform === "win32" ? ["godot.exe", "godot4.exe"] : ["godot", "godot4"];
  const dirs = (process.env.PATH ?? "").split(path.delimiter);
  for (const dir of dirs) {
    if (dir.length === 0) continue;
    for (const name of names) {
      const full = joinPath(dir, name);
      try {
        if (fs.statSync(full).isFile()) return full;
      } catch {
      }
    }
  }
  return null;
}

export async function spawnEditor(opts: { projectDir: string; port: number; headless: boolean; godot: string; logPath: string }): Promise<{ pid: number }> {
  const args = opts.headless
    ? [opts.godot, "--headless", "--editor", "--path", resolvePath(opts.projectDir)]
    : [opts.godot, "--editor", "--path", resolvePath(opts.projectDir)];
  const logFd = fs.openSync(resolvePath(opts.logPath), "a");
  const env = {
    ...process.env,
    GODOT_AUTOPILOT_PORT: String(opts.port),
    ...(opts.headless ? { GDA_FORCE_HEADLESS: "1" } : {}),
  };
  try {
    const [exe, ...rest] = args;
    const child = spawn(exe, rest, {
      stdio: ["ignore", logFd, logFd],
      env,
      // Windows 上必须脱父：Bun 的直系子进程随 CLI 退出被作业对象连带杀掉，
      // detached 让编辑器脱离父进程与控制台，spawn 返回的即编辑器自身 pid
      detached: process.platform === "win32",
    });
    if (child.pid === undefined) {
      const reason = await spawnFailureReason(child);
      throw new CliError(
        ExitCode.Transport,
        `failed to launch the editor executable: ${reason}`,
        "check the --godot path points at a runnable Godot binary",
      );
    }
    if (process.platform === "win32") {
      child.unref();
    }
    return { pid: child.pid };
  } finally {
    fs.closeSync(logFd);
  }
}

function spawnFailureReason(child: ChildProcess): Promise<string> {
  return new Promise((resolve) => {
    child.once("error", (error: Error) => resolve(error.message));
    child.once("exit", (code) => resolve(`the process exited with code ${code ?? "unknown"} before it started`));
  });
}

export async function terminateProcess(pid: number): Promise<TerminationOutcome> {
  if (!isAlive(pid)) return "already-exited";
  stopForce(pid);
  const deadline = Date.now() + TERMINATE_CONFIRM_MS;
  for (;;) {
    if (!isAlive(pid)) return "terminated";
    if (Date.now() >= deadline) return "failed";
    await Bun.sleep(TERMINATE_POLL_MS);
  }
}

export function terminationNote(outcome: TerminationOutcome, pid: number): string {
  switch (outcome) {
    case "already-exited":
      return `the editor process (pid ${pid}) had already exited`;
    case "terminated":
      return `the editor process (pid ${pid}) was terminated`;
    case "failed":
      return `the editor process (pid ${pid}) could not be terminated, stop it manually: ${manualKillCommand(pid)}`;
  }
}

export function manualKillCommand(pid: number): string {
  return process.platform === "win32" ? `taskkill /F /T /PID ${pid}` : `kill -9 ${pid}`;
}

export function stopGraceful(pid: number): { ok: boolean } {
  if (process.platform === "win32") {
    const r = spawnSync("taskkill", ["/PID", String(pid)]);
    return { ok: r.status === 0 };
  }
  try {
    process.kill(pid, "SIGTERM");
    return { ok: true };
  } catch {
    return { ok: false };
  }
}

export function stopForce(pid: number): { ok: boolean } {
  if (process.platform === "win32") {
    const r = spawnSync("taskkill", ["/F", "/T", "/PID", String(pid)]);
    return { ok: r.status === 0 };
  }
  try {
    process.kill(pid, "SIGKILL");
    return { ok: true };
  } catch {
    return { ok: false };
  }
}
