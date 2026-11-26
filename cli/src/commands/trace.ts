import type { GlobalOptions } from "../args";
import { CliError, ExitCode } from "../errors";
import { writeJson } from "./json-output";
import { numberField, readTraceRecords, stringField, summarize, type TraceRecord } from "../trace";

const DEFAULT_LIMIT = 50;
const DEFAULT_SLOW_MS = 1000;

interface Page<T> {
  events: T[];
  next_cursor: number;
  limit: number;
  count: number;
}

function count(text: string | number | undefined, fallback: number, flagName: string): number {
  if (text === undefined) return fallback;
  const value = Number(text);
  if (!Number.isInteger(value) || value < 0) {
    throw new CliError(ExitCode.Usage, `${flagName} must be a non-negative integer, got: ${text}`);
  }
  return value;
}

function optionNumber(opts: GlobalOptions, name: string): string | number | undefined {
  const value = (opts as unknown as Record<string, unknown>)[name];
  if (typeof value === "number" || typeof value === "string") return value;
  if (name === "--limit" && (typeof opts.limitText === "number" || typeof opts.limitText === "string")) {
    return opts.limitText;
  }
  return undefined;
}

export function paginate<T>(items: T[], opts: GlobalOptions, fallback = DEFAULT_LIMIT): Page<T> {
  const cursor = count(optionNumber(opts, "--cursor"), 0, "--cursor");
  const limit = count(optionNumber(opts, "--limit"), fallback, "--limit");
  const start = Math.min(cursor, items.length);
  const end = limit === 0 ? start : Math.min(items.length, start + limit);
  return {
    events: items.slice(start, end),
    next_cursor: end,
    limit,
    count: end - start,
  };
}

function writePage(command: string, events: unknown[], opts: GlobalOptions, page: Page<unknown>): void {
  writeJson({
    schema_version: 1,
    ok: true,
    command,
    data: { events, next_cursor: page.next_cursor, limit: page.limit, count: page.count },
    meta: { trace_id: "", instance_id: "" },
    warnings: [],
  }, opts);
}

function isToolCall(record: TraceRecord): boolean {
  return stringField(record, "kind") === "tool_call" || stringField(record, "tool") !== "";
}

function compact(record: TraceRecord): Record<string, unknown> {
  return {
    seq: numberField(record, "seq"),
    trace_id: stringField(record, "trace_id"),
    span_id: stringField(record, "span_id"),
    event_id: stringField(record, "event_id"),
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
    wall_start_ms: numberField(record, "wall_start_ms"),
    wall_end_ms: numberField(record, "wall_end_ms"),
  };
}

function slowThreshold(opts: GlobalOptions): number {
  const value = Number(optionNumber(opts, "--slow") ?? DEFAULT_SLOW_MS);
  if (!Number.isFinite(value) || value < 0) {
    throw new CliError(ExitCode.Usage, "--slow must be a non-negative number");
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

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0] ?? "list";
  const records = readTraceRecords();
  if (sub === "list") {
    const page = paginate(records.filter(isToolCall).map(compact), opts);
    writePage("trace.list", page.events, opts, page);
    return ExitCode.Success;
  }
  if (sub === "show") {
    const traceId = positional[1];
    if (traceId === undefined || traceId === "") {
      throw new CliError(ExitCode.Usage, "trace show requires a <trace_id>");
    }
    const page = paginate(records
      .filter((record) => stringField(record, "trace_id") === traceId)
      .map(compact), opts);
    writePage("trace.show", page.events, opts, page);
    return ExitCode.Success;
  }
  if (sub === "summary") {
    writeJson({
      schema_version: 1,
      ok: true,
      command: "trace.summary",
      data: { summary: summarize(records) },
      meta: { trace_id: "", instance_id: "" },
      warnings: [],
    }, opts);
    return ExitCode.Success;
  }
  if (sub === "slow") {
    const threshold = slowThreshold(opts);
    const slow = records
      .filter(isToolCall)
      .filter((record) => numberField(record, "duration_ms") >= threshold)
      .map(compact);
    const page = paginate(slow, opts);
    writePage("trace.slow", page.events, opts, page);
    return ExitCode.Success;
  }
  if (sub === "failures") {
    const failures = records
      .filter(isToolCall)
      .filter((record) => record.ok === false)
      .map(compact);
    const page = paginate(failures, opts);
    writePage("trace.failures", page.events, opts, page);
    return ExitCode.Success;
  }
  if (sub === "export") {
    const format = opts.exportFormat ?? "jsonl";
    const page = paginate(records, opts, Number.MAX_SAFE_INTEGER);
    if (format === "jsonl") {
      for (const record of page.events) {
        process.stdout.write(`${JSON.stringify(record)}\n`);
      }
      return ExitCode.Success;
    }
    if (format === "otlp") {
      writeJson(toOtlp(page.events as TraceRecord[]), opts);
      return ExitCode.Success;
    }
    throw new CliError(ExitCode.Usage, `trace export --format must be jsonl or otlp, got: ${format}`);
  }
  if (sub === "clear") {
    throw new CliError(ExitCode.Usage, "trace clear is not supported; remove the trace directories manually");
  }
  throw new CliError(ExitCode.Usage, "unknown trace subcommand: use list, show, summary, slow, failures, or export");
}
