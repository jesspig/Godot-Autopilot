import type { GlobalOptions } from "../args";
import { parseTransport } from "../args";
import { CliError, ExitCode } from "../errors";
import { callTool, META_DIRECT_TOOLS } from "../mcp/client";
import { err } from "../output";
import { assertCallFamilyUsage, callTimeoutSec, emit, extractPayload, loadToolArguments } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext, type ResolvedTarget } from "../target";
import { argsHash, recordTask } from "../tasks-index";
import { appendCliTrace, makeTraceparent, newSpanId, newTraceId } from "../trace";
import { writeJson } from "./json-output";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

function finishAsync(
  envelope: { result?: unknown },
  tool: string,
  args: Record<string, unknown>,
  target: ResolvedTarget,
  opts: GlobalOptions,
): number | null {
  if (opts.async !== true) return null;
  const result = envelope.result;
  if (result === null || typeof result !== "object" || Array.isArray(result)) return null;
  const record = result as { taskId?: unknown; task_id?: unknown; status?: unknown; resultType?: unknown };
  const taskId = typeof record.taskId === "string"
    ? record.taskId
    : typeof record.task_id === "string" ? record.task_id : "";
  const isTask = taskId !== "" || record.resultType === "task";
  if (!isTask) {
    err("server executed synchronously: this client or tool does not support MCP tasks");
    return null;
  }
  if (taskId === "") {
    throw new CliError(ExitCode.ToolFailure, "server returned a task result without a task id");
  }
  recordTask({
    task_id: taskId,
    tool,
    args_hash: argsHash(args),
    started_at: new Date().toISOString(),
    port: target.port,
    instance_id: target.instance?.id ?? target.pluginRecord?.instanceId ?? "",
    status: typeof record.status === "string" ? record.status : "working",
  });
  writeJson(
    {
      task_id: taskId,
      status: typeof record.status === "string" ? record.status : "working",
      tool,
    },
    opts,
  );
  return ExitCode.Success;
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("call", opts);
  if (positional.length !== 1) {
    throw new CliError(ExitCode.Usage, "call requires exactly one <tool> argument.");
  }
  const tool = positional[0];
  const args = loadToolArguments(opts);
  if (opts.confirm === true) {
    args.confirm = true;
  }
  const timeoutSec = callTimeoutSec(opts);
  const transport = parseTransport(opts.transportText);
  const progressSink = opts.progress === true
    ? (progress: { progress: number; total?: number; message?: string }) => {
        const total = progress.total === undefined ? "?" : String(progress.total);
        const message = progress.message === undefined ? "" : ` ${progress.message}`;
        err(`progress ${progress.progress}/${total}${message}`);
      }
    : undefined;
  const traceId = newTraceId();
  const traceparent = makeTraceparent(traceId);
  const startedAt = Date.now();
  if (opts.socketText !== undefined) {
    try {
      const envelope = await callTool(tool, args, {
        port: 0,
        timeoutSec,
        direct: META_DIRECT_TOOLS.has(tool),
        ipcEndpoint: {
          path: opts.socketText,
          token: process.env.GODOT_AUTOPILOT_IPC_TOKEN ?? "",
        },
        transport: "ipc",
        noFallback: opts.noFallback,
        traceparent,
        progressSink,
      });
      recordCliCall(traceId, tool, envelope, startedAt, "ipc");
      const extracted = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
      return emit(extracted, opts);
    } catch (error) {
      if (!(error instanceof CliError) || error.code !== ExitCode.Transport || opts.noFallback === true) {
        throw error;
      }
      err(`IPC channel unavailable (${error.message}); falling back to the port channel.`);
    }
  }
  const target = resolveTarget(opts, TARGET_CONTEXT);
  let usedTransport = transport;
  const envelope = await callTool(tool, args, {
    port: target.port,
    timeoutSec,
    direct: META_DIRECT_TOOLS.has(tool),
    ipcEndpoint: target.ipcEndpoint,
    transport,
    noFallback: opts.noFallback,
    traceparent,
    progressSink,
    transportSink: (actual) => {
      usedTransport = actual;
    },
  });
  recordCliCall(traceId, tool, envelope, startedAt, usedTransport);
  const asyncCode = finishAsync(envelope, tool, args, target, opts);
  if (asyncCode !== null) return asyncCode;
  const extracted = extractPayload(envelope, { saveImages: opts.saveImages, raw: opts.raw });
  return emit(extracted, opts);
}

function recordCliCall(
  traceId: string,
  tool: string,
  envelope: { error?: { code?: number; message?: string }; result?: unknown },
  startedAt: number,
  transport: string,
): void {
  const result = envelope.result as { error?: unknown } | undefined;
  const errorCode =
    envelope.error !== undefined
      ? String(envelope.error.code ?? "rpc_error")
      : result?.error !== undefined
        ? "tool_error"
        : "";
  const finishedAt = Date.now();
  appendCliTrace({
    trace_id: traceId,
    span_id: newSpanId(),
    event_id: newSpanId(),
    kind: "tool_call",
    tool,
    ok: errorCode === "",
    error_code: errorCode,
    duration_ms: finishedAt - startedAt,
    wall_start_ms: startedAt,
    wall_end_ms: finishedAt,
    transport,
    attempt: 1,
  });
}
