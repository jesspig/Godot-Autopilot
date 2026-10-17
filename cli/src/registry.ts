import fs from "node:fs";

import { CliError, ExitCode } from "./errors";
import { isAlive } from "./process";
import { joinPath, resolvePath, sep } from "./platform";

export interface InstanceEntry {
  id: string;
  project: string;
  port: number;
  pid: number;
  startedAt: string;
  headless: boolean;
  godot: string;
  log: string;
}

export interface RegistryData {
  version: number;
  instances: InstanceEntry[];
}

const LOCK_RETRY_INTERVAL_MS = 50;
const LOCK_RETRY_TOTAL_MS = 2000;
const LOCK_STALE_MS = 10000;

export function stateDir(): string {
  const override = process.env.GODOT_AUTOPILOT_CLI_HOME;
  if (override) return override;
  if (process.platform === "win32") {
    const base = process.env.LOCALAPPDATA;
    if (base) return joinPath(base, "godot-autopilot");
  } else {
    const xdg = process.env.XDG_STATE_HOME;
    if (xdg) return joinPath(xdg, "godot-autopilot");
    const home = process.env.HOME || process.env.USERPROFILE;
    if (home) return joinPath(home, ".local", "state", "godot-autopilot");
  }
  throw new CliError(ExitCode.Transport, "cannot determine the CLI state directory (set GODOT_AUTOPILOT_CLI_HOME)");
}

function registryPath(): string {
  return joinPath(stateDir(), "instances.json");
}

function lockPath(): string {
  return joinPath(stateDir(), "instances.lock");
}

export function withLock<T>(fn: () => T): T {
  fs.mkdirSync(stateDir(), { recursive: true });
  const lock = lockPath();
  const deadline = Date.now() + LOCK_RETRY_TOTAL_MS;
  for (;;) {
    try {
      const fd = fs.openSync(lock, "wx");
      fs.writeFileSync(fd, String(process.pid));
      fs.closeSync(fd);
      try {
        return fn();
      } finally {
        fs.rmSync(lock, { force: true });
      }
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code !== "EEXIST") throw error;
      if (Date.now() >= deadline) {
        if (tryStealExpiredLock(lock)) continue;
        throw new CliError(ExitCode.Transport, "registry lock busy");
      }
      sleepMs(LOCK_RETRY_INTERVAL_MS);
    }
  }
}

function tryStealExpiredLock(lock: string): boolean {
  try {
    const stat = fs.statSync(lock);
    if (Date.now() - stat.mtimeMs < LOCK_STALE_MS) return false;
    const pid = Number(fs.readFileSync(lock, "utf8").trim());
    if (Number.isInteger(pid) && pid > 0 && isAlive(pid)) return false;
    fs.rmSync(lock);
    return true;
  } catch {
    return false;
  }
}

function sleepMs(ms: number): void {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

export function loadRegistry(): RegistryData {
  return withLock(() => {
    const p = registryPath();
    let raw: string;
    try {
      raw = fs.readFileSync(p, "utf8");
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code === "ENOENT") {
        return { version: 1, instances: [] };
      }
      throw error;
    }
    let parsed: unknown;
    try {
      parsed = JSON.parse(raw);
    } catch {
      throw new CliError(ExitCode.Transport, `cannot parse instances.json at ${p}`);
    }
    if (typeof parsed !== "object" || parsed === null || !Array.isArray((parsed as { instances?: unknown }).instances)) {
      throw new CliError(ExitCode.Transport, `cannot parse instances.json at ${p}`);
    }
    const data = parsed as RegistryData;
    const alive = data.instances.filter((e) => isAlive(e.pid));
    if (alive.length !== data.instances.length) {
      data.instances = alive;
      writeRegistryAtomic(data);
    }
    return data;
  });
}

export function saveRegistry(data: RegistryData): void {
  withLock(() => {
    writeRegistryAtomic(data);
  });
}

function writeRegistryAtomic(data: RegistryData): void {
  const p = registryPath();
  const tmp = `${p}.${process.pid}.tmp`;
  fs.writeFileSync(tmp, JSON.stringify(data, null, 2));
  fs.renameSync(tmp, p);
}

export function findById(id: string): InstanceEntry | undefined {
  return loadRegistry().instances.find((e) => e.id === id);
}

export function findLiveByProject(project: string): InstanceEntry | undefined {
  const target = normalizeProjectPath(project);
  return loadRegistry().instances.find((e) => normalizeProjectPath(e.project) === target && isAlive(e.pid));
}

function normalizeProjectPath(p: string): string {
  let n = resolvePath(p).replace(/\//g, sep());
  if (process.platform === "win32") n = n.toLowerCase();
  return n;
}
