import fs from "node:fs";
import type { Dirent } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import { decodeUtf8Strict, toPosix } from "./guard_support";

const SELF_ROOT = path.resolve(fileURLToPath(new URL("../..", import.meta.url)));
const SKIP_DIRS = new Set([
  "third_party",
  "build",
  "node_modules",
  ".git",
  ".worktrees",
  ".handoff",
  "evidence",
  "dist",
  "gda_test_tmp",
]);
const HISTORY_DIRS = new Set(["changelog", "release-notes", "release_notes"]);
const TEXT_EXTENSIONS = new Set([
  ".bat",
  ".c",
  ".cc",
  ".cmake",
  ".cmd",
  ".cpp",
  ".cs",
  ".cxx",
  ".gdextension",
  ".gd",
  ".gdshader",
  ".h",
  ".hpp",
  ".in",
  ".js",
  ".json",
  ".jsonl",
  ".lock",
  ".md",
  ".markdown",
  ".mjs",
  ".ps1",
  ".sh",
  ".template",
  ".toml",
  ".ts",
  ".tsx",
  ".txt",
  ".xml",
  ".yaml",
  ".yml",
]);
const TEXT_BASENAMES = new Set([
  "CMakeLists.txt",
  ".env.template",
  ".gitattributes",
  ".gitignore",
  ".gitmodules",
]);
const SCAN_TREES: ReadonlyArray<readonly [string, Area]> = [
  ["src", "source"],
  ["csharp_bridge", "source"],
  ["cli/src", "cli"],
  ["cli/bin", "cli"],
  ["cli/scripts", "cli"],
  ["cli/assets", "cli"],
  ["cmake", "build"],
  ["devtools", "build"],
  ["tools", "build"],
  ["scripts", "build"],
  [".github", "build"],
  ["docs", "docs"],
  ["skills", "skills"],
  ["skills-zh", "skills"],
  [".agents/skills", "skills"],
];
const SCAN_FILES: ReadonlyArray<readonly [string, Area]> = [["cli/tsconfig.json", "cli"]];
const AREAS: Area[] = ["source", "cli", "build", "docs", "skills"];
const MAX_EXCERPT = 240;

type Area = "source" | "cli" | "build" | "docs" | "skills";

type ActiveFile = {
  absolute: string;
  relative: string;
  area: Area;
};

type Reference = {
  file: string;
  line: number;
  area: Area;
  occurrences: number;
  excerpt: string;
};

type FileTotal = {
  file: string;
  area: Area;
  occurrences: number;
  matching_lines: number;
};

type ScanResult = {
  references: Reference[];
  files: FileTotal[];
  byArea: Record<Area, number>;
  errors: string[];
};

function isTextFile(name: string): boolean {
  if (TEXT_BASENAMES.has(name)) return true;
  return TEXT_EXTENSIONS.has(path.extname(name).toLowerCase());
}

function isHistoryPath(relative: string): boolean {
  return relative.split("/").some((segment) => {
    const lower = segment.toLowerCase();
    return HISTORY_DIRS.has(lower) || /^(?:changelog|release[-_]notes)(?:\.|$)/.test(lower);
  });
}

function areaForRootFile(relative: string): Area {
  return path.extname(relative).toLowerCase() === ".md" ? "docs" : "build";
}

function pushActiveFile(
  root: string,
  absolute: string,
  area: Area,
  files: ActiveFile[],
): void {
  const relative = toPosix(path.relative(root, absolute));
  if (isHistoryPath(relative) || !isTextFile(path.basename(absolute))) return;
  const resolvedArea = relative === "cli/assets/skills.json" ? "skills" : area;
  files.push({ absolute, relative, area: resolvedArea });
}

function collectTree(
  root: string,
  directory: string,
  area: Area,
  files: ActiveFile[],
  errors: string[],
): void {
  const target = path.join(root, directory);
  if (!fs.existsSync(target)) return;
  const stack = [target];
  while (stack.length > 0) {
    const current = stack.pop() as string;
    let entries: Dirent[];
    try {
      entries = fs.readdirSync(current, { withFileTypes: true });
    } catch (error) {
      errors.push(`${toPosix(path.relative(root, current))}: read error: ${error instanceof Error ? error.message : String(error)}`);
      continue;
    }
    for (const entry of entries) {
      const absolute = path.join(current, entry.name);
      if (entry.isDirectory()) {
        if (SKIP_DIRS.has(entry.name.toLowerCase())) continue;
        stack.push(absolute);
      } else if (entry.isFile()) {
        pushActiveFile(root, absolute, area, files);
      }
    }
  }
}

function collectActiveFiles(root: string): { files: ActiveFile[]; errors: string[] } {
  const files: ActiveFile[] = [];
  const errors: string[] = [];
  for (const [directory, area] of SCAN_TREES) {
    collectTree(root, directory, area, files, errors);
  }
  for (const [relative, area] of SCAN_FILES) {
    const absolute = path.join(root, relative);
    if (fs.existsSync(absolute) && fs.statSync(absolute).isFile()) {
      pushActiveFile(root, absolute, area, files);
    }
  }

  let rootEntries: Dirent[];
  try {
    rootEntries = fs.readdirSync(root, { withFileTypes: true });
  } catch (error) {
    errors.push(`.: read error: ${error instanceof Error ? error.message : String(error)}`);
    rootEntries = [];
  }
  for (const entry of rootEntries) {
    if (!entry.isFile()) continue;
    pushActiveFile(root, path.join(root, entry.name), areaForRootFile(entry.name), files);
  }

  files.sort((left, right) => left.relative.localeCompare(right.relative));
  return { files, errors };
}

function excerptAround(line: string): string {
  const index = line.toLowerCase().indexOf("mcp");
  if (index === -1) return "";
  const start = Math.max(0, index - 80);
  const end = Math.min(line.length, start + MAX_EXCERPT);
  const prefix = start > 0 ? "..." : "";
  const suffix = end < line.length ? "..." : "";
  return `${prefix}${line.slice(start, end).trim()}${suffix}`;
}

function scanRoot(root: string): ScanResult {
  const { files, errors } = collectActiveFiles(root);
  const references: Reference[] = [];
  const totals = new Map<string, FileTotal>();
  const byArea: Record<Area, number> = { source: 0, cli: 0, build: 0, docs: 0, skills: 0 };

  for (const file of files) {
    let text: string;
    try {
      text = decodeUtf8Strict(fs.readFileSync(file.absolute));
    } catch (error) {
      errors.push(`${file.relative}: read error: ${error instanceof Error ? error.message : String(error)}`);
      continue;
    }
    let fileTotal = totals.get(file.relative);
    for (const [index, line] of text.split(/\r?\n/).entries()) {
      const occurrences = line.toLowerCase().split("mcp").length - 1;
      if (occurrences === 0) continue;
      references.push({
        file: file.relative,
        line: index + 1,
        area: file.area,
        occurrences,
        excerpt: excerptAround(line),
      });
      byArea[file.area] += occurrences;
      if (fileTotal === undefined) {
        fileTotal = { file: file.relative, area: file.area, occurrences: 0, matching_lines: 0 };
        totals.set(file.relative, fileTotal);
      }
      fileTotal.occurrences += occurrences;
      fileTotal.matching_lines += 1;
    }
  }

  return { references, files: [...totals.values()], byArea, errors };
}

function parseArgs(args: string[]): { root: string; strict: boolean; json: boolean } | null {
  let root = SELF_ROOT;
  let strict = false;
  let json = false;
  let rootSeen = false;
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    const value = args[index + 1];
    if (arg === "--strict") {
      strict = true;
    } else if (arg === "--json") {
      json = true;
    } else if (arg === "--root" && !rootSeen && value !== undefined && !value.startsWith("--")) {
      root = path.resolve(value);
      rootSeen = true;
      index += 1;
    } else {
      return null;
    }
  }
  return { root, strict, json };
}

function report(result: ScanResult, strict: boolean, json: boolean): number {
  if (result.errors.length > 0) {
    for (const item of result.errors) console.error(item);
    return 2;
  }

  const occurrences = AREAS.reduce((total, area) => total + result.byArea[area], 0);
  const mode = strict ? "strict" : "migration";
  const payload = {
    schema_version: 1,
    mode,
    totals: {
      occurrences,
      matching_lines: result.references.length,
      files: result.files.length,
    },
    by_area: result.byArea,
    files: result.files,
    references: result.references,
  };
  if (json) {
    console.log(JSON.stringify(payload, null, 2));
  } else {
    console.log(`no-mcp-guard: ${mode} mode: ${occurrences} MCP occurrence(s) in ${result.files.length} file(s)`);
    console.log(JSON.stringify(payload, null, 2));
  }

  if (strict && occurrences > 0) {
    console.error(`no-mcp-guard: strict mode found ${occurrences} MCP occurrence(s)`);
    return 1;
  }
  return 0;
}

function main(): number {
  const parsed = parseArgs(process.argv.slice(2));
  if (parsed === null) {
    console.error("usage: no_mcp_guard.ts [--root <dir>] [--strict] [--json]");
    return 2;
  }
  if (!fs.existsSync(parsed.root) || !fs.statSync(parsed.root).isDirectory()) {
    console.error(`no-mcp-guard: root is not a directory: ${parsed.root}`);
    return 2;
  }
  return report(scanRoot(parsed.root), parsed.strict, parsed.json);
}

process.exitCode = main();
