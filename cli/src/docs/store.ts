import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";

import { CliError, ExitCode } from "../errors";
import { gdaRoot } from "../platform";
import type { EngineVersion } from "./api";

export interface DocsMetaEngine {
  major: number;
  minor: number;
  patch: number;
  status: string;
  full_name: string;
}

export interface DocsMeta {
  schema: number;
  key: string;
  engine: DocsMetaEngine;
  source_binary: string;
  synced_at: string;
  bytes: number;
  sha256: string;
}

export interface DocsBucket {
  key: string;
  dir: string;
  apiPath: string;
  metaPath: string;
  text: string;
  meta: DocsMeta;
}

export interface DocsBucketSummary {
  key: string;
  dir: string;
  meta: DocsMeta;
}

const BUCKET_PATTERN = /^\d+\.\d+(-[a-z0-9]+)?$/;

export function docsRoot(): string {
  return path.join(gdaRoot(), "docs");
}

export function bucketKey(version: EngineVersion): string {
  const base = `${version.major}.${version.minor}`;
  const status = (version.status ?? "stable").replace(/[^a-z0-9]+/g, "");
  if (status === "" || status === "stable") return base;
  return `${base}-${status}`;
}

export function bucketDir(key: string): string {
  if (!BUCKET_PATTERN.test(key)) {
    throw new CliError(ExitCode.Usage, `invalid engine key: ${key} (expected x.y or x.y-status)`);
  }
  return path.join(docsRoot(), key);
}

function isDocsMetaEngine(value: unknown): value is DocsMetaEngine {
  if (value === null || typeof value !== "object" || Array.isArray(value)) return false;
  const record = value as Record<string, unknown>;
  return (
    typeof record.major === "number" &&
    typeof record.minor === "number" &&
    typeof record.patch === "number" &&
    typeof record.status === "string" &&
    typeof record.full_name === "string"
  );
}

function isDocsMeta(value: unknown): value is DocsMeta {
  if (value === null || typeof value !== "object" || Array.isArray(value)) return false;
  const record = value as Record<string, unknown>;
  return (
    record.schema === 1 &&
    typeof record.key === "string" &&
    isDocsMetaEngine(record.engine) &&
    typeof record.source_binary === "string" &&
    typeof record.synced_at === "string" &&
    typeof record.bytes === "number" &&
    typeof record.sha256 === "string"
  );
}

function readMeta(key: string): DocsMeta | undefined {
  const dir = bucketDir(key);
  try {
    const parsed: unknown = JSON.parse(fs.readFileSync(path.join(dir, "meta.json"), "utf8"));
    if (!isDocsMeta(parsed) || parsed.key !== key) return undefined;
    return parsed;
  } catch {
    return undefined;
  }
}

export function bucketPath(key: string): string {
  return path.join(bucketDir(key), "extension_api.json");
}

export function readBucket(key: string): DocsBucket | undefined {
  const dir = bucketDir(key);
  const meta = readMeta(key);
  if (meta === undefined) return undefined;
  const apiPath = path.join(dir, "extension_api.json");
  try {
    const text = fs.readFileSync(apiPath, "utf8");
    if (text.trim() === "") return undefined;
    return { key, dir, apiPath, metaPath: path.join(dir, "meta.json"), text, meta };
  } catch {
    return undefined;
  }
}

function writeAtomic(target: string, content: string): void {
  const tmp = `${target}.${process.pid}.${Date.now()}.${Math.random().toString(16).slice(2)}.tmp`;
  try {
    fs.writeFileSync(tmp, content, "utf8");
    fs.renameSync(tmp, target);
  } finally {
    try {
      fs.rmSync(tmp, { force: true });
    } catch {
    }
  }
}

export function writeBucket(key: string, text: string, meta: DocsMeta): DocsMeta {
  const dir = bucketDir(key);
  fs.mkdirSync(dir, { recursive: true });
  const finalMeta: DocsMeta = {
    ...meta,
    schema: 1,
    key,
    bytes: Buffer.byteLength(text, "utf8"),
    sha256: crypto.createHash("sha256").update(text, "utf8").digest("hex"),
  };
  writeAtomic(path.join(dir, "extension_api.json"), text);
  writeAtomic(path.join(dir, "meta.json"), `${JSON.stringify(finalMeta, null, 2)}\n`);
  return finalMeta;
}

function compareKeys(left: string, right: string): number {
  const parse = (key: string): { major: number; minor: number; status: string } => {
    const match = /^(\d+)\.(\d+)(?:-(.+))?$/.exec(key);
    return {
      major: match === null ? 0 : Number(match[1]),
      minor: match === null ? 0 : Number(match[2]),
      status: match?.[3] ?? "stable",
    };
  };
  const a = parse(left);
  const b = parse(right);
  if (a.major !== b.major) return a.major - b.major;
  if (a.minor !== b.minor) return a.minor - b.minor;
  return a.status.localeCompare(b.status);
}

export function listBuckets(): DocsBucketSummary[] {
  let entries: fs.Dirent[];
  try {
    entries = fs.readdirSync(docsRoot(), { withFileTypes: true });
  } catch {
    return [];
  }
  const buckets: DocsBucketSummary[] = [];
  for (const entry of entries) {
    if (!entry.isDirectory()) continue;
    if (!BUCKET_PATTERN.test(entry.name)) continue;
    const meta = readMeta(entry.name);
    if (meta === undefined) continue;
    const dir = bucketDir(entry.name);
    if (!fs.existsSync(path.join(dir, "extension_api.json"))) continue;
    buckets.push({ key: entry.name, dir, meta });
  }
  return buckets.sort((a, b) => compareKeys(a.key, b.key));
}

export function clearBuckets(keys: string[]): string[] {
  const removed: string[] = [];
  for (const key of [...new Set(keys)].sort()) {
    const dir = bucketDir(key);
    if (!fs.existsSync(dir)) continue;
    fs.rmSync(dir, { recursive: true, force: true });
    removed.push(key);
  }
  return removed;
}
