import fs from "node:fs";
import path from "node:path";

import { PROJECT_ROOT } from "../lib/config";
import { getCanonicalCatalog } from "./tool-audit";

export type HarnessSetupOptions = {
  project: string;
  headless: boolean;
};

export type HarnessCoverageOptions = {
  round: string;
  trace: string | null;
  json: boolean;
};

export type HarnessDigestOptions = {
  round: string;
  json: boolean;
};

export type StepRunner = (command: string[]) => number;
export type CaptureRunner = (command: string[]) => { exitCode: number; stdout: string };

export type CoverageTraceRecord = {
  tool: string;
  kind: string;
  ok: boolean;
  wallEndMs: number;
};

export type CoverageResult = {
  schema: number;
  catalog_total: number;
  used: string[];
  unused: string[];
  by_domain: Record<string, { total: number; used: number; unused: number }>;
  alias_excluded: Array<{ tool: string; count: number }>;
  round_start_ms: number;
  trace_snapshot: string;
  coverage_used: number;
  coverage_invalid_touches: number;
  evidence_rows: number;
  suspected_gaming: boolean;
};

export type CausalTriple = {
  action: string;
  condition: string;
  consequence: string;
};

export type DigestResult = {
  schema: number;
  round: string;
  q_items: string[];
  s_items: string[];
  levels: string[];
  playtime_minutes: number;
  causal_triples: CausalTriple[];
  coverage_gaps: string[];
  coverage_used: number;
  coverage_invalid_touches: number;
  evidence_rows: number;
  suspected_gaming: boolean;
};

const SNAPSHOT_NAME = "trace-snapshot.jsonl";

function message(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

function defaultStepRunner(command: string[]): number {
  try {
    const result = Bun.spawnSync(command, {
      cwd: PROJECT_ROOT,
      stdin: "inherit",
      stdout: "inherit",
      stderr: "inherit",
    });
    return result.exitCode ?? 1;
  } catch (error) {
    console.error(`[ERROR] failed to start ${command[0]}: ${message(error)}`);
    return 1;
  }
}

function defaultCaptureRunner(command: string[]): { exitCode: number; stdout: string } {
  try {
    const result = Bun.spawnSync(command, {
      cwd: PROJECT_ROOT,
      stdin: "ignore",
      stdout: "pipe",
      stderr: "pipe",
    });
    return { exitCode: result.exitCode ?? 1, stdout: result.stdout.toString() };
  } catch (error) {
    console.error(`[ERROR] failed to start ${command[0]}: ${message(error)}`);
    return { exitCode: 1, stdout: "" };
  }
}

export function setupCommandSequence(project: string, headless: boolean): string[][] {
  const start = ["gda", "start", "--project", project];
  if (headless) start.push("--headless");
  return [
    ["bun", "run", "gda-dev-tools", "build"],
    ["bun", "run", "gda-dev-tools", "deploy", "--project", project],
    start,
    ["gda", "list"],
    ["gda", "status", "--project", project, "--json"],
    ["gda", "ping", "--project", project],
    ["gda", "tools", "list"],
  ];
}

export async function runHarnessSetup(
  options: HarnessSetupOptions,
  runner: StepRunner = defaultStepRunner,
): Promise<number> {
  const steps = setupCommandSequence(options.project, options.headless);
  for (const command of steps) {
    console.log(`\n$ ${command.join(" ")}`);
    const code = runner(command);
    console.log(`[SETUP] exit=${code} ${command.join(" ")}`);
    if (code !== 0) {
      console.error(`[ERROR] harness setup stopped at: ${command.join(" ")} (exit ${code})`);
      return code;
    }
  }
  console.log("\n[DONE] harness setup complete.");
  return 0;
}

export function parseTraceJsonl(text: string): CoverageTraceRecord[] {
  const records: CoverageTraceRecord[] = [];
  for (const line of text.split(/\r?\n/)) {
    if (line.trim() === "") continue;
    let parsed: unknown;
    try {
      parsed = JSON.parse(line);
    } catch {
      continue;
    }
    if (typeof parsed !== "object" || parsed === null || Array.isArray(parsed)) continue;
    const obj = parsed as Record<string, unknown>;
    const tool =
      typeof obj.tool === "string" && obj.tool !== ""
        ? obj.tool
        : typeof obj.name === "string"
          ? obj.name
          : "";
    records.push({
      tool,
      kind: typeof obj.kind === "string" ? obj.kind : "",
      ok: obj.ok === true,
      wallEndMs:
        typeof obj.wall_end_ms === "number" && Number.isFinite(obj.wall_end_ms)
          ? obj.wall_end_ms
          : 0,
    });
  }
  return records;
}

export function parseRoundStartMs(roundName: string): number | undefined {
  const match = /^(\d{4})-(\d{2})-(\d{2})_(\d{2})-(\d{2})$/.exec(roundName);
  if (match === null) return undefined;
  const timestamp = new Date(
    Number(match[1]),
    Number(match[2]) - 1,
    Number(match[3]),
    Number(match[4]),
    Number(match[5]),
    0,
    0,
  ).getTime();
  return Number.isFinite(timestamp) ? timestamp : undefined;
}

export function countEvidenceRows(text: string): number {
  const lines = text.split(/\r?\n/);
  const tableRows = lines.filter((line) => {
    const trimmed = line.trim();
    return trimmed.startsWith("|") && trimmed.endsWith("|") && !/^\|[\s:\-|]+\|$/.test(trimmed);
  });
  if (tableRows.length > 1) return tableRows.length - 1;
  return lines.filter((line) => /img\/|trace-|截图|evidence/i.test(line)).length;
}

function domainKey(name: string): string {
  const underscore = name.indexOf("_");
  return underscore < 0 ? name : name.slice(0, underscore);
}

export function computeCoverage(
  records: CoverageTraceRecord[],
  catalog: string[],
  evidenceRows: number,
  roundStartMs?: number,
): CoverageResult {
  const canonical = new Set(catalog);
  const usedSet = new Set<string>();
  const aliasCounts = new Map<string, number>();
  let invalidTouches = 0;
  let roundStart = 0;
  const scoped =
    roundStartMs === undefined
      ? records
      : records.filter((record) => record.wallEndMs === 0 || record.wallEndMs >= roundStartMs);
  for (const record of scoped) {
    if (record.kind !== "tool_call" && record.tool === "") continue;
    if (record.wallEndMs > 0 && (roundStart === 0 || record.wallEndMs < roundStart)) {
      roundStart = record.wallEndMs;
    }
    if (!record.ok) invalidTouches += 1;
    if (record.tool === "") continue;
    if (canonical.has(record.tool)) usedSet.add(record.tool);
    else aliasCounts.set(record.tool, (aliasCounts.get(record.tool) ?? 0) + 1);
  }
  for (const count of aliasCounts.values()) invalidTouches += count;
  const used = [...usedSet].sort();
  const unused = catalog.filter((name) => !usedSet.has(name)).sort();
  const by_domain: CoverageResult["by_domain"] = {};
  for (const name of catalog) {
    const key = domainKey(name);
    const bucket = by_domain[key] ?? { total: 0, used: 0, unused: 0 };
    bucket.total += 1;
    if (usedSet.has(name)) bucket.used += 1;
    else bucket.unused += 1;
    by_domain[key] = bucket;
  }
  const alias_excluded = [...aliasCounts.entries()]
    .sort((a, b) => a[0].localeCompare(b[0]))
    .map(([tool, count]) => ({ tool, count }));
  const coverageUsed = used.length;
  return {
    schema: 1,
    catalog_total: catalog.length,
    used,
    unused,
    by_domain,
    alias_excluded,
    round_start_ms: roundStartMs ?? roundStart,
    trace_snapshot: SNAPSHOT_NAME,
    coverage_used: coverageUsed,
    coverage_invalid_touches: invalidTouches,
    evidence_rows: evidenceRows,
    suspected_gaming: coverageUsed > 0 && evidenceRows === 0,
  };
}

export type CoverageDeps = {
  runner?: CaptureRunner;
  catalog?: string[];
};

export function runHarnessCoverage(
  options: HarnessCoverageOptions,
  deps: CoverageDeps = {},
): number {
  const catalog = deps.catalog ?? getCanonicalCatalog();
  const runner = deps.runner ?? defaultCaptureRunner;
  let traceText: string;
  if (options.trace !== null) {
    try {
      traceText = fs.readFileSync(path.resolve(options.trace), "utf8");
    } catch (error) {
      console.error(`[ERROR] cannot read trace file ${options.trace}: ${message(error)}`);
      return 1;
    }
  } else {
    const result = runner(["gda", "trace", "export", "--format", "jsonl"]);
    if (result.exitCode !== 0) {
      console.error(`[ERROR] gda trace export failed (exit ${result.exitCode})`);
      return result.exitCode;
    }
    traceText = result.stdout;
  }
  const roundDir = path.resolve(options.round);
  const evidencePath = path.join(roundDir, "evidence.md");
  const evidenceRows = fs.existsSync(evidencePath)
    ? countEvidenceRows(fs.readFileSync(evidencePath, "utf8"))
    : 0;
  const roundStartMs = parseRoundStartMs(path.basename(roundDir));
  const result = computeCoverage(parseTraceJsonl(traceText), catalog, evidenceRows, roundStartMs);
  fs.mkdirSync(roundDir, { recursive: true });
  fs.writeFileSync(path.join(roundDir, result.trace_snapshot), traceText, "utf8");
  fs.writeFileSync(path.join(roundDir, "coverage.json"), `${JSON.stringify(result, null, 2)}\n`, "utf8");
  process.stdout.write(
    `[COVERAGE] catalog_total=${result.catalog_total} used=${result.coverage_used} unused=${result.unused.length} alias_excluded=${result.alias_excluded.length}\n`,
  );
  if (options.json) process.stdout.write(`${JSON.stringify(result, null, 2)}\n`);
  return 0;
}

export function extractIds(text: string, prefix: "Q" | "S"): string[] {
  const found = new Set<string>();
  const expression = new RegExp(`\\b${prefix}(\\d+)\\b`, "g");
  for (const match of text.matchAll(expression)) found.add(`${prefix}${match[1]}`);
  return [...found].sort((a, b) => Number(a.slice(1)) - Number(b.slice(1)));
}

export function extractLevels(text: string): string[] {
  const countMatch = text.match(/(?:关卡数|关数|levels)\s*[:：]?\s*(\d+)/i);
  if (countMatch) {
    const count = Number(countMatch[1]);
    if (Number.isFinite(count) && count > 0) {
      return Array.from({ length: Math.min(count, 100) }, (_, index) => `关卡${index + 1}`);
    }
  }
  const items: string[] = [];
  let inSection = false;
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (/^#{1,6}\s/.test(line)) {
      if (/关卡|level/i.test(line)) {
        inSection = true;
        continue;
      }
      if (inSection) break;
      continue;
    }
    if (!inSection) continue;
    const listItem = line.match(/^[-*+]\s+(.+)$/);
    if (listItem) {
      items.push(listItem[1].trim());
      continue;
    }
    const tableItem = line.match(/^\|\s*([^|]+?)\s*\|/);
    if (tableItem && !/^[\s:\-|]+$/.test(tableItem[1])) items.push(tableItem[1].trim());
  }
  return [...new Set(items)];
}

export function extractPlaytimeMinutes(text: string): number {
  const patterns = [
    /游玩时长\s*[:：]?\s*(\d+(?:\.\d+)?)\s*分钟/,
    /playtime_minutes\s*[:：]?\s*(\d+(?:\.\d+)?)/i,
    /单关[^\n]*?(\d+(?:\.\d+)?)\s*分钟/,
    /通关时长\s*[:：]?\s*(\d+(?:\.\d+)?)\s*分钟/,
  ];
  for (const pattern of patterns) {
    const match = text.match(pattern);
    if (match) {
      const value = Number(match[1]);
      if (Number.isFinite(value)) return value;
    }
  }
  return 0;
}

export function extractCausalTriples(text: string): CausalTriple[] {
  const triples: CausalTriple[] = [];
  const seen = new Set<string>();
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line.includes("→") && !line.includes("->")) continue;
    const segments = line.includes("|") ? line.split("|") : [line];
    for (const segment of segments) {
      const parts = segment
        .split(/→|->/)
        .map((part) => part.trim())
        .filter((part) => part.length > 0);
      if (parts.length < 3) continue;
      const triple: CausalTriple = {
        action: parts[0],
        condition: parts[1],
        consequence: parts.slice(2).join(" → "),
      };
      const key = `${triple.action}\u0000${triple.condition}\u0000${triple.consequence}`;
      if (seen.has(key)) continue;
      seen.add(key);
      triples.push(triple);
    }
  }
  return triples;
}

export function buildDigest(
  round: string,
  reportText: string,
  evidenceText: string,
  coverageText: string,
): DigestResult {
  if (reportText.trim() === "") throw new Error("report.md is empty");
  if (!/^#{1,6}\s/m.test(reportText) && extractIds(reportText, "Q").length === 0) {
    throw new Error("report.md has no recognizable structure");
  }
  if (evidenceText.trim() === "") throw new Error("evidence.md is empty");
  let coverage: Record<string, unknown>;
  try {
    coverage = JSON.parse(coverageText) as Record<string, unknown>;
  } catch {
    throw new Error("coverage.json is not valid JSON");
  }
  if (typeof coverage.catalog_total !== "number" || !Array.isArray(coverage.used)) {
    throw new Error("coverage.json missing catalog_total or used");
  }
  const used = (coverage.used as unknown[]).filter((name): name is string => typeof name === "string");
  const unused = Array.isArray(coverage.unused)
    ? (coverage.unused as unknown[]).filter((name): name is string => typeof name === "string")
    : [];
  const evidenceRows = countEvidenceRows(evidenceText);
  const coverageUsed =
    typeof coverage.coverage_used === "number" ? coverage.coverage_used : used.length;
  const coverageInvalid =
    typeof coverage.coverage_invalid_touches === "number" ? coverage.coverage_invalid_touches : 0;
  const combined = `${reportText}\n${evidenceText}`;
  return {
    schema: 1,
    round,
    q_items: extractIds(reportText, "Q"),
    s_items: extractIds(reportText, "S"),
    levels: extractLevels(combined),
    playtime_minutes: extractPlaytimeMinutes(combined),
    causal_triples: extractCausalTriples(combined),
    coverage_gaps: unused,
    coverage_used: coverageUsed,
    coverage_invalid_touches: coverageInvalid,
    evidence_rows: evidenceRows,
    suspected_gaming: coverageUsed > 0 && evidenceRows === 0,
  };
}

export function runHarnessDigest(options: HarnessDigestOptions): number {
  const roundDir = path.resolve(options.round);
  const reportPath = path.join(roundDir, "report.md");
  const evidencePath = path.join(roundDir, "evidence.md");
  const coveragePath = path.join(roundDir, "coverage.json");
  for (const input of [reportPath, evidencePath, coveragePath]) {
    if (!fs.existsSync(input)) {
      console.error(`[ERROR] missing digest input: ${input}`);
      return 1;
    }
  }
  let digest: DigestResult;
  try {
    digest = buildDigest(
      path.basename(roundDir),
      fs.readFileSync(reportPath, "utf8"),
      fs.readFileSync(evidencePath, "utf8"),
      fs.readFileSync(coveragePath, "utf8"),
    );
  } catch (error) {
    console.error(`[ERROR] digest parse failed: ${message(error)}`);
    return 1;
  }
  fs.writeFileSync(path.join(roundDir, "digest.json"), `${JSON.stringify(digest, null, 2)}\n`, "utf8");
  process.stdout.write(
    `[DIGEST] ${digest.round} q=${digest.q_items.length} s=${digest.s_items.length} levels=${digest.levels.length} gaps=${digest.coverage_gaps.length}\n`,
  );
  if (options.json) process.stdout.write(`${JSON.stringify(digest, null, 2)}\n`);
  return 0;
}
