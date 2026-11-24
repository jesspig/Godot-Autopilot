import { randomUUID } from "node:crypto";
import fs from "node:fs";
import path from "node:path";

import {
  ADDON_VERSION,
  BRIDGE_DIR,
  BRIDGE_SOURCES,
  PLATFORM_LIBS,
  PLATFORM_PDB,
  addonDir,
  artifactDir,
  hostPlatform,
  resolveProjects,
} from "../lib/config";
import { errorMessage } from "../lib/cmake";
import { GDEXTENSION_NAME, generateGdextension, libraries } from "../lib/gdextension";
import {
  prepareOwnedFile,
  readOwnedFilesManifest,
  removeOwnedFile,
  writeOwnedFilesManifest,
  type OwnedFilesManifest,
} from "../lib/owned-files";

const ADDON_RESOURCE_PREFIX = "res://addons/godot-autopilot/";
const STAGING_PREFIX = ".godot-autopilot-staging-";
const BACKUP_PREFIX = ".godot-autopilot-backup-";

export type DeployOptions = {
  release: boolean;
  debug: boolean;
  projects: string[];
};

export type DeployHooks = {
  copyFile?: (source: string, destination: string) => void;
  copyDirectory?: (source: string, destination: string) => void;
  rename?: (source: string, destination: string) => void;
  removeDirectory?: (target: string) => void;
};

export type DeployArtifactOptions = {
  binaryDir?: string;
  bridgeDir?: string;
  platform?: string;
  version?: string;
  hooks?: DeployHooks;
};

function copyFile(source: string, destination: string): void {
  fs.cpSync(source, destination, { preserveTimestamps: true });
}

function copyDirectory(source: string, destination: string): void {
  const sourceStat = fs.lstatSync(source);
  if (sourceStat.isSymbolicLink() || !sourceStat.isDirectory()) {
    throw new Error(`cannot copy addon directory: ${source}`);
  }
  fs.mkdirSync(destination, { recursive: true });
  for (const entry of fs.readdirSync(source, { withFileTypes: true })) {
    const sourcePath = path.join(source, entry.name);
    const destinationPath = path.join(destination, entry.name);
    if (entry.isSymbolicLink()) {
      const linkTarget = fs.readlinkSync(sourcePath);
      const linkType =
        process.platform === "win32" ? (fs.statSync(sourcePath).isDirectory() ? "junction" : "file") : undefined;
      fs.symlinkSync(linkTarget, destinationPath, linkType);
    } else if (entry.isDirectory()) {
      copyDirectory(sourcePath, destinationPath);
    } else if (entry.isFile()) {
      fs.cpSync(sourcePath, destinationPath, { preserveTimestamps: true });
    } else {
      throw new Error(`cannot copy unsupported addon entry: ${sourcePath}`);
    }
  }
}

function sizeLabel(source: string): string {
  return (fs.statSync(source).size / 1024).toFixed(0);
}

function bridgeSourceNames(bridgeDir: string): string[] {
  if (!fs.existsSync(bridgeDir)) return [];
  return fs
    .readdirSync(bridgeDir, { withFileTypes: true })
    .filter((entry) => entry.isFile() && entry.name.endsWith(".cs"))
    .map((entry) => entry.name)
    .sort();
}

function warnUnownedBridgeFiles(targetAddonDir: string, sourceNames: ReadonlySet<string>): void {
  for (const entry of fs.readdirSync(targetAddonDir, { withFileTypes: true })) {
    const unknownBridge = entry.isFile() && entry.name.endsWith(".cs") && !sourceNames.has(entry.name);
    if (unknownBridge || entry.name.startsWith("~") || entry.isDirectory() || entry.isSymbolicLink()) {
      console.log(`  [WARN] preserved unowned addon entry: ${entry.name}`);
    }
  }
}

function deployCSharpBridge(
  targetAddonDir: string,
  bridgeDir: string,
  copyArtifact: (source: string, destination: string) => void,
): string[] {
  const names = bridgeSourceNames(bridgeDir);
  for (const name of names) {
    const source = path.join(bridgeDir, name);
    const destination = prepareOwnedFile(targetAddonDir, name);
    copyArtifact(source, destination);
    console.log(`  ${name}  (${sizeLabel(source)} KB)`);
  }
  if (names.length === 0) console.log(`[WARN] no bridge sources found in ${bridgeDir}`);
  return names;
}

function readPreviousManifest(targetAddonDir: string): OwnedFilesManifest | null {
  try {
    return readOwnedFilesManifest(targetAddonDir);
  } catch (error) {
    console.log(`  [WARN] ${errorMessage(error)}; preserving all unlisted files`);
    return null;
  }
}

function removeStaleOwnedFiles(
  targetAddonDir: string,
  previous: OwnedFilesManifest | null,
  currentFiles: ReadonlySet<string>,
): void {
  if (previous === null) return;
  for (const relativePath of previous.files) {
    if (currentFiles.has(relativePath)) continue;
    const removal = removeOwnedFile(targetAddonDir, relativePath);
    if (removal === "removed") {
      console.log(`  removed stale ${relativePath}`);
    } else if (removal === "not-file") {
      console.log(`  [WARN] preserved non-file owned path: ${relativePath}`);
    }
  }
}

function validateAddonIntegrity(targetAddonDir: string, platform: string): boolean {
  const platformKey =
    platform === "windows" ? "windows" : platform === "linux" ? "linux" : platform === "darwin" ? "macos" : null;
  if (platformKey === null) {
    console.log(`[ERROR] Unsupported platform: ${platform}`);
    return false;
  }
  const content = fs.readFileSync(path.join(targetAddonDir, GDEXTENSION_NAME), "utf8");
  const missing: string[] = [];
  for (const entry of libraries(content)) {
    if (entry.key.split(".", 1)[0] !== platformKey) continue;
    if (!entry.path.startsWith(ADDON_RESOURCE_PREFIX)) {
      missing.push(entry.path);
      continue;
    }
    const diskPath = path.join(
      targetAddonDir,
      ...entry.path.slice(ADDON_RESOURCE_PREFIX.length).split("/"),
    );
    if (!fs.existsSync(diskPath)) missing.push(entry.path);
  }
  if (missing.length === 0) return true;
  for (const resPath of missing) {
    console.log(`[ERROR] Incomplete addon: ${resPath} missing; export will fail in host projects`);
  }
  return false;
}

function validateOwnedFiles(targetAddonDir: string, files: ReadonlySet<string>): boolean {
  const missing = [...files].filter((file) => !fs.lstatSync(path.join(targetAddonDir, file)).isFile());
  if (missing.length === 0) return true;
  for (const file of missing) {
    console.log(`[ERROR] Owned addon file is missing or invalid: ${file}`);
  }
  return false;
}

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

function replaceAddonDirectory(
  staging: string,
  target: string,
  hooks: DeployHooks,
): void {
  const rename = hooks.rename ?? fs.renameSync;
  const removeDirectory =
    hooks.removeDirectory ??
    ((directory: string) => fs.rmSync(directory, { recursive: true, force: true }));
  const backup = path.join(path.dirname(target), `${BACKUP_PREFIX}${randomUUID()}`);
  const targetStat = lstatOrNull(target);
  let originalMoved = false;
  try {
    if (targetStat !== null) {
      rename(target, backup);
      originalMoved = true;
    }
    rename(staging, target);
  } catch (error) {
    if (originalMoved) {
      try {
        if (lstatOrNull(target) !== null) {
          throw new Error("target unexpectedly exists after failed install");
        }
        rename(backup, target);
      } catch (rollbackError) {
        throw new Error(
          `${errorMessage(error)}; rollback failed, backup retained at ${backup}: ${errorMessage(rollbackError)}`,
        );
      }
    }
    throw error;
  }
  if (originalMoved) {
    try {
      removeDirectory(backup);
    } catch (error) {
      console.log(`  [WARN] deployed successfully but could not remove backup ${backup}: ${errorMessage(error)}`);
    }
  }
}

function removeStaging(stagingRoot: string | null, hooks: DeployHooks): void {
  if (stagingRoot === null || lstatOrNull(stagingRoot) === null) return;
  const removeDirectory =
    hooks.removeDirectory ??
    ((directory: string) => fs.rmSync(directory, { recursive: true, force: true }));
  try {
    removeDirectory(stagingRoot);
  } catch (error) {
    console.log(`  [WARN] could not remove staging directory ${stagingRoot}: ${errorMessage(error)}`);
  }
}

function removeCreatedParent(parent: string, parentExisted: boolean): void {
  if (parentExisted || lstatOrNull(parent) === null) return;
  try {
    if (fs.readdirSync(parent).length === 0) fs.rmdirSync(parent);
  } catch (error) {
    console.log(`  [WARN] could not remove empty addon parent ${parent}: ${errorMessage(error)}`);
  }
}

export function deployArtifacts(
  preset: string,
  projects: readonly string[],
  options: DeployArtifactOptions = {},
): number {
  const binaryDir = options.binaryDir ?? artifactDir(preset);
  const platform = options.platform ?? hostPlatform();
  const bridgeDir = options.bridgeDir ?? BRIDGE_DIR;
  const version = options.version ?? ADDON_VERSION;
  const hooks = options.hooks ?? {};
  const copyArtifact = hooks.copyFile ?? copyFile;
  const copyExistingDirectory = hooks.copyDirectory ?? copyDirectory;
  const libName = PLATFORM_LIBS[platform];
  if (libName === undefined) {
    console.log(`[ERROR] Unsupported platform: ${platform}`);
    return 1;
  }
  for (const projectDir of projects) {
    console.log(`[DEPLOY] ${projectDir}`);
    const targetAddonDir = addonDir(projectDir);
    const parent = path.dirname(targetAddonDir);
    const parentExisted = lstatOrNull(parent) !== null;
    let stagingRoot: string | null = null;
    try {
      const targetStat = lstatOrNull(targetAddonDir);
      if (targetStat !== null && !targetStat.isDirectory()) {
        throw new Error(`addon path is not a directory: ${targetAddonDir}`);
      }
      fs.mkdirSync(parent, { recursive: true });
      stagingRoot = fs.mkdtempSync(path.join(parent, STAGING_PREFIX));
      const staging = path.join(stagingRoot, "addon");
      const previous = targetStat === null ? null : readPreviousManifest(targetAddonDir);
      if (targetStat !== null) copyExistingDirectory(targetAddonDir, staging);

      const ownedFiles = new Set<string>([GDEXTENSION_NAME]);
      prepareOwnedFile(staging, GDEXTENSION_NAME);
      generateGdextension(staging);
      const libSource = path.join(binaryDir, libName);
      if (!fs.existsSync(libSource)) {
        throw new Error(`missing deployment artifact: ${libName}`);
      }
      copyArtifact(libSource, prepareOwnedFile(staging, libName));
      ownedFiles.add(libName);
      console.log(`  ${libName}  (${sizeLabel(libSource)} KB)`);
      if (platform === "windows") {
        const pdbSource = path.join(binaryDir, PLATFORM_PDB);
        if (!fs.existsSync(pdbSource)) {
          throw new Error(`missing deployment artifact: ${PLATFORM_PDB}`);
        }
        copyArtifact(pdbSource, prepareOwnedFile(staging, PLATFORM_PDB));
        ownedFiles.add(PLATFORM_PDB);
        console.log(`  ${PLATFORM_PDB}  (${sizeLabel(pdbSource)} KB)`);
      }
      const bridgeNames = deployCSharpBridge(staging, bridgeDir, copyArtifact);
      for (const name of bridgeNames) ownedFiles.add(name);
      removeStaleOwnedFiles(staging, previous, ownedFiles);
      warnUnownedBridgeFiles(staging, new Set(bridgeNames));
      writeOwnedFilesManifest(staging, [...ownedFiles], version);
      if (!validateOwnedFiles(staging, ownedFiles) || !validateAddonIntegrity(staging, platform)) {
        throw new Error("staged addon validation failed");
      }

      replaceAddonDirectory(staging, targetAddonDir, hooks);
      console.log(`  -> ${targetAddonDir}`);
      removeStaging(stagingRoot, hooks);
      continue;
    } catch (error) {
      console.log(`[ERROR] Deployment failed for ${projectDir}: ${errorMessage(error)}`);
      removeStaging(stagingRoot, hooks);
      removeCreatedParent(parent, parentExisted);
      return 1;
    }
  }
  return 0;
}

export function missingDeployArtifacts(binaryDir: string, platform: string): string[] {
  const libName = PLATFORM_LIBS[platform];
  if (libName === undefined) return ["unsupported platform: " + platform];
  const missing: string[] = [];
  if (!fs.existsSync(path.join(binaryDir, libName))) missing.push(libName);
  if (platform === "windows" && !fs.existsSync(path.join(binaryDir, PLATFORM_PDB))) {
    missing.push(PLATFORM_PDB);
  }
  for (const name of BRIDGE_SOURCES) {
    if (!fs.existsSync(path.join(BRIDGE_DIR, name))) missing.push(name);
  }
  return missing;
}

export async function runDeploy(options: DeployOptions): Promise<number> {
  if (options.release && options.debug) {
    console.log("[ERROR] Cannot specify both --release and --debug");
    return 1;
  }
  let projects: string[];
  try {
    projects = resolveProjects(options.projects);
  } catch (error) {
    console.log(`[ERROR] ${errorMessage(error)}`);
    return 1;
  }
  const preset = options.release ? "release" : "debug";
  const binaryDir = artifactDir(preset);
  const missing = missingDeployArtifacts(binaryDir, hostPlatform());
  if (missing.length > 0) {
    console.log(`[ERROR] Deployment aborted; missing artifacts: ${missing.join(", ")}`);
    console.log("[ERROR] Existing addons were left untouched.");
    return 1;
  }
  console.log(`\n[DEPLOY] Copying ${preset} artifacts to ${projects.length} project(s)...`);
  const code = deployArtifacts(preset, projects);
  if (code !== 0) return code;
  console.log("\n[DONE] Deploy complete.");
  return 0;
}
