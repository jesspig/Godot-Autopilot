import fs from "node:fs";
import path from "node:path";
import { randomBytes } from "node:crypto";

import { gdaRoot } from "./platform";

const CLI_TRACE_DIR = "cli-traces";
const PROJECT_TRACE_DIR = path.join("projects");
const TRACE_PREFIX = "trace-";
const TRACE_SUFFIX = ".jsonl";
const CLI_TRACE_MAX_AGE_MS = 7 * 24 * 60 * 60 * 1000;
const CLI_TRACE_MAX_BYTES = 100 * 1024 * 1024;
const TRACE_SCHEMA_VERSION = 4;

export interface TraceRecord {
  [key: string]: unknown;
  schema_version?: number;
  source?: string;
  trace_id?: string;
  span_id?: string;
  event_id?: string;
  tool?: string;
  name?: string;
  kind?: string;
  ok?: boolean;
  error_code?: string;
  duration_ms?: number;
  queue_wait_ms?: number;
  dispatch_wait_ms?: number;
  exec_ms?: number;
  serialize_ms?: number;
  wall_start_ms?: number;
  wall_end_ms?: number;
  transport?: string;
  retry_of?: string;
  attempt?: number;
}

export function newTraceId(): string {
  return randomBytes(16).toString("hex");
}

export function newSpanId(): string {
  return randomBytes(8).toString("hex");
}

export function makeTraceparent(traceId: string = newTraceId()): string {
  return `00-${traceId}-${newSpanId()}-01`;
}

export function traceIdFromTraceparent(value: string | undefined): string {
  if (value === undefined) return "";
  const parts = value.split("-");
  if (parts.length !== 4 || parts[1].length !== 32) return "";
  return /^[0-9a-f]+$/i.test(parts[1]) ? parts[1].toLowerCase() : "";
}

export function cliTraceDir(): string {
  return path.join(gdaRoot(), CLI_TRACE_DIR);
}

export function appendCliTrace(record: TraceRecord): void {
  const dir = cliTraceDir();
  try {
    fs.mkdirSync(dir, { recursive: true });
    const stamp = new Date().toISOString().slice(0, 10).replace(/-/g, "");
    const file = path.join(dir, `${TRACE_PREFIX}${stamp}${TRACE_SUFFIX}`);
    const line = JSON.stringify({ schema_version: TRACE_SCHEMA_VERSION, source: "cli", ...record });
    fs.appendFileSync(file, `${line}\n`, "utf8");
    pruneCliTraces(dir);
  } catch {
    return;
  }
}

export function readTraceRecords(): TraceRecord[] {
  const records: TraceRecord[] = [];
  for (const file of traceFiles(cliTraceDir())) {
    readJsonLines(file, records);
  }
  const projectsRoot = path.join(gdaRoot(), PROJECT_TRACE_DIR);
  for (const project of safeReadDir(projectsRoot)) {
    for (const file of traceFiles(path.join(projectsRoot, project, "traces"))) {
      readJsonLines(file, records);
    }
  }
  records.sort((a, b) => {
    const wall = numberField(a, "wall_end_ms") - numberField(b, "wall_end_ms");
    if (wall !== 0) return wall;
    return numberField(a, "seq") - numberField(b, "seq");
  });
  return records;
}

export function summarize(records: TraceRecord[]): Record<string, unknown> {
  const calls = records.filter((record) => stringField(record, "kind") === "tool_call");
  const durations = calls
    .map((record) => numberField(record, "duration_ms"))
    .filter((value) => value >= 0)
    .sort((a, b) => a - b);
  const failures = calls.filter((record) => record.ok === false);
  const toolCounts = new Map<string, number>();
  const errorCounts = new Map<string, number>();
  let retries = 0;
  let writeCalls = 0;
  let readAfterWrite = 0;
  for (const record of calls) {
    const tool = stringField(record, "tool") || stringField(record, "name");
    if (tool !== "") toolCounts.set(tool, (toolCounts.get(tool) ?? 0) + 1);
    if (record.retry_of !== undefined && String(record.retry_of) !== "") retries += 1;
  }
  for (const record of failures) {
    const code = stringField(record, "error_code") || "unknown";
    errorCounts.set(code, (errorCounts.get(code) ?? 0) + 1);
  }
  const ordered = [...calls].sort((a, b) => numberField(a, "wall_end_ms") - numberField(b, "wall_end_ms"));
  for (let index = 0; index < ordered.length; index += 1) {
    const record = ordered[index];
    const tool = stringField(record, "tool");
    if (!isWriteTool(tool)) continue;
    writeCalls += 1;
    const next = ordered[index + 1];
    if (next !== undefined && isReadTool(stringField(next, "tool"))) readAfterWrite += 1;
  }
  return {
    schema_version: TRACE_SCHEMA_VERSION,
    calls: calls.length,
    succeeded: calls.length - failures.length,
    failed: failures.length,
    success_rate: calls.length === 0 ? null : round((calls.length - failures.length) / calls.length, 4),
    latency_ms: {
      p50: percentile(durations, 0.5),
      p95: percentile(durations, 0.95),
      p99: percentile(durations, 0.99),
      max: durations.length === 0 ? null : durations[durations.length - 1],
    },
    latency_split_ms: averageSplit(calls),
    top_tools: topEntries(toolCounts, 10),
    top_errors: topEntries(errorCounts, 10),
    retries,
    read_after_write_ratio: writeCalls === 0 ? null : round(readAfterWrite / writeCalls, 4),
  };
}

export function traceFiles(dir: string): string[] {
  try {
    return fs
      .readdirSync(dir)
      .filter((name) => name.startsWith(TRACE_PREFIX) && name.endsWith(TRACE_SUFFIX))
      .map((name) => path.join(dir, name));
  } catch {
    return [];
  }
}

export function numberField(record: TraceRecord, key: string): number {
  const value = record[key];
  return typeof value === "number" && Number.isFinite(value) ? value : 0;
}

export function stringField(record: TraceRecord, key: string): string {
  const value = record[key];
  return typeof value === "string" ? value : "";
}

function readJsonLines(file: string, out: TraceRecord[]): void {
  let text: string;
  try {
    text = fs.readFileSync(file, "utf8");
  } catch {
    return;
  }
  for (const line of text.split(/\r?\n/)) {
    if (line.trim() === "") continue;
    try {
      const parsed = JSON.parse(line) as unknown;
      if (typeof parsed === "object" && parsed !== null && !Array.isArray(parsed)) {
        out.push(parsed as TraceRecord);
      }
    } catch {
      continue;
    }
  }
}

function safeReadDir(dir: string): string[] {
  try {
    return fs.readdirSync(dir, { withFileTypes: true }).filter((entry) => entry.isDirectory()).map((entry) => entry.name);
  } catch {
    return [];
  }
}

function pruneCliTraces(dir: string): void {
  const now = Date.now();
  const entries = traceFiles(dir)
    .map((file) => {
      try {
        const stat = fs.statSync(file);
        return { file, mtime: stat.mtimeMs, size: stat.size };
      } catch {
        return undefined;
      }
    })
    .filter((entry): entry is { file: string; mtime: number; size: number } => entry !== undefined)
    .sort((a, b) => b.mtime - a.mtime);
  let total = 0;
  for (const entry of entries) {
    total += entry.size;
    if (now - entry.mtime > CLI_TRACE_MAX_AGE_MS || total > CLI_TRACE_MAX_BYTES) {
      try {
        fs.rmSync(entry.file, { force: true });
      } catch {
        continue;
      }
      total -= entry.size;
    }
  }
}

function percentile(sorted: number[], ratio: number): number | null {
  if (sorted.length === 0) return null;
  const index = Math.min(sorted.length - 1, Math.max(0, Math.ceil(sorted.length * ratio) - 1));
  return sorted[index];
}

function averageSplit(calls: TraceRecord[]): Record<string, number | null> {
  if (calls.length === 0) {
    return { queue_wait: null, dispatch_wait: null, exec: null, serialize: null };
  }
  const sum = (key: string): number =>
    calls.reduce((total, record) => total + numberField(record, key), 0);
  return {
    queue_wait: round(sum("queue_wait_ms") / calls.length, 2),
    dispatch_wait: round(sum("dispatch_wait_ms") / calls.length, 2),
    exec: round(sum("exec_ms") / calls.length, 2),
    serialize: round(sum("serialize_ms") / calls.length, 2),
  };
}

function topEntries(counts: Map<string, number>, limit: number): Array<{ name: string; count: number }> {
  return [...counts.entries()]
    .sort((a, b) => b[1] - a[1])
    .slice(0, limit)
    .map(([name, count]) => ({ name, count }));
}

function isWriteTool(name: string): boolean {
  return /(^|_)(set|write|create|add|save|delete|remove|move|rename|attach|connect|apply|load|run|execute|start|stop|clear|erase|import|export)_/.test(name);
}

function isReadTool(name: string): boolean {
  return /(^|_)(get|list|read|search|find|describe|inspect|status|tree|query|show|count)_/.test(name);
}

function round(value: number, digits: number): number {
  const factor = 10 ** digits;
  return Math.round(value * factor) / factor;
}
