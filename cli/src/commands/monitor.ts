import type { GlobalOptions } from "../args";
import { nativeToolInvoke } from "../discovery";
import { CliError, ExitCode } from "../errors";
import { analyzeMonitorFile } from "../monitor";
import { assertCallFamilyUsage, callTimeoutSec } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveNativeTarget } from "../target";
import { writeJson } from "./json-output";

const DEFAULT_SLOW_MS = 33.3;
const DEFAULT_LIMIT = 50;

function asRecord(value: unknown): Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value)
    ? (value as Record<string, unknown>)
    : {};
}

function unwrapResult(display: unknown): Record<string, unknown> {
  const record = asRecord(display);
  return asRecord(record.result ?? record.data ?? display);
}

export function withImageExtension(pathname: string, extension = ".png"): string {
  const trimmed = pathname.trim();
  if (trimmed === "") return "";
  const normalized = extension.startsWith(".") ? extension : `.${extension}`;
  return trimmed.toLowerCase().endsWith(normalized.toLowerCase()) ? trimmed : `${trimmed}${normalized}`;
}

function optionNumber(opts: GlobalOptions, name: string): string | number | undefined {
  const value = (opts as unknown as Record<string, unknown>)[name];
  if (typeof value === "number" || typeof value === "string") return value;
  if (name === "--limit" && (typeof opts.limitText === "number" || typeof opts.limitText === "string")) {
    return opts.limitText;
  }
  return undefined;
}

function count(text: string | number | undefined, fallback: number, flagName: string): number {
  if (text === undefined) return fallback;
  const value = Number(text);
  if (!Number.isInteger(value) || value < 0) {
    throw new CliError(ExitCode.Usage, `${flagName} must be a non-negative integer, got: ${text}`);
  }
  return value;
}

function paginate<T>(items: T[], opts: GlobalOptions): { events: T[]; next_cursor: number; limit: number; count: number } {
  const cursor = count(optionNumber(opts, "--cursor"), 0, "--cursor");
  const limit = count(optionNumber(opts, "--limit"), DEFAULT_LIMIT, "--limit");
  const start = Math.min(cursor, items.length);
  const end = limit === 0 ? start : Math.min(items.length, start + limit);
  return { events: items.slice(start, end), next_cursor: end, limit, count: end - start };
}

function sessionIdOf(record: Record<string, unknown>): string {
  for (const key of ["session_id", "sessionId", "id"]) {
    const value = record[key];
    if (typeof value === "string" && value !== "") return value;
  }
  return "";
}

function normalizeSession(value: unknown): Record<string, unknown> {
  const record = asRecord(value);
  return { ...record, session_id: sessionIdOf(record) };
}

function emit(command: string, data: Record<string, unknown>, opts: GlobalOptions): number {
  writeJson({
    schema_version: 1,
    ok: true,
    command,
    data,
    meta: {
      session_id: typeof data.session_id === "string" ? data.session_id : "",
      instance_id: "",
    },
    warnings: [],
  }, opts);
  return ExitCode.Success;
}

async function callMonitor(action: string, args: Record<string, unknown>, opts: GlobalOptions): Promise<Record<string, unknown>> {
  const target = resolveNativeTarget(opts, { findById, findLiveByProject });
  if (target.ipcEndpoint === undefined) {
    throw new CliError(ExitCode.Transport, "no native IPC endpoint is available; pass --socket or --id from 'gda editor list'.");
  }
  const instanceId = target.instanceId ?? target.pluginRecord?.instanceId ?? target.instance?.id ?? "";
  const outcome = await nativeToolInvoke(
    target.ipcEndpoint,
    instanceId,
    "game_monitor",
    { action, ...args },
    { timeoutMs: Math.max(1, callTimeoutSec(opts)) * 1000 },
  );
  if (!outcome.ok) {
    throw new CliError(ExitCode.ToolFailure, `game_monitor ${action} failed: ${outcome.error_message}`);
  }
  return unwrapResult(outcome.payload);
}

function slowThreshold(opts: GlobalOptions): number {
  if (opts.slowText === undefined) {
    const raw = optionNumber(opts, "--slow");
    if (raw === undefined) return DEFAULT_SLOW_MS;
    const value = Number(raw);
    if (!Number.isFinite(value) || value < 0) throw new CliError(ExitCode.Usage, "--slow must be a non-negative number");
    return value;
  }
  const value = Number(opts.slowText);
  if (!Number.isFinite(value) || value < 0) {
    throw new CliError(ExitCode.Usage, "--slow must be a non-negative number");
  }
  return value;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("monitor", opts);
  const sub = positional[0] ?? "status";

  if (sub === "report" && opts.file !== undefined && opts.file !== "") {
    const report = analyzeMonitorFile(opts.file, slowThreshold(opts)) as unknown as Record<string, unknown>;
    return emit("monitor.report", { ...report, session_id: sessionIdOf(report) }, opts);
  }

  if (sub === "status") {
    const record = await callMonitor("status", {}, opts);
    return emit("monitor.status", normalizeSession(record), opts);
  }
  if (sub === "sessions") {
    const record = await callMonitor("sessions", {}, opts);
    const raw = Array.isArray(record.sessions) ? record.sessions : [];
    const page = paginate(raw.map(normalizeSession), opts);
    return emit("monitor.sessions", {
      sessions: page.events,
      next_cursor: page.next_cursor,
      limit: page.limit,
      count: page.count,
    }, opts);
  }
  if (sub === "report") {
    const session = positional[1];
    if (session === undefined || session === "") {
      throw new CliError(ExitCode.Usage, "monitor report requires a <session> or --file <path>");
    }
    const record = await callMonitor("report", { session, slow_ms: slowThreshold(opts) }, opts);
    return emit("monitor.report", normalizeSession(record), opts);
  }
  if (sub === "export") {
    const session = positional[1];
    if (session === undefined || session === "") {
      throw new CliError(ExitCode.Usage, "monitor export requires a <session>");
    }
    const record = await callMonitor("export", { session }, opts);
    const image = typeof record.image_path === "string"
      ? record.image_path
      : typeof record.image === "string" ? record.image : "";
    const explicit = (opts as unknown as Record<string, unknown>)["--image"];
    const imagePath = withImageExtension(typeof explicit === "string" ? explicit : image);
    return emit("monitor.export", {
      ...normalizeSession(record),
      session_id: sessionIdOf(record) || session,
      image_path: imagePath,
    }, opts);
  }
  throw new CliError(ExitCode.Usage, "unknown monitor subcommand: use status, sessions, report, or export");
}
