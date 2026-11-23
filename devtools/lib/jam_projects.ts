import fs from "node:fs";
import path from "node:path";

export const JAM_METADATA_FILE = "jam.json";

const SKIPPED_DIRECTORIES = new Set([".git", ".godot", "addons", "build", "node_modules"]);

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

export function readJamMetadata(dir: string): Record<string, unknown> | null {
  const metadataPath = path.join(dir, JAM_METADATA_FILE);
  if (!fs.existsSync(metadataPath)) return null;
  try {
    const parsed: unknown = JSON.parse(fs.readFileSync(metadataPath, "utf8"));
    return isRecord(parsed) ? parsed : null;
  } catch {
    return null;
  }
}

export function isJamIntake(dir: string): boolean {
  return readJamMetadata(dir)?.kind === "intake";
}

export function isE2eProject(dir: string): boolean {
  return readJamMetadata(dir)?.kind === "e2e";
}

function isGodotProject(dir: string): boolean {
  return fs.existsSync(path.join(dir, "project.godot"));
}

export function discoverProjectsIn(root: string): string[] {
  const projects: string[] = [];
  const walk = (dir: string, depth: number): void => {
    if (depth > 4) return;
    if (isGodotProject(dir)) {
      if (!isJamIntake(dir) && !isE2eProject(dir)) projects.push(dir);
      return;
    }
    let entries: fs.Dirent[];
    try {
      entries = fs.readdirSync(dir, { withFileTypes: true });
    } catch {
      return;
    }
    for (const entry of entries) {
      if (!entry.isDirectory() || entry.isSymbolicLink()) continue;
      if (SKIPPED_DIRECTORIES.has(entry.name)) continue;
      walk(path.join(dir, entry.name), depth + 1);
    }
  };
  walk(root, 0);
  return projects.sort();
}
