import fs from "node:fs";

import type { GlobalOptions } from "../args";
import type { IpcEndpoint } from "../ipc/client";
import {
  nativeToolInvoke,
  nativeTraceGet,
  type NativeCallOutcome,
  type NativeTraceContext,
} from "../discovery";
import { CliError, ExitCode } from "../errors";
import { findById, findLiveByProject } from "../registry";
import { resolveNativeTarget } from "../target";
import { readTraceRecords, summarize, type TraceRecord } from "../trace";
import { writeJson } from "./json-output";
import {
  confirmed,
  flagBool,
  flagNumber,
  flagString,
  readArgsPayload,
} from "./scene";

const TARGET_CONTEXT = { findById, findLiveByProject };

interface NativeBinding {
  endpoint: IpcEndpoint;
  instanceId: string;
}

interface Step {
  name: string;
  tool: string;
  ok: boolean;
  status: string;
  result: unknown;
  request_id: string;
  task_id: string;
  new_errors_since_last_call: number;
}

function binding(opts: GlobalOptions): NativeBinding {
  const target = resolveNativeTarget(opts, TARGET_CONTEXT);
  if (target.ipcEndpoint === undefined) {
    throw new CliError(
      ExitCode.Transport,
      "no native IPC endpoint is available; pass --socket or --id from 'gda editor list'.",
    );
  }
  return {
    endpoint: target.ipcEndpoint,
    instanceId: target.instanceId ?? target.pluginRecord?.instanceId ?? target.instance?.id ?? "",
  };
}

function stepRecord(name: string, tool: string, outcome: NativeCallOutcome): Step {
  const payload = outcome.payload;
  const record = payload !== null && typeof payload === "object" && !Array.isArray(payload)
    ? payload as Record<string, unknown>
    : {};
  return {
    name,
    tool,
    ok: outcome.ok,
    status: outcome.status,
    result: payload,
    request_id: outcome.request_id,
    task_id: outcome.task_id,
    new_errors_since_last_call: typeof record.new_errors_since_last_call === "number"
      ? record.new_errors_since_last_call
      : 0,
  };
}

async function invokeStep(
  target: NativeBinding,
  name: string,
  tool: string,
  args: Record<string, unknown>,
  opts: GlobalOptions,
): Promise<Step> {
  const outcome = await nativeToolInvoke(target.endpoint, target.instanceId, tool, args, {
    timeoutMs: timeoutMs(opts),
  });
  return stepRecord(name, tool, outcome);
}

function timeoutMs(opts: GlobalOptions): number | undefined {
  const seconds = flagString(opts, "--timeout") ?? opts.timeoutText;
  if (seconds === undefined) return undefined;
  const value = Number(seconds);
  return Number.isFinite(value) && value > 0 ? value * 1000 : undefined;
}

function limitOf(opts: GlobalOptions, fallback: number): number {
  const value = flagNumber(opts, "--limit");
  if (value === undefined) return fallback;
  if (!Number.isInteger(value) || value < 0) {
    throw new CliError(ExitCode.Usage, "--limit must be a non-negative integer");
  }
  return value;
}

function progress(opts: GlobalOptions, step: Step): void {
  if (opts.progress !== true && flagBool(opts, "--detach") !== true) return;
  process.stderr.write(`${JSON.stringify({
    kind: "progress",
    command: "debug.repro",
    step: step.name,
    tool: step.tool,
    status: step.status,
  })}\n`);
}

function emit(
  command: string,
  data: Record<string, unknown>,
  opts: GlobalOptions,
  ok = true,
): number {
  writeJson({
    schema_version: 1,
    ok,
    command,
    data,
    error: ok ? undefined : data.error ?? null,
    meta: {
      instance_id: typeof data.instance_id === "string" ? data.instance_id : "",
      trace_id: typeof data.trace_id === "string" ? data.trace_id : "",
    },
    warnings: [],
  }, opts);
  return ok ? ExitCode.Success : ExitCode.ToolFailure;
}

function entriesOf(payload: unknown): unknown[] {
  if (payload === null || typeof payload !== "object" || Array.isArray(payload)) return [];
  const record = payload as Record<string, unknown>;
  for (const key of ["entries", "errors", "lines", "output", "items"]) {
    if (Array.isArray(record[key])) return record[key] as unknown[];
  }
  return [];
}

function textField(record: Record<string, unknown>, key: string): string {
  const value = record[key];
  return typeof value === "string" ? value : "";
}

export function pngPath(pathname: string): string {
  const trimmed = pathname.trim();
  if (trimmed === "") return "";
  return /\.png$/i.test(trimmed) ? trimmed : `${trimmed}.png`;
}

function localSummary(opts: GlobalOptions): Record<string, unknown> {
  const records: TraceRecord[] = readTraceRecords();
  const summary = summarize(records) as Record<string, unknown>;
  if (flagBool(opts, "--local-trace") === true) summary.source = "cli-trace";
  return summary;
}

async function runStatus(target: NativeBinding, opts: GlobalOptions): Promise<number> {
  const steps: Step[] = [];
  steps.push(await invokeStep(target, "editor", "system_status", {}, opts));
  steps.push(await invokeStep(target, "game", "get_game_state", { action: "status" }, opts));
  let trace: NativeTraceContext | undefined;
  try {
    trace = await nativeTraceGet(target.endpoint, target.instanceId, "", { timeoutMs: timeoutMs(opts) });
  } catch {
    trace = undefined;
  }
  const data: Record<string, unknown> = {
    instance_id: target.instanceId,
    trace_id: trace?.trace_id ?? "",
    steps,
    trace: trace ?? null,
    local_trace: localSummary(opts),
  };
  return emit("debug.status", data, opts, steps.every((step) => step.ok));
}

async function runErrors(target: NativeBinding, opts: GlobalOptions): Promise<number> {
  const limit = limitOf(opts, 50);
  const steps: Step[] = [];
  steps.push(await invokeStep(target, "debugger_errors", "get_debugger_stream", { action: "errors", limit }, opts));
  steps.push(await invokeStep(target, "game_log", "get_game_log_entries", { limit }, opts));
  const entries = entriesOf(steps[0].result);
  return emit("debug.errors", {
    instance_id: target.instanceId,
    steps,
    entries,
    error_count: entries.length,
    watermark: steps.reduce((sum, step) => sum + step.new_errors_since_last_call, 0),
  }, opts, steps.every((step) => step.ok));
}

async function runOutput(target: NativeBinding, opts: GlobalOptions): Promise<number> {
  const step = await invokeStep(target, "debugger_output", "get_debugger_stream", {
    action: "output",
    limit: limitOf(opts, 50),
  }, opts);
  return emit("debug.output", {
    instance_id: target.instanceId,
    steps: [step],
    lines: entriesOf(step.result),
  }, opts, step.ok);
}

async function runStack(target: NativeBinding, opts: GlobalOptions): Promise<number> {
  const steps: Step[] = [];
  steps.push(await invokeStep(target, "stack", "get_debug_stack", {}, opts));
  steps.push(await invokeStep(target, "scene_tree", "get_debugger_scene_tree", {}, opts));
  return emit("debug.stack", { instance_id: target.instanceId, steps }, opts, steps.every((step) => step.ok));
}

async function runInspect(target: NativeBinding, opts: GlobalOptions): Promise<number> {
  const action = flagString(opts, "--action") ?? "object";
  const path = flagString(opts, "--path");
  const steps: Step[] = [];
  steps.push(await invokeStep(target, "debug_info", "get_debug_info", {
    action,
    ...(path === undefined ? {} : { path }),
  }, opts));
  if (path !== undefined) {
    steps.push(await invokeStep(target, "object_info", "get_debug_object_info", { path }, opts));
  }
  return emit("debug.inspect", { instance_id: target.instanceId, action, path: path ?? "", steps }, opts,
    steps.every((step) => step.ok));
}

async function runTrace(target: NativeBinding, opts: GlobalOptions): Promise<number> {
  const traceparent = flagString(opts, "--traceparent") ?? "";
  const context = await nativeTraceGet(target.endpoint, target.instanceId, traceparent, { timeoutMs: timeoutMs(opts) });
  return emit("debug.trace", {
    instance_id: target.instanceId,
    trace_id: context.trace_id,
    traceparent: context.traceparent,
    span_id: context.span_id,
    parent_span: context.parent_span,
    local_trace: localSummary(opts),
  }, opts);
}

async function runRepro(target: NativeBinding, opts: GlobalOptions): Promise<number> {
  if (!confirmed(opts)) {
    throw new CliError(
      ExitCode.Usage,
      "gda debug repro is high-risk and requires --confirm; no tool call was sent.",
      "re-run with --confirm after reviewing the exact target.",
    );
  }
  const args = readArgsPayload(opts);
  const path = flagString(opts, "--path");
  const captureTarget = flagString(opts, "--target") ?? "game";
  const steps: Step[] = [];
  const record = async (name: string, tool: string, callArgs: Record<string, unknown>): Promise<void> => {
    const step = await invokeStep(target, name, tool, callArgs, opts);
    steps.push(step);
    progress(opts, step);
  };
  await record("play", "editor_scene_control", { action: "play", ...(path === undefined ? {} : { path }) });
  await record("input", "game_input", { ...args, action: "sequence" });
  await record("sample", "get_game_evidence", { ...args, action: "sample" });
  const captureArgs: Record<string, unknown> = { target: captureTarget, save: true };
  const afterFrames = flagNumber(opts, "--after-frames");
  const when = flagString(opts, "--when");
  if (afterFrames !== undefined) captureArgs.after_frames = afterFrames;
  if (when !== undefined) captureArgs.when = when;
  await record("capture", "capture_editor_viewport", captureArgs);
  await record("errors", "get_debugger_stream", { action: "errors", limit: 50 });
  const image = steps.find((step) => step.name === "capture");
  const payload = image?.result;
  const imagePath = payload !== null && typeof payload === "object" && !Array.isArray(payload)
    ? textField(payload as Record<string, unknown>, "path") || textField(payload as Record<string, unknown>, "image_path")
    : "";
  const pending = steps.find((step) => step.task_id !== "");
  return emit("debug.repro", {
    instance_id: target.instanceId,
    steps,
    image_path: pngPath(imagePath),
    task_id: pending?.task_id ?? "",
    detached: flagBool(opts, "--detach"),
    new_errors_since_last_call: steps.reduce((sum, step) => sum + step.new_errors_since_last_call, 0),
  }, opts, steps.every((step) => step.ok));
}

async function runReport(target: NativeBinding, opts: GlobalOptions): Promise<number> {
  const steps: Step[] = [];
  steps.push(await invokeStep(target, "errors", "get_debugger_stream", { action: "errors", limit: 100 }, opts));
  steps.push(await invokeStep(target, "game_log", "get_game_log_entries", { limit: 100 }, opts));
  steps.push(await invokeStep(target, "monitor", "game_monitor", { action: "status" }, opts));
  let trace: NativeTraceContext | undefined;
  try {
    trace = await nativeTraceGet(target.endpoint, target.instanceId, "", { timeoutMs: timeoutMs(opts) });
  } catch {
    trace = undefined;
  }
  const data: Record<string, unknown> = {
    instance_id: target.instanceId,
    label: flagString(opts, "--label") ?? "",
    trace_id: trace?.trace_id ?? "",
    steps,
    errors: entriesOf(steps[0].result),
    log_entries: entriesOf(steps[1].result),
    local_trace: localSummary(opts),
    image_path: pngPath(flagString(opts, "--image") ?? ""),
  };
  const file = flagString(opts, "--file") ?? opts.file;
  if (file !== undefined && file !== "") {
    fs.writeFileSync(file, `${JSON.stringify(data, null, 2)}\n`, "utf8");
  }
  return emit("debug.report", data, opts, steps.every((step) => step.ok));
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0] ?? "status";
  const known = new Set(["status", "errors", "output", "stack", "inspect", "trace", "repro", "report", "attach", "eval"]);
  if (!known.has(sub)) {
    throw new CliError(ExitCode.Usage, "unknown debug subcommand: use status, errors, output, stack, inspect, trace, repro, or report");
  }
  const target = binding(opts);
  if (sub === "status") return runStatus(target, opts);
  if (sub === "errors") return runErrors(target, opts);
  if (sub === "output") return runOutput(target, opts);
  if (sub === "stack") return runStack(target, opts);
  if (sub === "inspect") return runInspect(target, opts);
  if (sub === "trace") return runTrace(target, opts);
  if (sub === "repro") return runRepro(target, opts);
  if (sub === "report") return runReport(target, opts);
  if (sub === "attach" || sub === "eval") {
    throw new CliError(ExitCode.Usage, `debug ${sub} is a legacy entry; use gda tool call with the canonical tool.`);
  }
  throw new CliError(ExitCode.Usage, "unknown debug subcommand: use status, errors, output, stack, inspect, trace, repro, or report");
}
