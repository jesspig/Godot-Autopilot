import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { writeJson } from "./json-output";
import { numberField, readTraceRecords, stringField, summarize, type TraceRecord } from "../trace";

const DEFAULT_LIMIT = 50;
const DEFAULT_SLOW_MS = 1000;

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0] ?? "summary";
  const records = readTraceRecords();
  if (sub === "list") {
    const limit = parseCount(opts.limitText, DEFAULT_LIMIT, "--limit");
    const calls = records.filter(isToolCall).slice(-limit);
    writeJson(calls.map(compact), opts);
    return ExitCode.Success;
  }
  if (sub === "show") {
    const traceId = positional[1];
    if (traceId === undefined || traceId === "") {
      throw new CliError(ExitCode.Usage, "trace show requires a <trace_id>");
    }
    writeJson(records.filter((record) => stringField(record, "trace_id") === traceId), opts);
    return ExitCode.Success;
  }
  if (sub === "summary") {
    writeJson(summarize(records), opts);
    return ExitCode.Success;
  }
  if (sub === "slow") {
    const threshold = parseCount(opts.slowText, DEFAULT_SLOW_MS, "--slow");
    const slow = records
      .filter(isToolCall)
      .filter((record) => numberField(record, "duration_ms") >= threshold)
      .map(compact);
    writeJson(slow, opts);
    return ExitCode.Success;
  }
  if (sub === "failures") {
    const failures = records
      .filter(isToolCall)
      .filter((record) => record.ok === false)
      .map(compact);
    writeJson(failures, opts);
    return ExitCode.Success;
  }
  if (sub === "export") {
    const format = opts.formatText ?? "jsonl";
    if (format === "jsonl") {
      for (const record of records) {
        process.stdout.write(`${JSON.stringify(record)}\n`);
      }
      return ExitCode.Success;
    }
    if (format === "otlp") {
      writeJson(toOtlp(records), opts);
      return ExitCode.Success;
    }
    throw new CliError(ExitCode.Usage, `trace export --format must be jsonl or otlp, got: ${format}`);
  }
  if (sub === "clear") {
    throw new CliError(ExitCode.Usage, "trace clear is not supported; remove the trace directories manually");
  }
  throw new CliError(ExitCode.Usage, `unknown trace subcommand: ${sub} (use list, show, summary, slow, failures, export)`);
}

function isToolCall(record: TraceRecord): boolean {
  return stringField(record, "kind") === "tool_call" || stringField(record, "tool") !== "";
}

function compact(record: TraceRecord): Record<string, unknown> {
  return {
    trace_id: stringField(record, "trace_id"),
    span_id: stringField(record, "span_id"),
    source: stringField(record, "source"),
    transport: stringField(record, "transport"),
    tool: stringField(record, "tool") || stringField(record, "name"),
    ok: record.ok === true,
    error_code: stringField(record, "error_code"),
    duration_ms: numberField(record, "duration_ms"),
    queue_wait_ms: numberField(record, "queue_wait_ms"),
    dispatch_wait_ms: numberField(record, "dispatch_wait_ms"),
    exec_ms: numberField(record, "exec_ms"),
    serialize_ms: numberField(record, "serialize_ms"),
    retry_of: stringField(record, "retry_of"),
    attempt: numberField(record, "attempt"),
    wall_end_ms: numberField(record, "wall_end_ms"),
  };
}

function parseCount(text: string | undefined, fallback: number, flag: string): number {
  if (text === undefined) return fallback;
  const value = Number(text);
  if (!Number.isInteger(value) || value < 0) {
    throw new CliError(ExitCode.Usage, `${flag} must be a non-negative integer, got: ${text}`);
  }
  return value;
}

function toOtlp(records: TraceRecord[]): Record<string, unknown> {
  return {
    resourceSpans: [
      {
        resource: { attributes: [{ key: "service.name", value: { stringValue: "godot-autopilot" } }] },
        scopeSpans: [
          {
            scope: { name: "gda.trace" },
            spans: records.map((record) => {
              const start = numberField(record, "wall_start_ms") || numberField(record, "wall_end_ms");
              const duration = numberField(record, "duration_ms");
              return {
                traceId: stringField(record, "trace_id"),
                spanId: stringField(record, "span_id"),
                parentSpanId: stringField(record, "parent_span"),
                name: stringField(record, "tool") || stringField(record, "name") || "event",
                kind: 1,
                startTimeUnixNano: String(start * 1000000),
                endTimeUnixNano: String((start + duration) * 1000000),
                attributes: [
                  { key: "gda.source", value: { stringValue: stringField(record, "source") } },
                  { key: "gda.ok", value: { boolValue: record.ok === true } },
                  { key: "gda.error_code", value: { stringValue: stringField(record, "error_code") } },
                ],
                status: { code: record.ok === true ? 1 : 2 },
              };
            }),
          },
        ],
      },
    ],
  };
}
