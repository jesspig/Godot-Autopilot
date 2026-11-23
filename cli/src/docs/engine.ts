import { spawnSync } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { CliError, errorMessage, ExitCode } from "../errors";
import { resolveGodotBin } from "../process";
import { findLiveByProject } from "../registry";
import {
  engineVersionFromHeader,
  parseDump,
  type ApiIndex,
  type DumpHeader,
  type EngineVersion,
} from "./api";
import {
  bucketKey,
  bucketPath,
  docsRoot,
  listBuckets,
  readBucket,
  writeBucket,
  type DocsBucket,
  type DocsBucketSummary,
  type DocsMeta,
} from "./store";

export type EngineSource = "explicit" | "live" | "project" | "newest-cache" | "path";

export interface EngineResolution {
  key: string;
  source: EngineSource;
  binary: string;
  version: EngineVersion;
}

export interface DocsCacheReport {
  status: "fresh" | "auto-exported" | "external";
  path: string;
  synced_at: string;
  generated_by: string;
  verified: boolean;
}

export interface LoadedDocs {
  engine: EngineResolution;
  cache: DocsCacheReport;
  index: ApiIndex;
  docsPath: string;
}

export interface ExportResult {
  index: ApiIndex;
  meta: DocsMeta;
  apiPath: string;
}

export interface ExportRun {
  status: number | null;
  error?: Error;
  stderr?: string;
  signal?: string | null;
}

export interface EngineDeps {
  readVersion?: (bin: string) => EngineVersion;
  findLive?: (project: string) => { godot: string } | undefined;
  readProjectPin?: (project: string) => EngineVersion | undefined;
  listBuckets?: () => DocsBucketSummary[];
  resolveBinary?: () => { bin: string; source: string };
  runExport?: (bin: string, cwd: string, timeoutMs: number) => ExportRun;
  now?: () => Date;
}

export interface CacheStatusReport {
  exists: boolean;
  path: string;
  generated_by: string;
  stale: boolean;
  next_query_will_export: boolean;
}

const EXPORT_TIMEOUT_MS = 120_000;
const VERSION_TIMEOUT_MS = 30_000;

export function readGodotVersion(bin: string): EngineVersion {
  const result = spawnSync(bin, ["--version"], {
    encoding: "utf8",
    timeout: VERSION_TIMEOUT_MS,
    windowsHide: true,
  });
  const stdout = typeof result.stdout === "string" ? result.stdout : "";
  const stderr = typeof result.stderr === "string" ? result.stderr : "";
  if (result.error !== undefined || result.status !== 0) {
    const detail = result.error !== undefined ? result.error.message : stderr.trim();
    throw new CliError(
      ExitCode.Transport,
      `cannot read the Godot version from ${bin}${detail === "" ? "" : `: ${detail}`}`,
      "Pass --godot <path> to a Godot executable that matches the project.",
    );
  }
  const line = stdout.split(/\r?\n/).map((item) => item.trim()).find((item) => item !== "") ?? "";
  const match = /^(\d+)\.(\d+)(?:\.(\d+))?(?:\.([A-Za-z0-9_-]+))?/.exec(line);
  if (match === null) {
    throw new CliError(
      ExitCode.Transport,
      `cannot parse the Godot version reported by ${bin}: ${line}`,
      "Pass --godot <path> to a Godot executable that matches the project.",
    );
  }
  return {
    major: Number(match[1]),
    minor: Number(match[2]),
    ...(match[3] !== undefined ? { patch: Number(match[3]) } : {}),
    ...(match[4] !== undefined ? { status: match[4] } : {}),
    fullName: line,
  };
}

function findProjectFile(start: string): string | undefined {
  let dir = path.resolve(start);
  for (;;) {
    const candidate = path.join(dir, "project.godot");
    if (fs.existsSync(candidate)) return candidate;
    const parent = path.dirname(dir);
    if (parent === dir) return undefined;
    dir = parent;
  }
}

export function readProjectPin(start: string): EngineVersion | undefined {
  const file = findProjectFile(start);
  if (file === undefined) return undefined;
  let text: string;
  try {
    text = fs.readFileSync(file, "utf8");
  } catch {
    return undefined;
  }
  const match = /config\/features=PackedStringArray\(([^)]*)\)/.exec(text);
  if (match === null) return undefined;
  for (const item of match[1].matchAll(/"(\d+)\.(\d+)"/g)) {
    return { major: Number(item[1]), minor: Number(item[2]) };
  }
  return undefined;
}

function resolution(source: EngineSource, binary: string, version: EngineVersion): EngineResolution {
  return { key: bucketKey(version), source, binary, version };
}

function pickNewest(buckets: DocsBucketSummary[]): DocsBucketSummary | undefined {
  return buckets.length === 0 ? undefined : buckets[buckets.length - 1];
}

function versionFromMeta(meta: DocsMeta): EngineVersion {
  return {
    major: meta.engine.major,
    minor: meta.engine.minor,
    patch: meta.engine.patch,
    status: meta.engine.status,
    fullName: meta.engine.full_name,
  };
}

export function resolveEngine(options: { godot?: string; project?: string } = {}, deps: EngineDeps = {}): EngineResolution {
  const projectPath = path.resolve(options.project ?? process.cwd());
  const readVersion = deps.readVersion ?? readGodotVersion;
  if (options.godot !== undefined && options.godot.trim() !== "") {
    const bin = path.resolve(options.godot);
    return resolution("explicit", bin, readVersion(bin));
  }
  const findLive = deps.findLive ?? ((project: string) => findLiveByProject(project));
  const live = findLive(projectPath);
  if (live !== undefined && live.godot.trim() !== "") {
    try {
      return resolution("live", live.godot, readVersion(live.godot));
    } catch {
    }
  }
  const readPin = deps.readProjectPin ?? readProjectPin;
  const pin = readPin(projectPath);
  if (pin !== undefined) return resolution("project", "", pin);
  const buckets = (deps.listBuckets ?? listBuckets)();
  const newest = pickNewest(buckets);
  if (newest !== undefined) {
    return {
      key: newest.key,
      source: "newest-cache",
      binary: newest.meta.source_binary,
      version: versionFromMeta(newest.meta),
    };
  }
  const resolved = (deps.resolveBinary ?? (() => resolveGodotBin({})))();
  return resolution("path", resolved.bin, readVersion(resolved.bin));
}

export function docsFreshness(header: DumpHeader, version: EngineVersion): { fresh: boolean; verified: boolean } {
  const sameMinor = header.version_major === version.major && header.version_minor === version.minor;
  if (version.patch === undefined) {
    return { fresh: sameMinor, verified: false };
  }
  const samePatch = header.version_patch === version.patch;
  const sameStatus = version.status === undefined || header.version_status === version.status;
  return { fresh: sameMinor && samePatch && sameStatus, verified: true };
}

function defaultRunExport(bin: string, cwd: string, timeoutMs: number): ExportRun {
  const result = spawnSync(bin, ["--headless", "--dump-extension-api-with-docs"], {
    cwd,
    timeout: timeoutMs,
    encoding: "utf8",
    windowsHide: true,
    maxBuffer: 16 * 1024 * 1024,
  });
  return {
    status: result.status,
    ...(result.error !== undefined ? { error: result.error } : {}),
    ...(typeof result.stderr === "string" ? { stderr: result.stderr } : {}),
    signal: result.signal,
  };
}

function describeRunFailure(result: ExportRun): string {
  const parts: string[] = [];
  if (result.error !== undefined) parts.push(result.error.message);
  if (result.status !== null) parts.push(`exit ${result.status}`);
  if (result.signal !== undefined && result.signal !== null) parts.push(`signal ${result.signal}`);
  const stderr = (result.stderr ?? "").trim();
  if (stderr !== "") {
    parts.push(stderr.split(/\r?\n/).slice(-3).join(" "));
  }
  return parts.length === 0 ? "unknown failure" : parts.join("; ");
}

export function exportDocs(options: {
  bin: string;
  key?: string;
  sourceBinary?: string;
  expected?: EngineVersion;
  timeoutMs?: number;
  deps?: EngineDeps;
}): ExportResult {
  if (options.bin.trim() === "") {
    throw new CliError(
      ExitCode.Transport,
      "cannot export engine docs without a Godot executable",
      "Pass --godot <path> to a Godot executable that matches the project.",
    );
  }
  const deps = options.deps ?? {};
  const timeoutMs = options.timeoutMs ?? EXPORT_TIMEOUT_MS;
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-docs-"));
  try {
    const runner = deps.runExport ?? defaultRunExport;
    const result = runner(options.bin, temp, timeoutMs);
    if (result.error !== undefined || result.status !== 0) {
      throw new CliError(
        ExitCode.Transport,
        `failed to export engine docs from ${options.bin}: ${describeRunFailure(result)}`,
        "Pass --godot <path> to a Godot executable that matches the project.",
      );
    }
    const apiFile = path.join(temp, "extension_api.json");
    if (!fs.existsSync(apiFile)) {
      throw new CliError(
        ExitCode.Transport,
        `the engine at ${options.bin} did not produce extension_api.json`,
        "This Godot build does not support --dump-extension-api-with-docs; use a standard official build.",
      );
    }
    const text = fs.readFileSync(apiFile, "utf8");
    const index = parseDump(text);
    const node2d = index.classes.get("Node2D");
    if (node2d === undefined || node2d.brief.trim() === "") {
      throw new CliError(
        ExitCode.Transport,
        `the Godot build at ${options.bin} was compiled without documentation`,
        "Use a standard official Godot build that includes class reference text.",
      );
    }
    if (options.expected !== undefined) {
      const freshness = docsFreshness(index.header, options.expected);
      if (!freshness.fresh) {
        throw new CliError(
          ExitCode.Transport,
          `the engine at ${options.bin} produced Godot ${index.header.version_major}.${index.header.version_minor}.${index.header.version_patch}.${index.header.version_status}, expected ${options.expected.major}.${options.expected.minor}${options.expected.patch === undefined ? "" : `.${options.expected.patch}`}`,
          "Pass --godot <path> to a Godot executable that matches the project.",
        );
      }
    }
    const key = options.key ?? bucketKey(engineVersionFromHeader(index.header));
    const now = deps.now?.() ?? new Date();
    const meta = writeBucket(key, text, {
      schema: 1,
      key,
      engine: {
        major: index.header.version_major,
        minor: index.header.version_minor,
        patch: index.header.version_patch,
        status: index.header.version_status,
        full_name: index.header.version_full_name ?? "",
      },
      source_binary: options.sourceBinary ?? options.bin,
      synced_at: now.toISOString(),
      bytes: 0,
      sha256: "",
    });
    process.stderr.write(
      `[gda] exported Godot ${key} docs from ${options.bin} (${meta.bytes} bytes)\n`,
    );
    return { index, meta, apiPath: path.join(docsRoot(), key, "extension_api.json") };
  } finally {
    fs.rmSync(temp, { recursive: true, force: true });
  }
}

function binaryFor(engine: EngineResolution, deps: EngineDeps): string {
  if (engine.binary.trim() !== "") return engine.binary;
  return (deps.resolveBinary ?? (() => resolveGodotBin({})))().bin;
}

function assertProjectBinaryMatches(engine: EngineResolution, bin: string, deps: EngineDeps): void {
  if (engine.source !== "project" || engine.binary.trim() !== "") return;
  const readVersion = deps.readVersion ?? readGodotVersion;
  const actual = readVersion(bin);
  if (actual.major !== engine.version.major || actual.minor !== engine.version.minor) {
    throw new CliError(
      ExitCode.Transport,
      `project.godot pins Godot ${engine.key}, but ${bin} reports ${actual.fullName ?? `${actual.major}.${actual.minor}`}`,
      `Pass --godot <path> to a Godot ${engine.key} executable, or update config/features in project.godot.`,
    );
  }
}

function cacheReport(bucket: DocsBucket, index: ApiIndex, verified: boolean): DocsCacheReport {
  return {
    status: "fresh",
    path: bucket.apiPath,
    synced_at: bucket.meta.synced_at,
    generated_by: index.header.version_full_name ?? "",
    verified,
  };
}

export function ensureDocs(engine: EngineResolution, deps: EngineDeps = {}): LoadedDocs {
  const bucket = readBucket(engine.key);
  if (bucket !== undefined) {
    try {
      const index = parseDump(bucket.text);
      const freshness = docsFreshness(index.header, engine.version);
      if (freshness.fresh) {
        return {
          engine,
          cache: { ...cacheReport(bucket, index, freshness.verified) },
          index,
          docsPath: bucket.apiPath,
        };
      }
    } catch {
    }
  }
  const bin = binaryFor(engine, deps);
  assertProjectBinaryMatches(engine, bin, deps);
  const exported = exportDocs({
    bin,
    key: engine.key,
    sourceBinary: bin,
    expected: engine.version,
    deps,
  });
  return {
    engine,
    cache: {
      status: "auto-exported",
      path: exported.apiPath,
      synced_at: exported.meta.synced_at,
      generated_by: exported.index.header.version_full_name ?? "",
      verified: true,
    },
    index: exported.index,
    docsPath: exported.apiPath,
  };
}

export function loadExternalDocs(filePath: string): LoadedDocs {
  const resolved = path.resolve(filePath);
  let text: string;
  try {
    text = fs.readFileSync(resolved, "utf8");
  } catch (error) {
    throw new CliError(
      ExitCode.Transport,
      `cannot read --docs-file ${filePath}: ${errorMessage(error)}`,
    );
  }
  const index = parseDump(text);
  const version = engineVersionFromHeader(index.header);
  return {
    engine: { key: bucketKey(version), source: "explicit", binary: resolved, version },
    cache: {
      status: "external",
      path: resolved,
      synced_at: "",
      generated_by: index.header.version_full_name ?? "",
      verified: false,
    },
    index,
    docsPath: resolved,
  };
}

export function cacheStatus(engine: EngineResolution): CacheStatusReport {
  const file = bucketPath(engine.key);
  const bucket = readBucket(engine.key);
  if (bucket === undefined) {
    return { exists: false, path: file, generated_by: "", stale: true, next_query_will_export: true };
  }
  try {
    const index = parseDump(bucket.text);
    const freshness = docsFreshness(index.header, engine.version);
    return {
      exists: true,
      path: file,
      generated_by: index.header.version_full_name ?? "",
      stale: !freshness.fresh,
      next_query_will_export: !freshness.fresh,
    };
  } catch {
    return { exists: true, path: file, generated_by: "", stale: true, next_query_will_export: true };
  }
}
