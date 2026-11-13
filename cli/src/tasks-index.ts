import { createHash } from "node:crypto";
import fs from "node:fs";
import path from "node:path";

import { gdaRoot, joinPath } from "./platform";

export interface TaskRecord {
  task_id: string;
  tool: string;
  args_hash: string;
  started_at: string;
  port: number;
  instance_id: string;
  status: string;
}

export interface TaskIndexEntry extends TaskRecord {
  file: string;
}

export function tasksDir(): string {
  return joinPath(gdaRoot(), "tasks");
}

export function argsHash(args: unknown): string {
  return createHash("sha256").update(JSON.stringify(args)).digest("hex").slice(0, 16);
}

export function recordTask(record: TaskRecord): void {
  const dir = tasksDir();
  fs.mkdirSync(dir, { recursive: true });
  const target = joinPath(dir, `${record.task_id}.json`);
  const tmp = `${target}.${process.pid}.tmp`;
  fs.writeFileSync(tmp, `${JSON.stringify(record, null, 2)}\n`, "utf8");
  fs.renameSync(tmp, target);
}

export function listTasks(): TaskIndexEntry[] {
  let names: string[];
  try {
    names = fs.readdirSync(tasksDir()).filter((name) => name.endsWith(".json"));
  } catch {
    return [];
  }
  const entries: TaskIndexEntry[] = [];
  for (const name of names) {
    const file = path.join(tasksDir(), name);
    try {
      const parsed = JSON.parse(fs.readFileSync(file, "utf8")) as TaskRecord;
      if (typeof parsed.task_id !== "string" || typeof parsed.tool !== "string") continue;
      entries.push({ ...parsed, file });
    } catch {
    }
  }
  return entries.sort((a, b) => b.started_at.localeCompare(a.started_at));
}

export function removeTask(taskId: string): boolean {
  const target = joinPath(tasksDir(), `${taskId}.json`);
  try {
    fs.rmSync(target, { force: true });
    return true;
  } catch {
    return false;
  }
}

export function isTerminalStatus(status: string): boolean {
  return status === "completed" || status === "failed" || status === "cancelled";
}
