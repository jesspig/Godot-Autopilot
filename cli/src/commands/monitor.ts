import { parseTransport, type GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { ensureConnection, callTool } from "../mcp/client";
import { analyzeMonitorFile } from "../monitor";
import { assertCallFamilyUsage, callTimeoutSec, extractPayload } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget } from "../target";
import { writeJson } from "./json-output";

const DEFAULT_SLOW_MS = 33.3;

function asRecord(value: unknown): Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value)
    ? (value as Record<string, unknown>)
    : {};
}

function unwrapResult(display: unknown): Record<string, unknown> {
  const record = asRecord(display);
  return asRecord(record.result ?? record.data ?? display);
}

async function callMonitor(action: string, args: Record<string, unknown>, opts: GlobalOptions): Promise<Record<string, unknown>> {
  const target = resolveTarget(opts, { findById, findLiveByProject });
  const transport = parseTransport(opts.transportText);
  if (transport !== "ipc") {
    await ensureConnection(target.port, callTimeoutSec(opts));
  }
  const envelope = await callTool("game_monitor", { action, ...args }, {
    port: target.port,
    timeoutSec: callTimeoutSec(opts),
    ipcEndpoint: target.ipcEndpoint,
    transport,
    noFallback: opts.noFallback,
  });
  const extracted = extractPayload(envelope);
  if (extracted.toolFailed) {
    throw new CliError(ExitCode.ToolFailure, `game_monitor ${action} failed`);
  }
  return unwrapResult(extracted.display);
}

function slowThreshold(opts: GlobalOptions): number {
  if (opts.slowText === undefined) return DEFAULT_SLOW_MS;
  const value = Number(opts.slowText);
  if (!Number.isFinite(value) || value < 0) {
    throw new CliError(ExitCode.Usage, "--slow must be a non-negative number");
  }
  return value;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("monitor", opts);
  const sub = positional[0] ?? "sessions";

  if (sub === "report" && opts.file !== undefined && opts.file !== "") {
    writeJson(analyzeMonitorFile(opts.file, slowThreshold(opts)), opts);
    return ExitCode.Success;
  }

  if (sub === "status") {
    writeJson(await callMonitor("status", {}, opts), opts);
    return ExitCode.Success;
  }
  if (sub === "sessions") {
    writeJson(await callMonitor("sessions", {}, opts), opts);
    return ExitCode.Success;
  }
  if (sub === "report") {
    const session = positional[1];
    if (session === undefined || session === "") {
      throw new CliError(ExitCode.Usage, "monitor report requires a <session> or --file <path>");
    }
    writeJson(await callMonitor("report", { session, slow_ms: slowThreshold(opts) }, opts), opts);
    return ExitCode.Success;
  }
  if (sub === "export") {
    const session = positional[1];
    if (session === undefined || session === "") {
      throw new CliError(ExitCode.Usage, "monitor export requires a <session>");
    }
    writeJson(await callMonitor("export", { session }, opts), opts);
    return ExitCode.Success;
  }
  throw new CliError(ExitCode.Usage, "unknown monitor subcommand: use status, sessions, report, export");
}
