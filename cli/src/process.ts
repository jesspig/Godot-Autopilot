import { spawn, spawnSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";

import { CliError, ExitCode } from "./errors";
import { joinPath, readTextFile, resolvePath } from "./platform";

export function isAlive(pid: number): boolean {
  if (process.platform === "win32") {
    const r = spawnSync("tasklist", ["/FI", `PID eq ${pid}`, "/NH"]);
    const out = r.stdout?.toString() ?? "";
    return r.status === 0 && out.trim().length > 0 && !/^(INFO|消息)/.test(out.trim());
  }
  try {
    process.kill(pid, 0);
    return true;
  } catch {
    return false;
  }
}

export function resolveGodotBin(opts: { explicit?: string; projectDir?: string }): { bin: string; source: string } {
  if (opts.explicit) {
    return { bin: resolvePath(opts.explicit), source: "explicit" };
  }
  const envBin = process.env.GODOT_PATH;
  if (envBin) {
    return { bin: resolvePath(envBin), source: "env" };
  }
  if (opts.projectDir) {
    const envFile = joinPath(resolvePath(opts.projectDir), ".env");
    try {
      const m = readTextFile(envFile).match(/^GODOT_PATH=(.+)$/m);
      if (m && m[1].trim().length > 0) {
        return { bin: resolvePath(m[1].trim()), source: "env-file" };
      }
    } catch {
    }
  }
  const fromPath = searchPath();
  if (fromPath !== null) {
    return { bin: fromPath, source: "path" };
  }
  throw new CliError(ExitCode.Transport, "cannot locate the Godot executable (use --godot, GODOT_PATH, .env GODOT_PATH, or PATH)");
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

export function spawnEditor(opts: { projectDir: string; port: number; headless: boolean; godot: string; logPath: string }): { pid: number } {
  const args = opts.headless
    ? [opts.godot, "--headless", "--editor", "--path", resolvePath(opts.projectDir)]
    : [opts.godot, "--editor", "--path", resolvePath(opts.projectDir)];
  const logFd = fs.openSync(resolvePath(opts.logPath), "a");
  const child = spawn(args[0], args.slice(1), {
    stdio: ["ignore", logFd, logFd],
    env: {
      ...process.env,
      GODOT_AUTOPILOT_PORT: String(opts.port),
      ...(opts.headless ? { GDA_FORCE_HEADLESS: "1" } : {}),
    },
  });
  fs.closeSync(logFd);
  if (child.pid === undefined) {
    throw new CliError(ExitCode.Transport, "failed to spawn the editor process");
  }
  return { pid: child.pid };
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
