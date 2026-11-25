import fs from "node:fs";
import type { Dirent } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import { decodeUtf8Strict, toPosix } from "./guard_support";

const SELF_ROOT = path.resolve(fileURLToPath(new URL("../..", import.meta.url)));
const SOURCE_EXTENSIONS = new Set([".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"]);
const RULES: ReadonlyArray<{ name: string; pattern: RegExp }> = [
  { name: "mcp-json-value-type", pattern: /\bmcp\s*::\s*JsonValue\b/g },
  { name: "mcp-json-value-include", pattern: /^\s*#\s*include\s*[<"][^>"]*mcp\/JsonValue\.hpp[>"]/gim },
];

type Violation = {
  file: string;
  line: number;
  rule: string;
  occurrences: number;
};

type ScanResult = {
  violations: Violation[];
  files: Set<string>;
  byRule: Record<string, number>;
  errors: string[];
};

function collectSourceFiles(root: string): { files: string[]; errors: string[] } {
  const sourceRoot = path.join(root, "src");
  const files: string[] = [];
  const errors: string[] = [];
  if (!fs.existsSync(sourceRoot)) {
    return { files, errors: ["src: missing directory"] };
  }

  const stack = [sourceRoot];
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
        stack.push(absolute);
      } else if (entry.isFile() && SOURCE_EXTENSIONS.has(path.extname(entry.name).toLowerCase())) {
        files.push(absolute);
      }
    }
  }
  files.sort((left, right) => toPosix(path.relative(root, left)).localeCompare(toPosix(path.relative(root, right))));
  return { files, errors };
}

function scanRoot(root: string): ScanResult {
  const { files, errors } = collectSourceFiles(root);
  const violations: Violation[] = [];
  const matchedFiles = new Set<string>();
  const byRule: Record<string, number> = Object.fromEntries(RULES.map((rule) => [rule.name, 0]));

  for (const file of files) {
    const relative = toPosix(path.relative(root, file));
    let text: string;
    try {
      text = decodeUtf8Strict(fs.readFileSync(file));
    } catch (error) {
      errors.push(`${relative}: read error: ${error instanceof Error ? error.message : String(error)}`);
      continue;
    }
    for (const [index, line] of text.split(/\r?\n/).entries()) {
      for (const rule of RULES) {
        rule.pattern.lastIndex = 0;
        const occurrences = line.match(rule.pattern)?.length ?? 0;
        if (occurrences === 0) continue;
        violations.push({ file: relative, line: index + 1, rule: rule.name, occurrences });
        matchedFiles.add(relative);
        byRule[rule.name] += occurrences;
      }
    }
  }

  return { violations, files: matchedFiles, byRule, errors };
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

  const occurrences = Object.values(result.byRule).reduce((total, count) => total + count, 0);
  const mode = strict ? "strict" : "migration";
  const payload = {
    schema_version: 1,
    mode,
    totals: {
      occurrences,
      files: result.files.size,
    },
    by_rule: result.byRule,
    violations: result.violations,
  };
  if (json) {
    console.log(JSON.stringify(payload, null, 2));
  } else {
    console.log(`no-mcp-json-guard: ${mode} mode: ${occurrences} occurrence(s) in ${result.files.size} file(s)`);
    for (const rule of RULES) {
      console.log(`no-mcp-json-guard: ${rule.name} ${result.byRule[rule.name]} occurrence(s)`);
    }
  }

  if (strict && occurrences > 0) {
    for (const violation of result.violations) {
      console.error(`${violation.file}:${violation.line}: [${violation.rule}] ${violation.occurrences} occurrence(s)`);
    }
    console.error(`no-mcp-json-guard: strict mode found ${occurrences} occurrence(s)`);
    return 1;
  }
  return 0;
}

function main(): number {
  const parsed = parseArgs(process.argv.slice(2));
  if (parsed === null) {
    console.error("usage: no_mcp_json_guard.ts [--root <dir>] [--strict] [--json]");
    return 2;
  }
  if (!fs.existsSync(parsed.root) || !fs.statSync(parsed.root).isDirectory()) {
    console.error(`no-mcp-json-guard: root is not a directory: ${parsed.root}`);
    return 2;
  }
  return report(scanRoot(parsed.root), parsed.strict, parsed.json);
}

process.exitCode = main();
