import fs from "node:fs";
import path from "node:path";

import { addonDir, resolveProjects } from "../lib/config";
import { errorMessage } from "../lib/cmake";

function removeQuietly(target: string): void {
  try {
    fs.rmSync(target, { recursive: true, force: true });
  } catch {}
}

export function cleanTargets(projects: readonly string[]): void {
  for (const projectDir of projects) {
    const godotCache = path.join(projectDir, ".godot");
    if (fs.existsSync(godotCache)) {
      console.log(`[CLEAN] Removing ${godotCache}...`);
      removeQuietly(godotCache);
    }
    const targetAddon = addonDir(projectDir);
    if (fs.existsSync(targetAddon)) {
      console.log(`[CLEAN] Removing ${targetAddon}...`);
      removeQuietly(targetAddon);
    }
  }
}

export function runClean(projects: readonly string[]): number {
  let targets: string[];
  try {
    targets = resolveProjects(projects);
  } catch (error) {
    console.log(`[ERROR] ${errorMessage(error)}`);
    return 1;
  }
  cleanTargets(targets);
  console.log("[DONE] Clean complete.");
  return 0;
}
