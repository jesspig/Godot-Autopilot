import type { GlobalOptions } from "../args";
import { toolRisk } from "../command-spec";
import { CliError, ConfirmationRequiredError, ExitCode } from "../errors";
import { callTool, ensureConnection, META_DIRECT_TOOLS } from "../mcp/client";
import { err, promptConfirmation, writeJson, type ConfirmationSummary } from "../output";
import { assertCallFamilyUsage, callTimeoutSec, extractPayload, loadToolArguments } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext, type ResolvedTarget } from "../target";
import { appendCliTrace, makeTraceparent, newSpanId, newTraceId } from "../trace";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

function confirmationSummary(
  tool: string,
  args: Record<string, unknown>,
  target: ResolvedTarget,
): ConfirmationSummary {
  const instance = target.pluginRecord?.instanceId ?? target.instance?.id ?? "";
  return {
    command: "tool.call",
    tool,
    target: instance !== "" ? `instance:${instance}` : `port:${target.port}`,
    risk: toolRisk(tool),
    args: { ...args },
  };
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

function needsConfirmation(envelope: { result?: unknown }): boolean {
  if (envelope.result === null || typeof envelope.result !== "object" || Array.isArray(envelope.result)) {
    return false;
  }
  const record = envelope.result as {
    needs_confirmation?: unknown;
    input_required?: unknown;
    inputRequests?: unknown;
    resultType?: unknown;
  };
  return record.needs_confirmation === true || record.input_required !== undefined ||
    record.inputRequests !== undefined || record.resultType === "input_required";
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  assertCallFamilyUsage("call", opts);
  if (positional.length !== 1) {
    throw new CliError(ExitCode.Usage, "call requires exactly one <tool> argument.");
  }
  const tool = positional[0];
  const args = loadToolArguments(opts);
  if (opts.confirm === true) args.confirm = true;

  const target = opts.socketText !== undefined
    ? resolveTarget({ portText: undefined, id: undefined, project: opts.project, socketText: opts.socketText }, TARGET_CONTEXT)
    : resolveTarget(opts, TARGET_CONTEXT);
  const ipcEndpoint = target.ipcEndpoint ?? (opts.socketText !== undefined
    ? { path: opts.socketText, token: process.env.GODOT_AUTOPILOT_IPC_TOKEN ?? "" }
    : undefined);
  const summary = confirmationSummary(tool, args, target);
  if (toolRisk(tool) === "high" || opts.confirm === true) {
    await promptConfirmation(summary, opts);
  }

  const timeoutSec = callTimeoutSec(opts);
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
  const callOptions = {
    port: target.port,
    timeoutSec,
    direct: META_DIRECT_TOOLS.has(tool),
    ipcEndpoint,
    transport: "auto" as const,
    noFallback: false,
    traceparent,
    progressSink,
  };

  let envelope: { error?: { code?: number; message?: string }; result?: unknown };
  if (ipcEndpoint !== undefined) {
    envelope = await callTool(tool, args, { ...callOptions, port: 0, ipcEndpoint });
    recordCliCall(traceId, tool, envelope, startedAt, "ipc");
  } else {
    await ensureConnection(target.port, timeoutSec);
    envelope = await callTool(tool, args, callOptions);
    recordCliCall(traceId, tool, envelope, startedAt, "ipc");
  }

  if (needsConfirmation(envelope)) {
    throw new ConfirmationRequiredError(
      `gda tool.call ${tool} needs confirmation; no result was applied and nothing was re-sent.`,
      { ...summary, args: { ...args } },
      "re-run with --confirm after reviewing the tool, target, risk and arguments.",
    );
  }

  const extracted = extractPayload(
    envelope as Parameters<typeof extractPayload>[0],
    { saveImages: opts.saveImages, raw: opts.raw },
  );
  writeJson(extracted.display, opts, "tool.call", !extracted.toolFailed);
  if (extracted.toolFailed) {
    err(extracted.failureMessage ?? "tool-level failure reported in the business payload.");
    return ExitCode.ToolFailure;
  }
  return ExitCode.Success;
}
