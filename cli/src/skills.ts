import { createHash } from "node:crypto";
import fs from "node:fs";
import path from "node:path";

import builtinSkills from "../assets/skills.json";

import { applyManagedTree } from "./archive";
import { CliError, ExitCode } from "./errors";
import { isPathInside, isSafeSkillName } from "./security-policy";

export const SKILLS_MANIFEST_SCHEMA_VERSION = 1;
export const SKILLS_MANIFEST_FILE = ".gda-skills-manifest.json";

export interface SkillSummary {
  name: string;
  path: string;
  files: number;
}

export interface SkillsRoot {
  path: string;
  source: "repo" | "project" | "builtin";
}

export interface SkillAssetManifest {
  schema_version: number;
  version: string;
  file_count: number;
  content_digest: string;
  files: Record<string, string>;
  retired?: string[];
}

export interface InstalledSkillsManifest {
  schema_version: number;
  version: string;
  content_digest: string;
  tree_digest: string;
  files: string[];
  installed_at: string;
}

export interface SkillSyncPlan {
  target: string;
  version: string;
  content_digest: string;
  managed_books: string[];
  writes: Record<string, string>;
  overwrites: string[];
  deletes: string[];
  preserved: string[];
  retired_present: string[];
  retired_removed: string[];
  old_version: string | null;
  dry_run: boolean;
  force: boolean;
  prune: boolean;
}

export interface SkillSyncResult {
  ok: boolean;
  applied: boolean;
  written: string[];
  deleted: string[];
  retired_removed: string[];
  preserved: string[];
  rolled_back: boolean;
  error: string;
}

export interface InstalledVerification {
  ok: boolean;
  reason: string;
  version: string | null;
}

interface InstalledManifestRead {
  manifest: InstalledSkillsManifest | null;
  corrupt: boolean;
  reason: string;
}

const asset = builtinSkills as SkillAssetManifest;

export function skillTreeDigest(files: Record<string, string>): string {
  const keys = Object.keys(files).sort();
  const parts: string[] = [String(SKILLS_MANIFEST_SCHEMA_VERSION), String(keys.length)];
  for (const key of keys) {
    const content = files[key] ?? "";
    parts.push(`${key}\t${content.length}`, content);
  }
  return createHash("sha256").update(parts.join("\n"), "utf8").digest("hex");
}

export function skillsContentDigest(
  version: string,
  files: Record<string, string>,
): string {
  const parts = [
    String(SKILLS_MANIFEST_SCHEMA_VERSION),
    version,
    skillTreeDigest(files),
  ];
  return createHash("sha256").update(parts.join("\n"), "utf8").digest("hex");
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

export function verifySkillAssetManifest(value: unknown): boolean {
  if (!isRecord(value)) return false;
  if (value.schema_version !== SKILLS_MANIFEST_SCHEMA_VERSION) return false;
  if (typeof value.version !== "string" || value.version === "") return false;
  if (typeof value.file_count !== "number" || !Number.isInteger(value.file_count)) return false;
  if (typeof value.content_digest !== "string" || !/^[0-9a-f]{64}$/.test(value.content_digest)) return false;
  if (!isRecord(value.files)) return false;
  if (Object.keys(value.files).length !== value.file_count) return false;
  const files = value.files as Record<string, string>;
  if (Object.values(files).some((content) => typeof content !== "string")) return false;
  return skillsContentDigest(value.version, files) === value.content_digest;
}

export function verifyBuiltinSkillsAsset(): boolean {
  return verifySkillAssetManifest(asset);
}

export function builtinSkillsManifest(): SkillAssetManifest {
  return asset;
}

export function builtinSkillsFiles(): Record<string, string> {
  return asset.files;
}

export function builtinSkillsVersion(): string {
  return asset.version;
}

export function builtinSkillsDigest(): string {
  return asset.content_digest ?? "";
}

export function builtinSkillFileCount(): number {
  return asset.file_count ?? Object.keys(asset.files).length;
}

export function normalizeRetiredSkills(value: unknown): string[] {
  if (!Array.isArray(value)) return [];
  const names = value.filter(
    (name): name is string => typeof name === "string" && /^[a-z0-9]+(-[a-z0-9]+)*$/.test(name),
  );
  return [...new Set(names)].sort((a, b) => a.localeCompare(b));
}

export function builtinRetiredSkills(): string[] {
  return normalizeRetiredSkills(asset.retired);
}

function countFiles(dir: string): number {
  let total = 0;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    if (entry.isDirectory()) {
      total += countFiles(path.join(dir, entry.name));
    } else if (entry.isFile()) {
      total += 1;
    }
  }
  return total;
}

export function resolveSkillsRoot(): SkillsRoot {
  const repoDir = "skills";
  const candidates: SkillsRoot[] = [
    { path: path.resolve(import.meta.dir, "..", "..", repoDir), source: "repo" },
    { path: path.join(process.cwd(), ".agents", "skills"), source: "project" },
  ];
  for (const candidate of candidates) {
    if (fs.existsSync(candidate.path) && fs.statSync(candidate.path).isDirectory()) {
      return candidate;
    }
  }
  return { path: "builtin", source: "builtin" };
}

export function listSkills(root: SkillsRoot): SkillSummary[] {
  if (root.source === "builtin") {
    const files = builtinSkillsFiles();
    const names = new Set<string>();
    for (const file of Object.keys(files)) {
      const [name, ...rest] = file.split("/");
      if (name !== undefined && rest.length === 1 && rest[0] === "SKILL.md") {
        names.add(name);
      }
    }
    return [...names]
      .sort((a, b) => a.localeCompare(b))
      .map((name) => ({
        name,
        path: `builtin:${name}`,
        files: Object.keys(files).filter((file) => file.startsWith(`${name}/`)).length,
      }));
  }
  const books: SkillSummary[] = [];
  for (const entry of fs.readdirSync(root.path, { withFileTypes: true })) {
    if (!entry.isDirectory()) {
      continue;
    }
    const dir = path.join(root.path, entry.name);
    if (!fs.existsSync(path.join(dir, "SKILL.md"))) {
      continue;
    }
    books.push({ name: entry.name, path: dir, files: countFiles(dir) });
  }
  books.sort((a, b) => a.name.localeCompare(b.name));
  return books;
}

export function readSkillFile(
  root: SkillsRoot,
  name: string,
  relative?: string,
): string {
  if (!isSafeSkillName(name)) {
    throw new CliError(ExitCode.Usage, `invalid skill name: ${name}`);
  }
  if (root.source === "builtin") {
    const target = relative === undefined ? "SKILL.md" : relative;
    const normalized = path.posix.normalize(target);
    if (normalized.startsWith("..") || path.posix.isAbsolute(normalized)) {
      throw new CliError(ExitCode.Usage, `--file must stay inside the skill directory: ${relative}`);
    }
    const key = `${name}/${normalized}`;
    const content = builtinSkillsFiles()[key];
    if (content === undefined) {
      throw new CliError(ExitCode.Usage, `skill file not found: ${name}/${target}`);
    }
    return content;
  }
  const bookDir = path.join(root.path, name);
  if (!fs.existsSync(bookDir) || !fs.statSync(bookDir).isDirectory()) {
    throw new CliError(ExitCode.Usage, `unknown skill: ${name} (run 'gda skills list')`);
  }
  const target = relative === undefined ? "SKILL.md" : relative;
  const resolved = path.resolve(bookDir, target);
  const normalizedBook = path.resolve(bookDir) + path.sep;
  if (!resolved.startsWith(normalizedBook)) {
    throw new CliError(ExitCode.Usage, `--file must stay inside the skill directory: ${relative}`);
  }
  if (!fs.existsSync(resolved) || !fs.statSync(resolved).isFile()) {
    throw new CliError(ExitCode.Usage, `skill file not found: ${name}/${target}`);
  }
  const rootReal = fs.realpathSync(path.resolve(root.path));
  const bookReal = fs.realpathSync(bookDir);
  const targetReal = fs.realpathSync(resolved);
  if (!isPathInside(rootReal, bookReal) || !isPathInside(bookReal, targetReal)) {
    throw new CliError(ExitCode.Usage, `--file must stay inside the skill directory: ${relative}`);
  }
  return fs.readFileSync(targetReal, "utf8");
}

function managedBooks(paths: string[]): string[] | null {
  const books = new Set<string>();
  for (const relative of paths) {
    if (relative.includes("\\") || relative.includes("\0")) return null;
    const normalized = path.posix.normalize(relative);
    if (normalized !== relative || normalized.startsWith("..") || path.posix.isAbsolute(normalized)) return null;
    const parts = normalized.split("/");
    if (parts.length < 2 || parts.some((part) => part === "" || part === "." || part === "..")) return null;
    if (!isSafeSkillName(parts[0] ?? "")) return null;
    books.add(parts[0] ?? "");
  }
  return [...books].sort((a, b) => a.localeCompare(b));
}

export function isManagedReferencePath(relative: string, books: string[]): boolean {
  if (!relative.endsWith(".md")) return false;
  const parts = relative.split("/");
  if (parts.length < 2 || !books.includes(parts[0] ?? "")) return false;
  if (parts[1] === "SKILL.md") return parts.length === 2;
  if (parts[1] !== "references") return false;
  return parts.slice(2).every((part) => part !== "" && part !== "." && part !== "..");
}

function parseInstalledManifest(raw: unknown): InstalledSkillsManifest | null {
  if (!isRecord(raw)) return null;
  if (raw.schema_version !== SKILLS_MANIFEST_SCHEMA_VERSION) return null;
  if (typeof raw.version !== "string" || raw.version === "") return null;
  if (typeof raw.content_digest !== "string" || raw.content_digest === "") return null;
  if (typeof raw.tree_digest !== "string" || !/^[0-9a-f]{64}$/.test(raw.tree_digest)) return null;
  if (!Array.isArray(raw.files) || raw.files.some((item) => typeof item !== "string")) return null;
  if (typeof raw.installed_at !== "string") return null;
  return {
    schema_version: SKILLS_MANIFEST_SCHEMA_VERSION,
    version: raw.version,
    content_digest: raw.content_digest,
    tree_digest: raw.tree_digest,
    files: [...(raw.files as string[])].sort(),
    installed_at: raw.installed_at,
  };
}

export function installedManifestPath(root: string): string {
  return path.join(root, SKILLS_MANIFEST_FILE);
}

export function inspectInstalledManifest(root: string): InstalledManifestRead {
  const file = installedManifestPath(root);
  if (!fs.existsSync(file)) return { manifest: null, corrupt: false, reason: "not_installed" };
  let raw: unknown;
  try {
    raw = JSON.parse(fs.readFileSync(file, "utf8"));
  } catch {
    return { manifest: null, corrupt: true, reason: "invalid installed manifest JSON" };
  }
  const manifest = parseInstalledManifest(raw);
  if (manifest === null) return { manifest: null, corrupt: true, reason: "invalid installed manifest schema" };
  return { manifest, corrupt: false, reason: "" };
}

export function readInstalledManifest(root: string): InstalledSkillsManifest | null {
  return inspectInstalledManifest(root).manifest;
}

export function writeInstalledManifest(root: string, manifest: InstalledSkillsManifest): void {
  const target = installedManifestPath(root);
  fs.mkdirSync(path.dirname(target), { recursive: true });
  const tmp = `${target}.${process.pid}.tmp`;
  try {
    fs.writeFileSync(tmp, `${JSON.stringify(manifest, null, 2)}\n`, "utf8");
    fs.renameSync(tmp, target);
  } finally {
    fs.rmSync(tmp, { force: true });
  }
}

export function verifyInstalledSkills(root: string): InstalledVerification {
  const inspected = inspectInstalledManifest(root);
  if (inspected.manifest === null) {
    return { ok: !inspected.corrupt, reason: inspected.reason, version: null };
  }
  const manifest = inspected.manifest;
  const books = managedBooks(manifest.files);
  if (books === null || books.length === 0) {
    return { ok: false, reason: "installed manifest declares paths outside a managed book", version: manifest.version };
  }
  const files: Record<string, string> = {};
  for (const relative of manifest.files) {
    if (!isManagedReferencePath(relative, books)) {
      return { ok: false, reason: `installed manifest declares an out-of-scope path: ${relative}`, version: manifest.version };
    }
    const target = path.join(root, ...relative.split("/"));
    if (!fs.existsSync(target) || !fs.statSync(target).isFile()) {
      return { ok: false, reason: `missing managed file: ${relative}`, version: manifest.version };
    }
    files[relative] = fs.readFileSync(target, "utf8");
  }
  if (skillTreeDigest(files) !== manifest.tree_digest) {
    return { ok: false, reason: "installed skills content digest mismatch", version: manifest.version };
  }
  return {
    ok: true,
    reason: "ok",
    version: manifest.version,
  };
}

function walkManagedFiles(dir: string, prefix: string): string[] {
  const found: string[] = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    if (entry.isSymbolicLink()) continue;
    const relative = `${prefix}/${entry.name}`;
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      found.push(...walkManagedFiles(full, relative));
    } else if (entry.isFile() && entry.name.endsWith(".md")) {
      found.push(relative);
    }
  }
  return found;
}

export function planSkillSync(
  target: string,
  files: Record<string, string>,
  options: {
    version: string;
    content_digest: string;
    dryRun?: boolean;
    force?: boolean;
    prune?: boolean;
  },
): SkillSyncPlan {
  const desired = Object.keys(files);
  const books = managedBooks(desired);
  if (books === null || books.length === 0) {
    throw new CliError(ExitCode.Transport, "skill asset declares unsafe managed paths");
  }
  for (const relative of desired) {
    if (!isManagedReferencePath(relative, books)) {
      throw new CliError(ExitCode.Transport, `skill asset declares an out-of-scope path: ${relative}`);
    }
  }
  const desiredSet = new Set(desired);
  const inspected = inspectInstalledManifest(target);
  if (inspected.corrupt && options.force !== true) {
    throw new CliError(
      ExitCode.Transport,
      `installed skills manifest is corrupt: ${inspected.reason}`,
      "Run 'gda skills sync --force' to rebuild the managed files.",
    );
  }
  const old = inspected.manifest;
  const deletes = new Set<string>();
  if (old !== null) {
    const oldBooks = managedBooks(old.files);
    if (oldBooks === null) {
      throw new CliError(ExitCode.Transport, "installed manifest declares unsafe managed paths");
    }
    for (const relative of old.files) {
      if (!isManagedReferencePath(relative, books)) {
        throw new CliError(
          ExitCode.Transport,
          `installed manifest declares an out-of-scope path: ${relative}`,
          "Refusing to remove a path outside the managed skill books.",
        );
      }
      if (!desiredSet.has(relative)) deletes.add(relative);
    }
  }
  for (const book of books) {
    const dir = path.join(target, book);
    if (!fs.existsSync(dir) || !fs.statSync(dir).isDirectory()) continue;
    for (const relative of walkManagedFiles(dir, book)) {
      if (isManagedReferencePath(relative, books) && !desiredSet.has(relative)) deletes.add(relative);
    }
  }
  const overwrites = desired.filter((relative) => {
    const file = path.join(target, ...relative.split("/"));
    if (!fs.existsSync(file) || !fs.statSync(file).isFile()) return false;
    return fs.readFileSync(file, "utf8") !== files[relative];
  });
  const retired = new Set(builtinRetiredSkills());
  const preserved: string[] = [];
  const retiredPresent: string[] = [];
  let entries: fs.Dirent[] = [];
  try {
    entries = fs.readdirSync(target, { withFileTypes: true });
  } catch {
    entries = [];
  }
  for (const entry of entries) {
    if (!entry.isDirectory() || books.includes(entry.name)) continue;
    if (retired.has(entry.name)) retiredPresent.push(entry.name);
    else preserved.push(entry.name);
  }
  const prune = options.prune === true;
  return {
    target,
    version: options.version,
    content_digest: options.content_digest,
    managed_books: books,
    writes: { ...files },
    overwrites: overwrites.sort(),
    deletes: [...deletes].sort(),
    preserved: preserved.sort(),
    retired_present: retiredPresent.sort(),
    retired_removed: prune ? [...retiredPresent].sort() : [],
    old_version: old?.version ?? null,
    dry_run: options.dryRun === true,
    force: options.force === true,
    prune,
  };
}

export function applySkillSync(plan: SkillSyncPlan): SkillSyncResult {
  const preserved = [...plan.preserved];
  if (plan.dry_run) {
    return {
      ok: true,
      applied: false,
      written: Object.keys(plan.writes).sort(),
      deleted: [...plan.deletes],
      retired_removed: [...plan.retired_removed],
      preserved,
      rolled_back: false,
      error: "",
    };
  }
  if (!verifyBuiltinSkillsAsset()) {
    return {
      ok: false,
      applied: false,
      written: [],
      deleted: [],
      retired_removed: [],
      preserved,
      rolled_back: false,
      error: "builtin skills asset digest mismatch",
    };
  }
  const manifest: InstalledSkillsManifest = {
    schema_version: SKILLS_MANIFEST_SCHEMA_VERSION,
    version: plan.version,
    content_digest: plan.content_digest,
    tree_digest: skillTreeDigest(plan.writes),
    files: Object.keys(plan.writes).sort(),
    installed_at: new Date().toISOString(),
  };
  const backups: Array<{ dir: string; backup: string }> = [];
  try {
    fs.mkdirSync(plan.target, { recursive: true });
    for (const name of plan.retired_removed) {
      if (!isSafeSkillName(name)) {
        throw new CliError(ExitCode.Transport, `unsafe retired skill name: ${name}`);
      }
      const dir = path.join(plan.target, name);
      if (!fs.existsSync(dir) || !fs.statSync(dir).isDirectory()) continue;
      const backup = `${dir}.${process.pid}.gda-backup`;
      fs.rmSync(backup, { recursive: true, force: true });
      fs.renameSync(dir, backup);
      backups.push({ dir, backup });
    }
    const applied = applyManagedTree(
      plan.target,
      {
        writes: { ...plan.writes, [SKILLS_MANIFEST_FILE]: `${JSON.stringify(manifest, null, 2)}\n` },
        deletes: plan.deletes,
      },
      { force: plan.force },
    );
    for (const item of backups) fs.rmSync(item.backup, { recursive: true, force: true });
    return {
      ok: true,
      applied: true,
      written: applied.written,
      deleted: applied.deleted,
      retired_removed: [...plan.retired_removed],
      preserved,
      rolled_back: false,
      error: "",
    };
  } catch (error) {
    for (const item of backups.reverse()) {
      try {
        if (fs.existsSync(item.backup)) {
          if (fs.existsSync(item.dir)) fs.rmSync(item.dir, { recursive: true, force: true });
          fs.renameSync(item.backup, item.dir);
        }
      } catch {
      }
    }
    return {
      ok: false,
      applied: false,
      written: [],
      deleted: [],
      retired_removed: [],
      preserved,
      rolled_back: true,
      error: String(error),
    };
  }
}
