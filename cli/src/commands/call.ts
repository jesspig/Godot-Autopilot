import type { GlobalOptions } from "../args";
import { parseTransport } from "../args";
import { CliError, ExitCode } from "../errors";
import { callTool, META_DIRECT_TOOLS } from "../mcp/client";
import { err } from "../output";
import { assertCallFamilyUsage, callTimeoutSec, emit, extractPayload, loadToolArguments } from "../payload";
import { findById, findLiveByProject } from "../registry";
import { resolveTarget, type TargetContext } from "../target";
import { appendCliTrace, makeTraceparent, newSpanId, newTraceId } from "../trace";

const TARGET_CONTEXT: TargetContext = { findById, findLiveByProject };

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
    transportSink: (actual) => {
      usedTransport = actual;
    },
  });
  recordCliCall(traceId, tool, envelope, startedAt, usedTransport);
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

function recordCliCall(
