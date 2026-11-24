import fs from "node:fs";
import path from "node:path";

import { OWNED_FILES_MANIFEST } from "./config";

export type OwnedFilesManifest = {
  schema: 1;
  version: string;
  files: string[];
};

export type OwnedFileRemoval = "missing" | "removed" | "not-file";

export function isSafeOwnedPath(value: string): boolean {
  if (value === "" || value.includes("\0") || value.includes("\\") || value.includes(":")) return false;
  if (value === OWNED_FILES_MANIFEST || path.posix.isAbsolute(value)) return false;
  if (path.win32.isAbsolute(value) || path.win32.parse(value).root !== "") return false;
  const segments = value.split("/");
  return (
    segments.every((segment) => segment !== "" && segment !== "." && segment !== "..") &&
    path.posix.normalize(value) === value
  );
}

function invalidManifest(detail: string): Error {
  return new Error(`invalid owned-files manifest: ${detail}`);
}

function parseManifest(text: string): OwnedFilesManifest {
  let value: unknown;
  try {
    value = JSON.parse(text);
  } catch {
    throw invalidManifest("not valid JSON");
  }
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw invalidManifest("expected an object");
  }
  const candidate = value as Record<string, unknown>;
  if (candidate.schema !== 1) throw invalidManifest("unsupported schema");
  if (typeof candidate.version !== "string" || candidate.version.trim() === "") {
    throw invalidManifest("missing version");
  }
  if (!Array.isArray(candidate.files)) throw invalidManifest("missing files");
  const files: string[] = [];
  for (const entry of candidate.files) {
    if (typeof entry !== "string" || !isSafeOwnedPath(entry)) {
      throw invalidManifest("unsafe file path");
    }
    files.push(entry);
  }
  if (new Set(files).size !== files.length) throw invalidManifest("duplicate file path");
  return { schema: 1, version: candidate.version, files: files.sort() };
}

export function readOwnedFilesManifest(addonDir: string): OwnedFilesManifest | null {
  const manifestPath = path.join(addonDir, OWNED_FILES_MANIFEST);
  if (!fs.existsSync(manifestPath)) return null;
  if (!fs.lstatSync(manifestPath).isFile()) {
    throw invalidManifest("manifest is not a regular file");
  }
  return parseManifest(fs.readFileSync(manifestPath, "utf8"));
}

export function writeOwnedFilesManifest(
  addonDir: string,
  files: readonly string[],
  version: string,
): void {
  if (version.trim() === "") throw invalidManifest("missing version");
  const uniqueFiles = [...new Set(files)];
  if (uniqueFiles.some((file) => !isSafeOwnedPath(file))) {
    throw invalidManifest("unsafe file path");
  }
  const manifestPath = path.join(addonDir, OWNED_FILES_MANIFEST);
  ensureSafeDirectory(addonDir);
  removeLeafForWrite(manifestPath);
  const manifest: OwnedFilesManifest = {
    schema: 1,
    version,
    files: uniqueFiles.sort(),
  };
  fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 2) + "\n", "utf8");
}

function lstatOrNull(target: string): fs.Stats | null {
  try {
    return fs.lstatSync(target);
  } catch (error) {
    if (isMissing(error)) return null;
    throw error;
  }
}

function ensureSafeDirectory(target: string): void {
  const stat = lstatOrNull(target);
  if (stat === null) {
    fs.mkdirSync(target, { recursive: true });
    return;
  }
  if (stat.isSymbolicLink() || !stat.isDirectory()) {
    throw invalidManifest("unsafe parent path");
  }
}

function removeLeafForWrite(target: string): void {
  if (lstatOrNull(target) !== null) {
    fs.rmSync(target, { recursive: true, force: true });
  }
}

function isSymlink(target: string): boolean {
  try {
    return fs.lstatSync(target).isSymbolicLink();
  } catch (error) {
    if (isMissing(error)) return false;
    throw error;
  }
}

function isMissing(error: unknown): boolean {
  return (
    typeof error === "object" &&
    error !== null &&
    "code" in error &&
    (error as { code?: unknown }).code === "ENOENT"
  );
}

export function ownedFilePath(addonDir: string, relativePath: string): string {
  if (!isSafeOwnedPath(relativePath)) throw invalidManifest("unsafe file path");
  const rootStat = lstatOrNull(addonDir);
  if (rootStat !== null && (rootStat.isSymbolicLink() || !rootStat.isDirectory())) {
    throw invalidManifest("unsafe directory path");
  }
  const segments = relativePath.split("/");
  let parent = addonDir;
  for (const segment of segments.slice(0, -1)) {
    parent = path.join(parent, segment);
    let stat: fs.Stats;
    try {
      stat = fs.lstatSync(parent);
    } catch (error) {
      if (isMissing(error)) return path.join(addonDir, ...segments);
      throw error;
    }
    if (stat.isSymbolicLink() || !stat.isDirectory()) {
      throw invalidManifest("unsafe parent path");
    }
  }
  return path.join(addonDir, ...segments);
}

export function prepareOwnedFile(addonDir: string, relativePath: string): string {
  if (!isSafeOwnedPath(relativePath)) throw invalidManifest("unsafe file path");
  ensureSafeDirectory(addonDir);
  const segments = relativePath.split("/");
  let parent = addonDir;
  for (const segment of segments.slice(0, -1)) {
    parent = path.join(parent, segment);
    ensureSafeDirectory(parent);
  }
  const target = path.join(addonDir, ...segments);
  removeLeafForWrite(target);
  return target;
}

export function removeOwnedFile(addonDir: string, relativePath: string): OwnedFileRemoval {
  const target = ownedFilePath(addonDir, relativePath);
  if (!fs.existsSync(target) && !isSymlink(target)) return "missing";
  const stat = fs.lstatSync(target);
  if (!stat.isFile() && !stat.isSymbolicLink()) return "not-file";
  if (stat.isSymbolicLink()) fs.rmSync(target, { force: true });
  else fs.unlinkSync(target);
  return "removed";
}
