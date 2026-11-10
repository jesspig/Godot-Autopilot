import fs from "node:fs";
import path from "node:path";

import { gdaRoot } from "./platform";
import { isAlive } from "./process";

export interface ProjectSummary {
  name: string;
  path: string;
  hash: string;
  instances: number;
  lastActive: string;
  live: boolean;
}

interface InstanceRecord {
  pid?: unknown;
  project_path?: unknown;
  started_at?: unknown;
}

function readRecords(instancesDir: string): InstanceRecord[] {
  let names: string[];
  try {
    names = fs.readdirSync(instancesDir);
  } catch {
    return [];
  }
  const records: InstanceRecord[] = [];
  for (const name of names) {
    if (!name.endsWith(".json")) continue;
    try {
      const parsed = JSON.parse(fs.readFileSync(path.join(instancesDir, name), "utf8")) as unknown;
      if (typeof parsed === "object" && parsed !== null && !Array.isArray(parsed)) {
        records.push(parsed as InstanceRecord);
      }
    } catch {
      continue;
    }
  }
  return records;
}

function newestFileMtime(dir: string): number {
  let newest = 0;
  let entries: fs.Dirent[];
  try {
    entries = fs.readdirSync(dir, { withFileTypes: true });
  } catch {
    return 0;
  }
  for (const entry of entries) {
    const full = path.join(dir, entry.name);
    try {
      if (entry.isDirectory()) {
        newest = Math.max(newest, newestFileMtime(full));
      } else if (entry.isFile()) {
        newest = Math.max(newest, fs.statSync(full).mtimeMs);
      }
    } catch {
      continue;
    }
  }
  return newest;
}

export function listProjects(): ProjectSummary[] {
  const projectsRoot = path.join(gdaRoot(), "projects");
  let entries: fs.Dirent[];
  try {
    entries = fs.readdirSync(projectsRoot, { withFileTypes: true });
  } catch {
    return [];
  }
  const projects: ProjectSummary[] = [];
  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    const dir = path.join(projectsRoot, entry.name);
    const dash = entry.name.lastIndexOf("-");
    const hash = dash >= 0 ? entry.name.slice(dash + 1) : "";
    const dirName = dash >= 0 ? entry.name.slice(0, dash) : entry.name;
    const records = readRecords(path.join(dir, "instances"));
    const projectPath = records
      .map((record) => record.project_path)
      .find((value): value is string => typeof value === "string" && value !== "");
    const newestStart = records.reduce(
      (max, record) =>
        typeof record.started_at === "number" && record.started_at > max ? record.started_at : max,
      0,
    );
    const last = Math.max(newestStart, Math.round(newestFileMtime(dir)));
    projects.push({
      name: projectPath !== undefined ? path.basename(path.resolve(projectPath)) : dirName,
      path: projectPath !== undefined ? path.resolve(projectPath) : "",
      hash,
      instances: records.length,
      lastActive: last > 0 ? new Date(last).toISOString() : "",
      live: records.some((record) => typeof record.pid === "number" && isAlive(record.pid)),
    });
  }
  projects.sort((a, b) => a.name.localeCompare(b.name));
  return projects;
}