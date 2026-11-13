import { createHash } from "node:crypto";
import fs from "node:fs";
import path from "node:path";

import { CliError, ExitCode } from "./errors";
import { gdaRoot, joinPath } from "./platform";

export type CacheArtifact = "cli" | "plugin" | "skills";
export type CacheSource = "github" | "atomgit" | "builtin";

export interface CacheManifest {
  artifact: CacheArtifact;
  version: string;
  source: CacheSource;
  asset: string;
  sha256: string;
  size: number;
  platform: string;
  lang: string;
  fetched_at: string;
  builtin: boolean;
}

const MANIFEST_DIR = "manifests";
const LOCK_NAME = "cache.lock";
const LOCK_STALE_MS = 10_000;
const DEFAULT_KEEP_VERSIONS = 3;
const DEFAULT_MAX_BYTES = 500 * 1024 * 1024;

export function cacheRoot(): string {
  return joinPath(gdaRoot(), "cache");
}

export function manifestDir(): string {
  return joinPath(cacheRoot(), MANIFEST_DIR);
}

export function manifestPath(manifest: CacheManifest): string {
  const suffix = manifest.artifact === "skills" ? `-${manifest.lang}` : "";
  return joinPath(manifestDir(), `${manifest.artifact}-${manifest.version}${suffix}.json`);
}

export function cacheAssetPath(manifest: CacheManifest): string {
  const root = cacheRoot();
  switch (manifest.artifact) {
    case "cli":
      return joinPath(root, "cli", manifest.version, manifest.platform, manifest.asset);
    case "plugin":
      return joinPath(root, "plugin", manifest.version, manifest.asset);
    case "skills":
      return joinPath(root, "skills", manifest.lang, manifest.version, manifest.asset);
  }
}

function sleepMs(ms: number): void {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

export function withCacheLock<T>(fn: () => T): T {
  const dir = cacheRoot();
  fs.mkdirSync(dir, { recursive: true });
  const lock = joinPath(dir, LOCK_NAME);
  const deadline = Date.now() + 2000;
  for (;;) {
    try {
      const fd = fs.openSync(lock, "wx");
      fs.writeFileSync(fd, String(process.pid));
      fs.closeSync(fd);
      try {
        return fn();
      } finally {
        fs.rmSync(lock, { force: true });
      }
    } catch (error) {
      if ((error as NodeJS.ErrnoException).code !== "EEXIST") throw error;
      if (Date.now() >= deadline) {
        try {
          const stat = fs.statSync(lock);
          if (Date.now() - stat.mtimeMs >= LOCK_STALE_MS) {
            fs.rmSync(lock, { force: true });
            continue;
          }
        } catch {
        }
        throw new CliError(ExitCode.Transport, "cache lock busy");
      }
      sleepMs(50);
    }
  }
}

export function writeManifest(manifest: CacheManifest): void {
  withCacheLock(() => {
    fs.mkdirSync(manifestDir(), { recursive: true });
    const target = manifestPath(manifest);
    const tmp = `${target}.${process.pid}.tmp`;
    fs.writeFileSync(tmp, `${JSON.stringify(manifest, null, 2)}\n`, "utf8");
    fs.renameSync(tmp, target);
  });
}

function parseManifest(raw: unknown): CacheManifest | null {
  if (raw === null || typeof raw !== "object" || Array.isArray(raw)) return null;
  const doc = raw as Record<string, unknown>;
  const artifact = doc.artifact;
  const version = doc.version;
  const source = doc.source;
  const asset = doc.asset;
  const sha256 = doc.sha256;
  const size = doc.size;
  const platform = doc.platform;
  const lang = doc.lang;
  const fetchedAt = doc.fetched_at;
  const builtin = doc.builtin;
  if (!["cli", "plugin", "skills"].includes(String(artifact))) return null;
  if (typeof version !== "string" || version === "") return null;
  if (!["github", "atomgit", "builtin"].includes(String(source))) return null;
  if (typeof asset !== "string" || asset === "") return null;
  if (typeof sha256 !== "string" || !/^[0-9a-f]{64}$/i.test(sha256)) return null;
  if (typeof size !== "number" || size < 0) return null;
  if (typeof platform !== "string" || typeof lang !== "string") return null;
  if (typeof fetchedAt !== "string" || fetchedAt === "") return null;
  if (typeof builtin !== "boolean") return null;
  return {
    artifact: artifact as CacheArtifact,
    version,
    source: source as CacheSource,
    asset,
    sha256: sha256.toLowerCase(),
    size,
    platform,
    lang,
    fetched_at: fetchedAt,
    builtin,
  };
}

export function listManifests(): CacheManifest[] {
  let names: string[];
  try {
    names = fs.readdirSync(manifestDir()).filter((name) => name.endsWith(".json"));
  } catch {
    return [];
  }
  const manifests: CacheManifest[] = [];
  for (const name of names) {
    try {
      const parsed = parseManifest(JSON.parse(fs.readFileSync(joinPath(manifestDir(), name), "utf8")));
      if (parsed !== null) manifests.push(parsed);
    } catch {
    }
  }
  return manifests.sort((a, b) => a.artifact.localeCompare(b.artifact) || b.fetched_at.localeCompare(a.fetched_at));
}

function versionKey(version: string): number[] {
  const parts = version.replace(/^v/, "").split(/[.+-]/).map((part) => Number(part));
  if (parts.length < 3 || parts.some((part) => !Number.isFinite(part))) return [0, 0, 0];
  return [parts[0], parts[1], parts[2]];
}

function compareVersion(a: string, b: string): number {
  const left = versionKey(a);
  const right = versionKey(b);
  for (let index = 0; index < 3; index += 1) {
    if (left[index] !== right[index]) return left[index] - right[index];
  }
  return a.localeCompare(b);
}

export function selectLatest(
  artifact: CacheArtifact,
  lang?: string,
): CacheManifest | undefined {
  const candidates = listManifests().filter(
    (manifest) =>
      manifest.artifact === artifact &&
      !manifest.builtin &&
      (lang === undefined || artifact !== "skills" || manifest.lang === lang),
  );
  if (candidates.length === 0) return undefined;
  if (artifact === "cli" || artifact === "plugin") {
    return candidates.reduce((best, current) =>
      compareVersion(current.version, best.version) > 0 ? current : best,
    );
  }
  return candidates.reduce((best, current) =>
    current.fetched_at > best.fetched_at ? current : best,
  );
}

export function sha256File(filePath: string): string {
  return createHash("sha256").update(fs.readFileSync(filePath)).digest("hex");
}

export function verifyManifest(manifest: CacheManifest): boolean {
  const assetPath = cacheAssetPath(manifest);
  try {
    if (fs.statSync(assetPath).size !== manifest.size) return false;
    return sha256File(assetPath) === manifest.sha256;
  } catch {
    return false;
  }
}

export interface CacheCleanResult {
  removed: CacheManifest[];
  kept: CacheManifest[];
  bytesBefore: number;
  bytesAfter: number;
}

export function cleanCache(options: {
  keepVersions?: number;
  maxBytes?: number;
} = {}): CacheCleanResult {
  const keepVersions = options.keepVersions ?? DEFAULT_KEEP_VERSIONS;
  const maxBytes = options.maxBytes ?? DEFAULT_MAX_BYTES;
  return withCacheLock(() => {
    const manifests = listManifests();
    const groups = new Map<string, CacheManifest[]>();
    for (const manifest of manifests) {
      if (manifest.builtin) continue;
      const key = `${manifest.artifact}:${manifest.artifact === "skills" ? manifest.lang : ""}`;
      const group = groups.get(key) ?? [];
      group.push(manifest);
      groups.set(key, group);
    }
    const removed: CacheManifest[] = [];
    for (const group of groups.values()) {
      group.sort((a, b) =>
        a.artifact === "cli" || a.artifact === "plugin"
          ? compareVersion(b.version, a.version)
          : b.fetched_at.localeCompare(a.fetched_at),
      );
      for (const manifest of group.slice(keepVersions)) {
        removeManifest(manifest);
        removed.push(manifest);
      }
    }
    let kept = listManifests().filter((manifest) => !manifest.builtin);
    let bytes = kept.reduce((sum, manifest) => sum + manifest.size, 0);
    const bytesBefore = bytes;
    kept.sort((a, b) => a.fetched_at.localeCompare(b.fetched_at));
    while (bytes > maxBytes && kept.length > 1) {
      const manifest = kept.shift();
      if (manifest === undefined) break;
      removeManifest(manifest);
      removed.push(manifest);
      bytes -= manifest.size;
    }
    return {
      removed,
      kept: listManifests(),
      bytesBefore,
      bytesAfter: bytes,
    };
  });
}

function removeManifest(manifest: CacheManifest): void {
  const manifestFile = manifestPath(manifest);
  fs.rmSync(manifestFile, { force: true });
  const asset = cacheAssetPath(manifest);
  fs.rmSync(asset, { force: true });
  const parent = path.dirname(asset);
  try {
    if (fs.readdirSync(parent).length === 0) fs.rmdirSync(parent);
  } catch {
  }
}

export interface CacheInfo {
  root: string;
  manifests: number;
  builtin: number;
  bytes: number;
  by_artifact: Record<string, number>;
}

export function cacheInfo(): CacheInfo {
  const manifests = listManifests();
  const byArtifact: Record<string, number> = {};
  let bytes = 0;
  let builtin = 0;
  for (const manifest of manifests) {
    byArtifact[manifest.artifact] = (byArtifact[manifest.artifact] ?? 0) + 1;
    bytes += manifest.size;
    if (manifest.builtin) builtin += 1;
  }
  return {
    root: cacheRoot(),
    manifests: manifests.length,
    builtin,
    bytes,
    by_artifact: byArtifact,
  };
}
