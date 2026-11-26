import fs from "node:fs";
import path from "node:path";

import type { GlobalOptions } from "../args";
import {
  nativeEventsResume,
  nativeEventsSubscribe,
  nativeTaskCancel,
  nativeTaskGet,
  nativeTaskList,
  type NativeEventPage,
  type NativeEventRecord,
  type NativeTaskRecord,
  NATIVE_ERROR_CURSOR_EXPIRED,
  NATIVE_ERROR_TASK_NOT_FOUND,
} from "../discovery";
import { CliError, ExitCode, isIpcRequestSent } from "../errors";
import type { IpcEndpoint } from "../ipc/client";
import { gdaRoot } from "../platform";
import { findById, findLiveByProject } from "../registry";
import { resolveNativeTarget } from "../target";
import { writeJson } from "./json-output";
import { confirmed, flagBool, flagNumber, flagString } from "./scene";

const TARGET_CONTEXT = { findById, findLiveByProject };
const TERMINAL_STATES = new Set(["succeeded", "failed", "cancelled", "interrupted", "expired", "not_found"]);
const CANCEL_STAGES = new Set(["requested", "acknowledged", "draining", "completed"]);

interface JobRecord {
  task_id: string;
  instance_id: string;
  generation: number;
  state: string;
  cancel_stage: string;
  progress: number;
  progress_message: string;
  result: unknown;
  error_symbol: string;
  error_message: string;
  command: string;
  started_at: string;
  updated_at: string;
  terminal: boolean;
  cursor?: number;
}

interface Binding {
  endpoint: IpcEndpoint;
  instanceId: string;
  generation: number;
}

function storePath(): string {
  const override = process.env.GODOT_AUTOPILOT_CLI_HOME;
  const root = override !== undefined && override.trim() !== "" ? path.resolve(override.trim()) : gdaRoot();
  return path.join(root, "cli", "jobs.json");
}

export function readJobStore(): Record<string, JobRecord> {
  try {
    const parsed = JSON.parse(fs.readFileSync(storePath(), "utf8")) as unknown;
    if (parsed === null || typeof parsed !== "object" || Array.isArray(parsed)) return {};
    return parsed as Record<string, JobRecord>;
  } catch {
    return {};
  }
}

function writeJobStore(records: Record<string, JobRecord>): void {
  const file = storePath();
  fs.mkdirSync(path.dirname(file), { recursive: true });
  const tmp = `${file}.${process.pid}.tmp`;
  fs.writeFileSync(tmp, `${JSON.stringify(records, null, 2)}\n`, "utf8");
  fs.renameSync(tmp, file);
}

function keyOf(instanceId: string, generation: number, taskId: string): string {
  return `${instanceId}/${generation}/${taskId}`;
}

function persist(command: string, task: NativeTaskRecord, binding: Binding): JobRecord {
  const records = readJobStore();
  const key = keyOf(binding.instanceId, binding.generation, task.task_id);
  const previous = records[key];
  const record: JobRecord = {
    task_id: task.task_id,
    instance_id: task.instance_id === "" ? binding.instanceId : task.instance_id,
    generation: task.generation === 0 ? binding.generation : task.generation,
    state: task.state,
    cancel_stage: task.cancel_stage,
    progress: task.progress,
    progress_message: task.progress_message,
    result: task.result,
    error_symbol: task.error_symbol,
    error_message: task.error_message,
    command,
    started_at: previous?.started_at ?? new Date().toISOString(),
    updated_at: new Date().toISOString(),
    terminal: TERMINAL_STATES.has(task.state),
  };
  records[key] = record;
  writeJobStore(records);
  return record;
}

function persistedFor(taskId: string): JobRecord | undefined {
  return Object.values(readJobStore()).find((record) => record.task_id === taskId);
}

function binding(opts: GlobalOptions): Binding {
  const target = resolveNativeTarget(opts, TARGET_CONTEXT);
  if (target.ipcEndpoint === undefined) {
    throw new CliError(
      ExitCode.Transport,
      "no native IPC endpoint is available; pass --socket or --id from 'gda editor list'.",
    );
  }
  const generation = flagNumber(opts, "--generation") ?? 0;
  if (!Number.isInteger(generation) || generation < 0) {
    throw new CliError(ExitCode.Usage, "--generation must be a non-negative integer");
  }
  return {
    endpoint: target.ipcEndpoint,
    instanceId: target.instanceId ?? target.pluginRecord?.instanceId ?? target.instance?.id ?? "",
    generation,
  };
}

function gcpCode(error: unknown): number {
  if (error === null || typeof error !== "object") return 0;
  const value = (error as { gcp_code?: unknown }).gcp_code;
  return typeof value === "number" && Number.isFinite(value) ? value : 0;
}

function taskData(task: NativeTaskRecord | JobRecord): Record<string, unknown> {
  return {
    task_id: task.task_id,
    instance_id: task.instance_id,
    generation: task.generation,
    state: task.state,
    cancel_stage: "cancel_stage" in task ? task.cancel_stage : "",
    progress: task.progress,
    progress_message: task.progress_message,
    result: task.result,
    error_symbol: task.error_symbol,
    error_message: task.error_message,
  };
}

function emit(
  command: string,
  data: Record<string, unknown>,
  opts: GlobalOptions,
  ok: boolean,
): number {
  writeJson({
    schema_version: 1,
    ok,
    command,
    data,
    error: ok ? undefined : data.error ?? null,
    meta: {
      instance_id: typeof data.instance_id === "string" ? data.instance_id : "",
      task_id: typeof data.task_id === "string" ? data.task_id : "",
      generation: typeof data.generation === "number" ? data.generation : 0,
    },
    warnings: [],
  }, opts);
  return ok ? ExitCode.Success : ExitCode.ToolFailure;
}

function errorData(code: string, message: string, extra: Record<string, unknown> = {}): Record<string, unknown> {
  return { error: { code, message, retryable: false }, ...extra };
}

function progress(opts: GlobalOptions, command: string, state: string, taskId: string): void {
  if (state !== "reconnect" && opts.progress !== true && !flagBool(opts, "--detach")) return;
  process.stderr.write(`${JSON.stringify({
    kind: "progress",
    command,
    state,
    task_id: taskId,
  })}\n`);
}

function interrupted(opts: GlobalOptions, command: string, taskId: string, previous: JobRecord): number {
  const record: JobRecord = { ...previous, state: "interrupted", updated_at: new Date().toISOString(), terminal: true };
  const records = readJobStore();
  records[keyOf(record.instance_id, record.generation, record.task_id)] = record;
  writeJobStore(records);
  return emit(command, errorData("interrupted",
    `job ${taskId} belonged to instance ${record.instance_id} generation ${record.generation}; the current target is different.`,
    { ...taskData(record), interrupted: true }), opts, false);
}

function fromNative(task: NativeTaskRecord, command: string, target: Binding): { record?: JobRecord; error?: number } {
  if (task.task_id !== "" && task.instance_id !== "" && task.instance_id !== target.instanceId) return { error: 8 };
  if (task.generation !== 0 && task.generation !== target.generation) return { error: 8 };
  return { record: persist(command, task, target) };
}

async function runList(opts: GlobalOptions, target: Binding): Promise<number> {
  const generation = target.generation;
  let tasks: NativeTaskRecord[] = [];
  try {
    tasks = await nativeTaskList(target.endpoint, target.instanceId, generation, {});
  } catch {
    tasks = [];
  }
  for (const task of tasks) persist("job.list", task, target);
  const records = readJobStore();
  const merged = new Map<string, JobRecord>();
  for (const record of Object.values(records)) {
    if (record.generation === generation && record.instance_id === target.instanceId) {
      merged.set(record.task_id, record);
    }
  }
  return emit("job.list", {
    instance_id: target.instanceId,
    generation,
    jobs: [...merged.values()].map((record) => taskData(record)),
    count: merged.size,
  }, opts, true);
}

async function runStatus(positional: string[], opts: GlobalOptions, target: Binding): Promise<number> {
  const taskId = positional[1] ?? flagString(opts, "--task-id") ?? "";
  if (taskId === "") throw new CliError(ExitCode.Usage, "job status requires a <task-id>");
  const previous = persistedFor(taskId);
  if (previous !== undefined && (previous.instance_id !== target.instanceId || previous.generation !== target.generation)) {
    return interrupted(opts, "job.status", taskId, previous);
  }
  let task: NativeTaskRecord;
  try {
    task = await nativeTaskGet(target.endpoint, target.instanceId, taskId, target.generation, {});
  } catch (error) {
    if (gcpCode(error) === NATIVE_ERROR_TASK_NOT_FOUND) {
      return emit("job.status", errorData("not_found", `job ${taskId} was not found or expired.`, {
        task_id: taskId, instance_id: target.instanceId, generation: target.generation, state: "not_found",
      }), opts, false);
    }
    throw error;
  }
  const converted = fromNative(task, "job.status", target);
  if (converted.error === 8) {
    return interrupted(opts, "job.status", taskId, previous ?? {
      task_id: taskId, instance_id: target.instanceId, generation: target.generation, state: "interrupted",
      cancel_stage: "", progress: 0, progress_message: "", result: null, error_symbol: "", error_message: "",
      command: "job.status", started_at: new Date().toISOString(), updated_at: new Date().toISOString(), terminal: true,
    });
  }
  return emit("job.status", {
    instance_id: target.instanceId,
    generation: target.generation,
    ...taskData(task),
    terminal: converted.record?.terminal ?? false,
  }, opts, task.state !== "failed" && task.state !== "cancelled");
}

function waitTimeoutMs(opts: GlobalOptions): number {
  const raw = flagString(opts, "--timeout") ?? opts.timeoutText;
  if (raw === undefined) return 300_000;
  const seconds = Number(raw);
  if (!Number.isFinite(seconds) || seconds <= 0) {
    throw new CliError(ExitCode.Usage, "--timeout must be a positive number of seconds");
  }
  return Math.round(seconds * 1000);
}

async function runWait(positional: string[], opts: GlobalOptions, target: Binding): Promise<number> {
  const taskId = positional[1] ?? flagString(opts, "--task-id") ?? "";
  if (taskId === "") throw new CliError(ExitCode.Usage, "job wait requires a <task-id>");
  const previous = persistedFor(taskId);
  if (previous !== undefined && (previous.instance_id !== target.instanceId || previous.generation !== target.generation)) {
    return interrupted(opts, "job.wait", taskId, previous);
  }
  const deadline = Date.now() + waitTimeoutMs(opts);
  const retries = flagNumber(opts, "--retries") ?? 2;
  if (!Number.isInteger(retries) || retries < 0) {
    throw new CliError(ExitCode.Usage, "--retries must be a non-negative integer");
  }
  let reconnects = 0;
  let lastState = "";
  for (;;) {
    let task: NativeTaskRecord;
    try {
      task = await nativeTaskGet(target.endpoint, target.instanceId, taskId, target.generation, {});
    } catch (error) {
      if (gcpCode(error) === NATIVE_ERROR_TASK_NOT_FOUND) {
        return emit("job.wait", errorData("not_found", `job ${taskId} was not found or expired.`, {
          task_id: taskId, instance_id: target.instanceId, generation: target.generation, state: "not_found",
        }), opts, false);
      }
      if (isIpcRequestSent(error) && reconnects < retries) {
        reconnects += 1;
        progress(opts, "job.wait", "reconnect", taskId);
        continue;
      }
      throw error;
    }
    const converted = fromNative(task, "job.wait", target);
    if (converted.error === 8) {
      return interrupted(opts, "job.wait", taskId, previous ?? {
        task_id: taskId, instance_id: target.instanceId, generation: target.generation, state: "interrupted",
        cancel_stage: "", progress: 0, progress_message: "", result: null, error_symbol: "", error_message: "",
        command: "job.wait", started_at: new Date().toISOString(), updated_at: new Date().toISOString(), terminal: true,
      });
    }
    if (task.state !== lastState) {
      lastState = task.state;
      progress(opts, "job.wait", task.state, taskId);
    }
    if (TERMINAL_STATES.has(task.state) || flagBool(opts, "--detach")) {
      return emit("job.wait", {
        instance_id: target.instanceId,
        generation: target.generation,
        ...taskData(task),
        terminal: TERMINAL_STATES.has(task.state),
        detached: flagBool(opts, "--detach"),
        reconnects,
      }, opts, task.state === "succeeded");
    }
    if (Date.now() >= deadline) {
      return emit("job.wait", errorData("timeout", `job ${taskId} did not finish before the timeout (last state: ${task.state}).`, {
        instance_id: target.instanceId, generation: target.generation, ...taskData(task), reconnects,
      }), opts, false);
    }
    await new Promise((resolve) => setTimeout(resolve, 250));
  }
}

async function runCancel(positional: string[], opts: GlobalOptions, target: Binding): Promise<number> {
  if (!confirmed(opts)) {
    throw new CliError(
      ExitCode.Usage,
      "gda job cancel is high-risk and requires --confirm; no tool call was sent.",
      "re-run with --confirm after reviewing the exact task and instance.",
    );
  }
  const taskId = positional[1] ?? flagString(opts, "--task-id") ?? "";
  if (taskId === "") throw new CliError(ExitCode.Usage, "job cancel requires a <task-id>");
  const previous = persistedFor(taskId);
  if (previous !== undefined && (previous.instance_id !== target.instanceId || previous.generation !== target.generation)) {
    return interrupted(opts, "job.cancel", taskId, previous);
  }
  const reason = flagString(opts, "--reason") ?? "cancelled by gda job cancel";
  let task: NativeTaskRecord;
  try {
    task = await nativeTaskCancel(target.endpoint, target.instanceId, taskId, target.generation, reason, {});
  } catch (error) {
    if (gcpCode(error) === NATIVE_ERROR_TASK_NOT_FOUND) {
      return emit("job.cancel", errorData("not_found", `job ${taskId} was not found or expired.`, {
        task_id: taskId, instance_id: target.instanceId, generation: target.generation,
      }), opts, false);
    }
    throw error;
  }
  const converted = fromNative(task, "job.cancel", target);
  if (converted.error === 8) {
    return interrupted(opts, "job.cancel", taskId, previous ?? {
      task_id: taskId, instance_id: target.instanceId, generation: target.generation, state: "interrupted",
      cancel_stage: "", progress: 0, progress_message: "", result: null, error_symbol: "", error_message: "",
      command: "job.cancel", started_at: new Date().toISOString(), updated_at: new Date().toISOString(), terminal: true,
    });
  }
  const stage = CANCEL_STAGES.has(task.cancel_stage)
    ? task.cancel_stage
    : task.state === "cancelled" ? "completed" : "requested";
  return emit("job.cancel", {
    instance_id: target.instanceId,
    generation: target.generation,
    ...taskData(task),
    cancel_stage: stage,
    cancelled: task.state === "cancelled",
    terminal: converted.record?.terminal ?? false,
  }, opts, task.state !== "failed");
}

function eventData(event: NativeEventRecord): Record<string, unknown> {
  return {
    seq: event.seq,
    instance_id: event.instance_id,
    task_id: event.task_id,
    event_type: event.event_type,
    traceparent: event.traceparent,
    request_id: event.request_id,
    payload: event.payload,
  };
}

async function runLogs(positional: string[], opts: GlobalOptions, target: Binding): Promise<number> {
  const taskId = flagString(opts, "--task-id") ?? positional[1] ?? "";
  const cursor = flagNumber(opts, "--cursor") ?? 0;
  const limit = flagNumber(opts, "--limit") ?? 50;
  if (!Number.isInteger(cursor) || cursor < 0) throw new CliError(ExitCode.Usage, "--cursor must be a non-negative integer");
  if (!Number.isInteger(limit) || limit < 0) throw new CliError(ExitCode.Usage, "--limit must be a non-negative integer");
  const eventType = flagString(opts, "--event-type") ?? "all";
  let subscription: string;
  try {
    subscription = await nativeEventsSubscribe(target.endpoint, target.instanceId, cursor, eventType, {});
  } catch (error) {
    if (gcpCode(error) === NATIVE_ERROR_CURSOR_EXPIRED) {
      return emit("job.logs", errorData("resync_required",
        `event cursor ${cursor} expired; re-subscribe from 0.`,
        { task_id: taskId, instance_id: target.instanceId, cursor }), opts, false);
    }
    throw error;
  }
  let page: NativeEventPage;
  try {
    page = await nativeEventsResume(target.endpoint, target.instanceId, subscription, limit, {});
  } catch (error) {
    if (gcpCode(error) === NATIVE_ERROR_CURSOR_EXPIRED) {
      return emit("job.logs", errorData("resync_required", "event cursor expired during resume; re-subscribe from 0.", {
        task_id: taskId, instance_id: target.instanceId, cursor,
      }), opts, false);
    }
    throw error;
  }
  const events = page.events
    .filter((event) => taskId === "" || event.task_id === taskId)
    .map(eventData);
  const previous = taskId === "" ? undefined : persistedFor(taskId);
  if (previous !== undefined) {
    const records = readJobStore();
    records[keyOf(previous.instance_id, previous.generation, previous.task_id)] = {
      ...previous, cursor: page.next_cursor, updated_at: new Date().toISOString(),
    };
    writeJobStore(records);
  }
  return emit("job.logs", {
    task_id: taskId,
    instance_id: target.instanceId,
    generation: target.generation,
    cursor,
    next_cursor: page.next_cursor,
    delivered: page.delivered,
    events,
    count: events.length,
    detached: flagBool(opts, "--detach"),
  }, opts, true);
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === undefined || sub === "get" || sub === "clean") {
    throw new CliError(ExitCode.Usage, "job requires list, status, wait, cancel, or logs (legacy get/clean use gda tasks)");
  }
  const target = binding(opts);
  if (sub === "list") return runList(opts, target);
  if (sub === "status") return runStatus(positional, opts, target);
  if (sub === "wait") return runWait(positional, opts, target);
  if (sub === "cancel") return runCancel(positional, opts, target);
  if (sub === "logs") return runLogs(positional, opts, target);
  throw new CliError(ExitCode.Usage, "unknown job subcommand: use list, status, wait, cancel, or logs");
}
