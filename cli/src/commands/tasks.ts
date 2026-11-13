import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { request } from "../mcp/client";
import { err } from "../output";
import { callTimeoutSec } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";
import {
  isTerminalStatus,
  listTasks,
  removeTask,
} from "../tasks-index";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

interface TaskResult {
  taskId?: unknown;
  task_id?: unknown;
  status?: unknown;
  statusMessage?: unknown;
  result?: unknown;
  error?: unknown;
}

function taskIdOf(result: unknown): string {
  if (result === null || typeof result !== "object") return "";
  const record = result as TaskResult;
  if (typeof record.taskId === "string") return record.taskId;
  if (typeof record.task_id === "string") return record.task_id;
  return "";
}

function statusOf(result: unknown): string {
  if (result === null || typeof result !== "object") return "";
  const status = (result as TaskResult).status;
  return typeof status === "string" ? status : "";
}

async function requestTask(
  method: string,
  params: Record<string, unknown>,
  opts: GlobalOptions,
): Promise<unknown> {
  const target = resolveTarget(opts, TARGET_CONTEXT);
  const response = await request(method, params, {
    port: target.port,
    timeoutSec: callTimeoutSec(opts),
    ipcEndpoint: target.ipcEndpoint,
  });
  if (response.error !== undefined) {
    throw new CliError(
      ExitCode.ToolFailure,
      `${method} failed: ${response.error.message ?? "server error"} (code ${response.error.code})`,
    );
  }
  return response.result;
}

function waitTimeoutMs(opts: GlobalOptions): number {
  if (opts.timeoutText !== undefined) {
    const seconds = Number(opts.timeoutText);
    if (!Number.isFinite(seconds) || seconds <= 0) {
      throw new CliError(ExitCode.Usage, `--timeout must be a positive number of seconds, got: ${opts.timeoutText}`);
    }
    return Math.round(seconds * 1000);
  }
  return 300_000;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0];
  if (sub === "list") {
    const entries = listTasks();
    writeJson({ tasks: entries.map(({ file: _file, ...entry }) => entry) }, opts);
    return ExitCode.Success;
  }
  if (sub === "get") {
    const id = positional[1];
    if (id === undefined) throw new CliError(ExitCode.Usage, "tasks get requires a <task-id> argument.");
    const result = await requestTask("tasks/get", { taskId: id }, opts);
    writeJson(result, opts);
    return ExitCode.Success;
  }
  if (sub === "wait") {
    const id = positional[1];
    if (id === undefined) throw new CliError(ExitCode.Usage, "tasks wait requires a <task-id> argument.");
    const deadline = Date.now() + waitTimeoutMs(opts);
    let lastStatus = "";
    for (;;) {
      const result = await requestTask("tasks/get", { taskId: id }, opts);
      const status = statusOf(result);
      if (status !== lastStatus) {
        lastStatus = status;
        err(`[task ${id}] ${status}`);
      }
      if (isTerminalStatus(status)) {
        writeJson(result, opts);
        return status === "completed" ? ExitCode.Success : ExitCode.ToolFailure;
      }
      if (Date.now() >= deadline) {
        throw new CliError(ExitCode.Transport, `task ${id} did not finish before the timeout (last status: ${status || "unknown"}).`);
      }
      await new Promise((resolve) => setTimeout(resolve, 500));
    }
  }
  if (sub === "cancel") {
    const id = positional[1];
    if (id === undefined) throw new CliError(ExitCode.Usage, "tasks cancel requires a <task-id> argument.");
    const result = await requestTask("tasks/cancel", { taskId: id, reason: "cancelled by gda tasks cancel" }, opts);
    writeJson(result, opts);
    return ExitCode.Success;
  }
  if (sub === "clean") {
    const entries = listTasks();
    const removed: string[] = [];
    for (const entry of entries) {
      if (isTerminalStatus(entry.status) && removeTask(entry.task_id)) {
        removed.push(entry.task_id);
      }
    }
    writeJson({ removed }, opts);
    return ExitCode.Success;
  }
  throw new CliError(ExitCode.Usage, `unknown tasks subcommand: ${sub ?? "(none)"} (expected list, get, wait, cancel or clean)`);
}
