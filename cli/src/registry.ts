import fs from "node:fs";

import { CliError, ExitCode } from "./errors";
import { isAlive } from "./process";
import { gdaRoot, joinPath } from "./platform";
import { projectPathKey } from "./target";

export interface InstanceEntry {
  id: string;
  project: string;
  port: number;
  pid: number;
  startedAt: string;
  headless: boolean;
  godot: string;
  log: string;
  projectPath?: string;
  instancePort?: number;
  apiAvailable?: boolean;
}

export interface RegistryData {
  version: number;
  instances: InstanceEntry[];
}

const LOCK_RETRY_INTERVAL_MS = 50;
const LOCK_RETRY_TOTAL_MS = 2000;
const LOCK_STALE_MS = 10000;
const CLI_STATE_DIR_NAME = "cli";

export function stateDir(): string {
  const override = process.env.GODOT_AUTOPILOT_CLI_HOME;
  if (override !== undefined && override.trim() !== "") return override;
  try {
    return joinPath(gdaRoot(), CLI_STATE_DIR_NAME);
  } catch {
    throw new CliError(ExitCode.Transport, "cannot determine the GDA home directory (set GODOT_AUTOPILOT_HOME)");
  }
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
    removeOrphanTmpFiles();
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
    data.instances = data.instances.map(normalizeEntry);
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

export function normalizeEntry(entry: InstanceEntry): InstanceEntry {
  return {
    ...entry,
    projectPath: entry.projectPath ?? entry.project,
    instancePort: entry.instancePort ?? entry.port,
    apiAvailable: entry.apiAvailable ?? false,
  };
}

function writeRegistryAtomic(data: RegistryData): void {
  const p = registryPath();
  fs.mkdirSync(stateDir(), { recursive: true });
  const tmp = `${p}.${process.pid}.tmp`;
  fs.writeFileSync(tmp, JSON.stringify(data, null, 2));
  fs.renameSync(tmp, p);
}

const ORPHAN_TMP_PATTERN = /^instances\.json\.(\d+)\.tmp$/;

function removeOrphanTmpFiles(): void {
  const dir = stateDir();
  for (const name of fs.readdirSync(dir)) {
    const m = ORPHAN_TMP_PATTERN.exec(name);
    if (m === null) continue;
    if (isAlive(Number(m[1]))) continue;
    fs.rmSync(joinPath(dir, name), { force: true });
  }
}

export function findById(id: string): InstanceEntry | undefined {
  return loadRegistry().instances.find((e) => e.id === id);
}

export function findLiveByProject(project: string): InstanceEntry | undefined {
  const target = projectPathKey(project);
  return loadRegistry().instances.find((e) => projectPathKey(e.project) === target && isAlive(e.pid));
}
