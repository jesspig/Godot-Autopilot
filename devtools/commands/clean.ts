import fs from "node:fs";
import path from "node:path";

import { DEPLOY_TARGETS, addonDir } from "../lib/config";

function removeQuietly(target: string): void {
  try {
    fs.rmSync(target, { recursive: true, force: true });
  } catch {}
}

export function cleanTargets(): void {
  for (const projectDir of DEPLOY_TARGETS) {
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

export function runClean(): number {
  cleanTargets();
  console.log("[DONE] Clean complete.");
  return 0;
}
