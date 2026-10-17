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

export async function spawnEditor(opts: { projectDir: string; port: number; headless: boolean; godot: string; logPath: string; timeoutMs?: number }): Promise<{ pid: number }> {
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
    if (process.platform === "win32") {
      const child = spawn("cmd", ["/c", "start", "", "/b", ...args], {
        stdio: ["ignore", logFd, logFd],
        env,
      });
      if (child.pid === undefined) {
        throw new CliError(ExitCode.Transport, "failed to spawn the editor process");
      }
      const godotPid = await resolvePidByPort(opts.port, opts.timeoutMs ?? 10_000);
      return { pid: godotPid };
    }
    const child = spawn(args[0], args.slice(1), {
      stdio: ["ignore", logFd, logFd],
      env,
    });
    if (child.pid === undefined) {
      throw new CliError(ExitCode.Transport, "failed to spawn the editor process");
    }
    return { pid: child.pid };
  } finally {
    fs.closeSync(logFd);
  }
}

async function resolvePidByPort(port: number, timeoutMs: number): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  const portPattern = new RegExp(`:${port}\\b`);
  for (;;) {
    const r = spawnSync("netstat", ["-ano", "-p", "tcp"], { encoding: "utf8" });
    const out = r.stdout ?? "";
    for (const line of out.split("\n")) {
      if (portPattern.test(line) && line.includes("LISTENING")) {
        const pid = parseInt(line.trim().split(/\s/).pop() ?? "", 10);
        if (!isNaN(pid) && pid > 0) {
          return pid;
        }
      }
    }
    if (Date.now() >= deadline) {
      throw new CliError(ExitCode.Transport, "failed to resolve editor pid from listening port");
    }
    await Bun.sleep(200);
  }
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
