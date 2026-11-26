import fs from "node:fs";

import type { GlobalOptions } from "../args";
import { nativeToolInvoke, type NativeCallOutcome } from "../discovery";
import { CliError, ExitCode } from "../errors";
import { findById, findLiveByProject } from "../registry";
import { resolveNativeTarget } from "../target";
import { cliTraceDir, readTraceRecords, stringField, type TraceRecord } from "../trace";
import { writeJson } from "./json-output";
import { readRawEntry } from "./registry-raw";
import { flagBool, flagNumber, flagString } from "./scene";

const DEFAULT_TAIL = 50;
const TARGET_CONTEXT = { findById, findLiveByProject };

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  try {
    return await logs(positional, opts);
  } catch (error) {
    if (error instanceof CliError) {
      process.stderr.write(`[gda] ${error.message}\n`);
      if (error.suggestion !== undefined) {
        process.stderr.write(`[gda] suggestion: ${error.suggestion}\n`);
      }
      return error.code;
    }
    throw error;
  }
}

export function parseTail(text: string | number | undefined): number {
  if (text === undefined) return DEFAULT_TAIL;
  const n = Number(text);
  if (!Number.isInteger(n) || n < 0) {
    throw new CliError(ExitCode.Usage, `--tail must be a non-negative integer, got: ${text}`);
  }
  return n;
}

export function selectTail<T>(lines: T[], tail: number): T[] {
  if (tail === 0) return [];
  return tail >= lines.length ? [...lines] : lines.slice(-tail);
}

function readLogLines(logPath: string, id: string): string[] {
  let text: string;
  try {
    text = fs.readFileSync(logPath, "utf8");
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code === "ENOENT") {
      throw new CliError(ExitCode.Transport, `log file missing for instance ${id}: ${logPath}`);
    }
    throw error;
  }
  const lines = text.split(/\r?\n/);
  if (lines.length > 0 && lines[lines.length - 1] === "") lines.pop();
  return lines;
}

function readEditorLog(id: string | undefined, opts: GlobalOptions): { id: string; lines: string[] } {
  const selected = id ?? opts.id;
  if (selected === undefined || selected === "") {
    throw new CliError(ExitCode.Usage, "usage: gda logs editor <id> [--tail <n>] (or --id)");
  }
  const entry = readRawEntry(selected);
  if (entry === undefined) {
    throw new CliError(ExitCode.Transport, `unknown instance: ${selected} (run "gda editor list" to see running instances)`);
  }
  return { id: selected, lines: readLogLines(entry.log, selected) };
}

function readCliLog(opts: GlobalOptions): { id: string; lines: string[] } {
  const records: TraceRecord[] = readTraceRecords();
  const lines: string[] = [];
  for (const record of records) {
    const line = JSON.stringify(record);
    if (opts.raw === true) {
      lines.push(line);
      continue;
    }
    const kind = stringField(record, "kind");
    const source = stringField(record, "source");
    const tool = stringField(record, "tool") || stringField(record, "name");
    lines.push(`${kind || "cli"}${source === "" ? "" : ` [${source}]`}${tool === "" ? "" : ` ${tool}`} ${line}`);
  }
  const dir = cliTraceDir();
  if (lines.length === 0 && !fs.existsSync(dir)) {
    lines.push("[gda] no CLI trace records yet");
  }
  return { id: "cli", lines };
}

function payloadEntries(payload: unknown): unknown[] {
  if (payload === null || typeof payload !== "object" || Array.isArray(payload)) return [];
  const record = payload as Record<string, unknown>;
  for (const key of ["entries", "lines", "errors", "logs", "items"]) {
    if (Array.isArray(record[key])) return record[key] as unknown[];
  }
  return [];
}

async function nativeLog(
  tool: string,
  args: Record<string, unknown>,
  opts: GlobalOptions,
): Promise<{ id: string; lines: string[] }> {
  const target = resolveNativeTarget(opts, TARGET_CONTEXT);
  if (target.ipcEndpoint === undefined) {
    throw new CliError(
      ExitCode.Transport,
      "no native IPC endpoint is available; pass --socket or --id from 'gda editor list'.",
    );
  }
  const instanceId = target.instanceId ?? target.pluginRecord?.instanceId ?? target.instance?.id ?? "";
  const outcome: NativeCallOutcome = await nativeToolInvoke(target.ipcEndpoint, instanceId, tool, args, {});
  const entries = payloadEntries(outcome.payload);
  return {
    id: instanceId,
    lines: entries.map((entry) => typeof entry === "string" ? entry : JSON.stringify(entry)),
  };
}

function fieldOf(line: string, key: string): string {
  if (!line.trimStart().startsWith("{")) return "";
  try {
    const parsed = JSON.parse(line) as Record<string, unknown>;
    const value = parsed[key];
    return typeof value === "string" ? value : "";
  } catch {
    return "";
  }
}

function timestampOf(line: string): number {
  if (!line.trimStart().startsWith("{")) return 0;
  try {
    const parsed = JSON.parse(line) as Record<string, unknown>;
    for (const key of ["ts", "time", "timestamp", "wall_end_ms"]) {
      const value = parsed[key];
      if (typeof value === "number" && Number.isFinite(value)) return value;
      if (typeof value === "string") {
        const numeric = Number(value);
        if (Number.isFinite(numeric)) return numeric;
        const parsedTime = Date.parse(value);
        if (Number.isFinite(parsedTime)) return parsedTime;
      }
    }
  } catch {
    return 0;
  }
  return 0;
}

const LEVEL_ORDER: Readonly<Record<string, number>> = {
  debug: 10,
  info: 20,
  warn: 30,
  warning: 30,
  error: 40,
  fatal: 50,
};

function matches(line: string, opts: GlobalOptions, since: number): boolean {
  const level = flagString(opts, "--level");
  if (level !== undefined && level !== "") {
    const minimum = LEVEL_ORDER[level.toLowerCase()] ?? 40;
    const actual = LEVEL_ORDER[fieldOf(line, "level").toLowerCase()];
    if (actual === undefined || actual < minimum) return false;
  }
  const source = flagString(opts, "--source") ?? opts.sourceText;
  if (source !== undefined && source !== "" && !line.toLowerCase().includes(source.toLowerCase())) {
    return false;
  }
  if (since > 0) {
    const observed = timestampOf(line);
    if (observed > 0 && observed < since) return false;
  }
  return true;
}

function sinceMs(opts: GlobalOptions): number {
  const text = flagString(opts, "--since");
  if (text === undefined || text === "") return 0;
  const numeric = Number(text);
  if (Number.isFinite(numeric)) return numeric;
  const parsed = Date.parse(text);
  if (Number.isFinite(parsed)) return parsed;
  throw new CliError(ExitCode.Usage, `--since must be an ISO time or epoch-ms value, got: ${text}`);
}

function exportPath(opts: GlobalOptions): string {
  return flagString(opts, "--export") ?? "";
}

function emit(
  command: string,
  id: string,
  lines: string[],
  opts: GlobalOptions,
  extra: Record<string, unknown> = {},
): number {
  const destination = exportPath(opts);
  if (destination !== "") {
    fs.writeFileSync(destination, `${lines.join("\n")}${lines.length === 0 ? "" : "\n"}`, "utf8");
  }
  writeJson({
    schema_version: 1,
    ok: true,
    command,
    data: {
      id,
      source: command.slice("logs.".length),
      lines,
      count: lines.length,
      export_path: destination,
      ...extra,
    },
    meta: { instance_id: id },
    warnings: [],
  }, opts);
  return ExitCode.Success;
}

async function followPoll(
  opts: GlobalOptions,
  read: () => { id: string; lines: string[] },
  command: string,
): Promise<number> {
  const interval = flagNumber(opts, "--interval") ?? Number(opts.intervalText ?? "1");
  const seconds = Number.isFinite(interval) && interval > 0 ? interval : 1;
  const timeoutRaw = flagString(opts, "--timeout") ?? opts.timeoutText;
  const timeoutSeconds = timeoutRaw === undefined ? 5 : Number(timeoutRaw);
  if (!Number.isFinite(timeoutSeconds) || timeoutSeconds < 0) {
    throw new CliError(ExitCode.Usage, "--timeout must be a non-negative number of seconds");
  }
  const deadline = Date.now() + timeoutSeconds * 1000;
  let last: { id: string; lines: string[] } = read();
  let rounds = 0;
  for (;;) {
    rounds += 1;
    process.stderr.write(`${JSON.stringify({
      kind: "progress",
      command,
      round: rounds,
      count: last.lines.length,
    })}\n`);
    const wait = Math.min(seconds * 1000, Math.max(0, deadline - Date.now()));
    if (wait === 0) break;
    await new Promise((resolve) => setTimeout(resolve, wait));
    last = read();
  }
  return emit(command, last.id, last.lines, opts, { follow: true, rounds });
}

async function logs(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0] ?? "editor";
  if (sub === "show" || sub === "list") {
    const id = positional[1];
    const entry = id === undefined ? undefined : readRawEntry(id);
    if (sub === "list" || id === undefined || entry === undefined) {
      writeJson({ streams: id === undefined ? [] : [{ id, known: false }] }, opts);
      return ExitCode.Success;
    }
    const tail = parseTail(flagNumber(opts, "--tail") ?? opts.tailText);
    const lines = selectTail(readLogLines(entry.log, id), tail);
    if (lines.length > 0) process.stdout.write(`${lines.join("\n")}\n`);
    return ExitCode.Success;
  }

  if (sub !== "editor" && sub !== "game" && sub !== "plugin" && sub !== "cli") {
    const entry = readRawEntry(sub);
    if (entry === undefined) {
      throw new CliError(
        ExitCode.Transport,
        `unknown instance: ${sub} (run "gda editor list" to see running instances)`,
        "run 'gda list' to refresh the instance registry.",
      );
    }
    const tail = parseTail(flagNumber(opts, "--tail") ?? opts.tailText);
    const lines = selectTail(readLogLines(entry.log, sub), tail);
    if (lines.length > 0) process.stdout.write(`${lines.join("\n")}\n`);
    return ExitCode.Success;
  }

  const since = sinceMs(opts);
  const tail = parseTail(flagNumber(opts, "--tail") ?? opts.tailText);
  const read = (): { id: string; lines: string[] } => {
    if (sub === "editor") return readEditorLog(positional[1], opts);
    if (sub === "cli") return readCliLog(opts);
    throw new CliError(ExitCode.Usage, `unknown logs subcommand: ${sub} (use editor, game, plugin, or cli)`);
  };
  if (sub === "game" || sub === "plugin") {
    const tool = sub === "game" ? "get_game_log_entries" : "get_plugin_log";
    const limit = flagNumber(opts, "--limit") ?? (tail === 0 ? 0 : tail);
    const callArgs: Record<string, unknown> = { limit };
    const level = flagString(opts, "--level");
    const source = flagString(opts, "--source") ?? opts.sourceText;
    if (level !== undefined && level !== "") callArgs.level = level;
    if (source !== undefined && source !== "") callArgs.source = source;
    const fetchPage = async (): Promise<{ id: string; lines: string[] }> => nativeLog(tool, callArgs, opts);
    if (flagBool(opts, "--follow")) {
      throw new CliError(ExitCode.Usage, "logs follow for game/plugin requires a live poll loop; use --detach with job logs instead");
    }
    const page = await fetchPage();
    return emit(`logs.${sub}`, page.id, selectTail(page.lines.filter((line) => matches(line, opts, since)), tail), opts);
  }
  if (flagBool(opts, "--follow")) {
    return await followPoll(opts, () => {
      const page = read();
      return { id: page.id, lines: selectTail(page.lines.filter((line) => matches(line, opts, since)), tail) };
    }, `logs.${sub}`);
  }
  const page = read();
  return emit(`logs.${sub}`, page.id, selectTail(page.lines.filter((line) => matches(line, opts, since)), tail), opts);
}
