import fs from "node:fs";
import path from "node:path";

import { gdaRoot, projectPathKey } from "../platform";
import type { InstanceEntry } from "../registry";

export const TASK_STORE_POISON_SIGNATURE = "task store: failed to persist";

const LOG_TAIL_BYTES = 65536;
const POISONING_DIAGNOSIS =
  "the task store looks poisoned (a persist failure now repeats on every call); this matches the SDK FileTaskStore compaction bug on Windows, not a transient disk error";
const FORCE_RISK = "retry with --force (this discards unsaved scene edits)";
const BASE_GRACEFUL_STOP_HINT = "the process is still running; retry with --force";
const BASE_RESTART_SUGGESTION =
  "retry with --force, or stop it manually before running gda restart again";
const FORCE_STOP_HINT = "the process survived a force stop";

export function detectTaskStorePoisoning(text: string): boolean {
  return text.includes(TASK_STORE_POISON_SIGNATURE);
}

export function gracefulStopHint(poisoned: boolean): string {
  return poisoned ? `${POISONING_DIAGNOSIS}; ${FORCE_RISK}` : BASE_GRACEFUL_STOP_HINT;
}

export function stopFailureHint(method: "force" | "graceful", poisoned: boolean): string {
  return method === "force" ? FORCE_STOP_HINT : gracefulStopHint(poisoned);
}

export function gracefulRestartSuggestion(poisoned: boolean): string {
  return poisoned
    ? `${POISONING_DIAGNOSIS}; ${FORCE_RISK}, or stop it manually before running gda restart again`
    : BASE_RESTART_SUGGESTION;
}

export function collectTaskStorePoisoning(entry: InstanceEntry): boolean {
  const logsDir = projectLogsDir(entry.project);
  if (logsDir === undefined) return false;
  return detectTaskStorePoisoning(readNewestLogTail(logsDir));
}

function projectLogsDir(projectPath: string): string | undefined {
  const key = projectPathKey(projectPath);
  const projectsRoot = path.join(gdaRoot(), "projects");
  let entries: fs.Dirent[];
  try {
    entries = fs.readdirSync(projectsRoot, { withFileTypes: true });
  } catch {
    return undefined;
  }
  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    const projectDir = path.join(projectsRoot, entry.name);
    if (instanceDirMatchesProject(path.join(projectDir, "instances"), key)) {
      return path.join(projectDir, "logs");
    }
  }
  return undefined;
}

function instanceDirMatchesProject(instancesDir: string, key: string): boolean {
  let names: string[];
  try {
    names = fs.readdirSync(instancesDir);
  } catch {
    return false;
  }
  for (const name of names) {
    if (!name.endsWith(".json")) continue;
    try {
      const parsed = JSON.parse(
        fs.readFileSync(path.join(instancesDir, name), "utf8"),
      ) as { project_path?: unknown };
      if (typeof parsed.project_path === "string" && projectPathKey(parsed.project_path) === key) {
        return true;
      }
    } catch {
      continue;
    }
  }
  return false;
}

function readNewestLogTail(logsDir: string): string {
  let newest: { path: string; mtime: number } | undefined;
  let names: string[];
  try {
    names = fs.readdirSync(logsDir);
  } catch {
    return "";
  }
  for (const name of names) {
    if (!name.startsWith("gda-") || !name.endsWith(".log")) continue;
    const full = path.join(logsDir, name);
    try {
      const mtime = fs.statSync(full).mtimeMs;
      if (newest === undefined || mtime > newest.mtime) {
        newest = { path: full, mtime };
      }
    } catch {
      continue;
    }
  }
  if (newest === undefined) return "";
  try {
    const fd = fs.openSync(newest.path, "r");
    try {
      const size = fs.fstatSync(fd).size;
      const length = Math.min(size, LOG_TAIL_BYTES);
      const buffer = Buffer.alloc(length);
      fs.readSync(fd, buffer, 0, length, size - length);
      return buffer.toString("utf8");
    } finally {
      fs.closeSync(fd);
    }
  } catch {
    return "";
  }
}