import fs from "node:fs";
import path from "node:path";

import { OWNED_FILES_MANIFEST, addonDir, resolveProjects } from "../lib/config";
import { errorMessage } from "../lib/cmake";
import { readOwnedFilesManifest, removeOwnedFile, type OwnedFilesManifest } from "../lib/owned-files";

function lstatOrNull(target: string): fs.Stats | null {
  try {
    return fs.lstatSync(target);
  } catch (error) {
    if (
      typeof error === "object" &&
      error !== null &&
      "code" in error &&
      (error as { code?: unknown }).code === "ENOENT"
    ) {
      return null;
    }
    throw error;
  }
}

function warnUnownedEntries(addonDir: string, ownedFiles: ReadonlySet<string>): void {
  for (const entry of fs.readdirSync(addonDir, { withFileTypes: true })) {
    if (entry.name === OWNED_FILES_MANIFEST || ownedFiles.has(entry.name)) continue;
    console.log(`  [WARN] preserved unowned addon entry: ${entry.name}`);
  }
}

export function cleanTargets(projects: readonly string[]): number {
  let failures = 0;
  for (const projectDir of projects) {
    const godotCache = path.join(projectDir, ".godot");
    try {
      if (lstatOrNull(godotCache) !== null) {
        console.log(`[CLEAN] Removing ${godotCache}...`);
        fs.rmSync(godotCache, { recursive: true, force: true });
      }
    } catch (error) {
      console.log(`  [WARN] could not remove Godot cache ${godotCache}: ${errorMessage(error)}`);
      failures += 1;
    }
    const targetAddon = addonDir(projectDir);
    const targetStat = lstatOrNull(targetAddon);
    if (targetStat === null) continue;
    if (!targetStat.isDirectory()) {
      console.log(`  [WARN] preserved non-directory addon path: ${targetAddon}`);
      failures += 1;
      continue;
    }

    let manifest: OwnedFilesManifest | null;
    try {
      manifest = readOwnedFilesManifest(targetAddon);
    } catch (error) {
      console.log(`  [WARN] ${errorMessage(error)}; preserving all addon files`);
      warnUnownedEntries(targetAddon, new Set());
      failures += 1;
      continue;
    }
    if (manifest === null) {
      console.log(`  [WARN] no owned-files manifest in ${targetAddon}; preserving all addon files`);
      warnUnownedEntries(targetAddon, new Set());
      continue;
    }

    let projectFailed = false;
    for (const relativePath of manifest.files) {
      try {
        const removal = removeOwnedFile(targetAddon, relativePath);
        if (removal === "removed") {
          console.log(`  removed owned ${relativePath}`);
        } else if (removal === "not-file") {
          console.log(`  [WARN] preserved non-file owned path: ${relativePath}`);
        }
      } catch (error) {
        console.log(`  [WARN] could not remove owned file ${relativePath}: ${errorMessage(error)}`);
        projectFailed = true;
      }
    }
    if (projectFailed) {
      failures += 1;
      warnUnownedEntries(targetAddon, new Set(manifest.files));
      continue;
    }

    const manifestPath = path.join(targetAddon, OWNED_FILES_MANIFEST);
    try {
      if (lstatOrNull(manifestPath) !== null) fs.unlinkSync(manifestPath);
    } catch (error) {
      console.log(`  [WARN] could not remove owned manifest: ${errorMessage(error)}`);
      failures += 1;
    }
    warnUnownedEntries(targetAddon, new Set(manifest.files));
  }
  return failures === 0 ? 0 : 1;
}

export function runClean(projects: readonly string[]): number {
  let targets: string[];
  try {
    targets = resolveProjects(projects);
  } catch (error) {
    console.log(`[ERROR] ${errorMessage(error)}`);
    return 1;
  }
  const code = cleanTargets(targets);
  if (code === 0) {
    console.log("[DONE] Clean complete.");
  } else {
    console.log("[ERROR] Clean incomplete; some cache or addon files could not be removed.");
  }
  return code;
}
