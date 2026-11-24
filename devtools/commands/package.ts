import fs from "node:fs";
import type { Dirent } from "node:fs";
import os from "node:os";
import path from "node:path";

import {
  ADDON_VERSION,
  BRIDGE_DIR,
  BRIDGE_SOURCES,
  PLATFORM_LIBS,
  PROJECT_ROOT,
  artifactDir,
  hostPlatform,
  hostPlatformLib,
} from "../lib/config";
import { errorMessage } from "../lib/cmake";
import { selectUniqueArtifact } from "../lib/artifact-selection";
import { generateGdextension } from "../lib/gdextension";
import { writeZip, type ZipSourceEntry } from "../lib/zip";

const ADDON_SUBPATH = path.join("addons", "godot-autopilot");
const ADDON_ARCHIVE_PREFIX = "addons/godot-autopilot";

function findAllFiles(dir: string): string[] {
  let entries: Dirent[];
  try {
    entries = fs.readdirSync(dir, { withFileTypes: true });
  } catch {
    return [];
  }
  const found: string[] = [];
  for (const entry of entries) {
    const full = path.join(dir, entry.name);
    if (entry.isFile()) found.push(full);
    if (entry.isDirectory()) found.push(...findAllFiles(full));
  }
  return found.sort();
}

function collectPlatformLibs(libsDir: string, targetAddonDir: string, preset: "debug" | "release"): Set<string> {
  const collected = new Set<string>();
  const candidates = findAllFiles(libsDir);
  for (const libName of Object.values(PLATFORM_LIBS)) {
    const verdict = selectUniqueArtifact(candidates, libName, preset);
    if (verdict.failure === "missing") continue;
    if (verdict.failure !== "none") {
      throw new Error(`Ambiguous or invalid artifact for ${libName} under ${libsDir}: ${verdict.failure}`);
    }
    const source = verdict.selected!;
    fs.cpSync(source, path.join(targetAddonDir, libName), { preserveTimestamps: true });
    console.log(`  ${libName}  <- ${path.relative(libsDir, source)}`);
    collected.add(libName);
  }
  return collected;
}

function collectCSharpBridge(targetAddonDir: string): void {
  const missing = BRIDGE_SOURCES.filter((name) => {
    const source = path.join(BRIDGE_DIR, name);
    return !fs.existsSync(source) || !fs.statSync(source).isFile();
  });
  if (missing.length > 0) {
    throw new Error(`Missing C# bridge sources in ${BRIDGE_DIR}: ${missing.join(", ")}`);
  }
  const names = fs
    .readdirSync(BRIDGE_DIR, { withFileTypes: true })
    .filter((entry) => entry.isFile() && entry.name.endsWith(".cs"))
    .map((entry) => entry.name)
    .sort();
  for (const name of names) {
    const source = path.join(BRIDGE_DIR, name);
    fs.cpSync(source, path.join(targetAddonDir, name), { preserveTimestamps: true });
    console.log(`  ${name}  (${(fs.statSync(source).size / 1024).toFixed(0)} KB)`);
  }
}

function collectPackageFiles(
  libDir: string,
  targetAddonDir: string,
  requiredLibs: readonly string[],
  preset: "debug" | "release",
): void {
  const collected = collectPlatformLibs(libDir, targetAddonDir, preset);
  const missing = requiredLibs.filter((name) => !collected.has(name));
  if (missing.length > 0) {
    throw new Error(`Missing platform libraries under ${libDir}: ${missing.join(", ")}`);
  }
  collectCSharpBridge(targetAddonDir);
  generateGdextension(targetAddonDir);
}

function collectZipEntries(root: string): ZipSourceEntry[] {
  const entries: ZipSourceEntry[] = [];
  const walk = (dir: string, prefix: string): void => {
    const children = fs
      .readdirSync(dir, { withFileTypes: true })
      .sort((left, right) => (left.name < right.name ? -1 : left.name > right.name ? 1 : 0));
    for (const entry of children) {
      const full = path.join(dir, entry.name);
      const name = prefix === "" ? entry.name : `${prefix}/${entry.name}`;
      if (entry.isDirectory()) walk(full, name);
      else if (entry.isFile()) {
        entries.push({ name, data: fs.readFileSync(full), mtime: fs.statSync(full).mtime });
      }
    }
  };
  walk(root, "");
  return entries;
}

function packageAddon(libsDir: string | null, preset: "debug" | "release"): void {
  let libDir: string;
  let requiredLibs: string[];
  if (libsDir !== null) {
    libDir = libsDir;
    requiredLibs = Object.values(PLATFORM_LIBS);
  } else {
    libDir = artifactDir(preset);
    if (!fs.existsSync(libDir)) {
      throw new Error(
        `Build artifacts not found in ${libDir}; run bun run gda-dev-tools build${preset === "release" ? " --release" : ""} first`,
      );
    }
    const libName = hostPlatformLib();
    if (libName === null) throw new Error(`Unsupported platform: ${hostPlatform()}`);
    requiredLibs = [libName];
  }

  const distDir = path.join(PROJECT_ROOT, "dist");
  fs.mkdirSync(distDir, { recursive: true });
  const stageRoot = fs.mkdtempSync(path.join(os.tmpdir(), "gda-package-"));
  try {
    const stageAddon = path.join(stageRoot, ADDON_SUBPATH);
    fs.mkdirSync(stageAddon, { recursive: true });
    collectPackageFiles(libDir, stageAddon, requiredLibs, preset);
    const zipPath = path.join(distDir, `godot-autopilot-${ADDON_VERSION}.zip`);
    const entries: ZipSourceEntry[] = [
      {
        name: `${ADDON_ARCHIVE_PREFIX}/`,
        data: Buffer.alloc(0),
        mtime: fs.statSync(stageAddon).mtime,
      },
      ...collectZipEntries(stageRoot),
    ];
    writeZip(zipPath, entries);
    console.log(`[PACKAGE] ${zipPath} (${(fs.statSync(zipPath).size / 1024).toFixed(0)} KB)`);
  } finally {
    fs.rmSync(stageRoot, { recursive: true, force: true });
  }
}

export function runPackage(libsDir: string | null, preset: "debug" | "release"): number {
  console.log("[PACKAGE] Packaging build artifacts...");
  try {
    packageAddon(libsDir, preset);
    return 0;
  } catch (error) {
    console.log(`[ERROR] ${errorMessage(error)}`);
    return 1;
  }
}
