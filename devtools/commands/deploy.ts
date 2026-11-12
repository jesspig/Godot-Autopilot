import fs from "node:fs";
import path from "node:path";

import {
  BRIDGE_DIR,
  DEMO_DIRS,
  PLATFORM_LIBS,
  PLATFORM_PDB,
  addonDir,
  artifactDir,
  hostPlatform,
  resolveDemos,
} from "../lib/config";
import { errorMessage } from "../lib/cmake";
import { GDEXTENSION_NAME, generateGdextension, libraries } from "../lib/gdextension";
import { cleanTargets } from "./clean";

const RES_PREFIX = "res://";

export type DeployOptions = {
  release: boolean;
  debug: boolean;
  demos: string[] | null;
};

function copyFile(source: string, destination: string): void {
  fs.cpSync(source, destination, { preserveTimestamps: true });
}

function sizeLabel(source: string): string {
  return (fs.statSync(source).size / 1024).toFixed(0);
}

function bridgeSourceNames(): string[] {
  if (!fs.existsSync(BRIDGE_DIR)) return [];
  return fs
    .readdirSync(BRIDGE_DIR, { withFileTypes: true })
    .filter((entry) => entry.isFile() && entry.name.endsWith(".cs"))
    .map((entry) => entry.name)
    .sort();
}

function deployCSharpBridge(targetAddonDir: string): void {
  const names = bridgeSourceNames();
  const sourceNames = new Set(names);
  const existing = fs
    .readdirSync(targetAddonDir, { withFileTypes: true })
    .filter((entry) => entry.isFile() && entry.name.endsWith(".cs"))
    .map((entry) => entry.name)
    .sort();
  for (const name of existing) {
    if (sourceNames.has(name)) continue;
    fs.unlinkSync(path.join(targetAddonDir, name));
    console.log(`  removed stale ${name}`);
  }
  for (const name of names) {
    const source = path.join(BRIDGE_DIR, name);
    copyFile(source, path.join(targetAddonDir, name));
    console.log(`  ${name}  (${sizeLabel(source)} KB)`);
  }
  if (names.length === 0) console.log(`[WARN] no bridge sources found in ${BRIDGE_DIR}`);
}

function removeHotReloadBackups(targetAddonDir: string): void {
  for (const entry of fs.readdirSync(targetAddonDir, { withFileTypes: true })) {
    if (!entry.name.startsWith("~")) continue;
    try {
      fs.unlinkSync(path.join(targetAddonDir, entry.name));
    } catch (error) {
      console.log(`  [WARN] skip locked temp file ${entry.name}: ${errorMessage(error)}`);
    }
  }
}

function validateAddonIntegrity(targetAddonDir: string, projectDir: string): boolean {
  const platform = hostPlatform();
  const platformKey = platform === "windows" ? "windows" : platform === "linux" ? "linux" : platform === "darwin" ? "macos" : null;
  if (platformKey === null) {
    console.log(`[ERROR] Unsupported platform: ${platform}`);
    return false;
  }
  const content = fs.readFileSync(path.join(targetAddonDir, GDEXTENSION_NAME), "utf8");
  const missing: string[] = [];
  for (const entry of libraries(content)) {
    if (entry.key.split(".", 1)[0] !== platformKey) continue;
    const diskPath = path.join(projectDir, entry.path.startsWith(RES_PREFIX) ? entry.path.slice(RES_PREFIX.length) : entry.path);
    if (!fs.existsSync(diskPath)) missing.push(entry.path);
  }
  if (missing.length === 0) return true;
  for (const resPath of missing) {
    console.log(
      `[ERROR] Incomplete addon: ${resPath} missing; export will fail in host projects`,
    );
  }
  return false;
}

export function deployArtifacts(preset: string, demos: readonly string[] | null = null): number {
  const binaryDir = artifactDir(preset);
  const platform = hostPlatform();
  const libName = PLATFORM_LIBS[platform];
  if (libName === undefined) {
    console.log(`[ERROR] Unsupported platform: ${platform}`);
    return 1;
  }
  for (const projectDir of demos === null ? DEMO_DIRS : demos) {
    console.log(`[DEPLOY] ${projectDir}`);
    const targetAddonDir = addonDir(projectDir);
    fs.mkdirSync(targetAddonDir, { recursive: true });
    removeHotReloadBackups(targetAddonDir);
    generateGdextension(targetAddonDir);
    const libSource = path.join(binaryDir, libName);
    if (fs.existsSync(libSource)) {
      copyFile(libSource, path.join(targetAddonDir, libName));
      console.log(`  ${libName}  (${sizeLabel(libSource)} KB)`);
    } else {
      console.log(`[WARN] ${libName} not found in ${binaryDir}`);
    }
    if (platform === "windows") {
      const pdbSource = path.join(binaryDir, PLATFORM_PDB);
      if (fs.existsSync(pdbSource)) {
        copyFile(pdbSource, path.join(targetAddonDir, PLATFORM_PDB));
        console.log(`  ${PLATFORM_PDB}  (${sizeLabel(pdbSource)} KB)`);
      }
    }
    deployCSharpBridge(targetAddonDir);
    console.log(`  -> ${targetAddonDir}`);
    if (!validateAddonIntegrity(targetAddonDir, projectDir)) return 1;
  }
  return 0;
}

export async function runDeploy(options: DeployOptions): Promise<number> {
  if (options.release && options.debug) {
    console.log("[ERROR] Cannot specify both --release and --debug");
    return 1;
  }
  let demos: string[];
  try {
    demos = resolveDemos(options.demos);
  } catch (error) {
    console.log(`[ERROR] ${errorMessage(error)}`);
    return 1;
  }
  const preset = options.release ? "release" : "debug";
  if (options.release) cleanTargets();
  console.log(`\n[DEPLOY] Copying ${preset} artifacts to examples/*/addons/...`);
  const code = deployArtifacts(preset, demos);
  if (code !== 0) return code;
  console.log("\n[DONE] Deploy complete.");
  return 0;
}
