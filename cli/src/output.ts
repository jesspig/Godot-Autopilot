import * as readline from "node:readline/promises";

import type { GlobalOptions } from "./args";
import {
  CliError,
  ConfirmationRequiredError,
  errorCodeForExit,
  ExitCode,
  InterruptedError,
  isIpcRequestSent,
  UserDeclinedError,
} from "./errors";

export const ENVELOPE_SCHEMA_VERSION = 1;

export interface CliEnvelopeError {
  code: string;
  message: string;
  retryable: boolean;
  request_sent: boolean;
  suggestion?: string;
  [key: string]: unknown;
}

export interface CliEnvelope {
  schema_version: number;
  ok: boolean;
  command: string;
  data: unknown;
  error: CliEnvelopeError | null;
  meta: Record<string, unknown>;
  warnings: unknown[];
}

export interface ConfirmationSummary {
  command: string;
  tool: string;
  target: string;
  risk: string;
  args: Record<string, unknown>;
  items?: number;
}

export interface BatchStatus {
  total: number;
  succeeded: number;
  failed: number;
  partial: boolean;
}

let activeCommand = "";

export function setActiveCommand(command: string): void {
  activeCommand = command;
}

export function activeCommandName(): string {
  return activeCommand;
}

export function renderJson(payload: unknown, opts: Pick<GlobalOptions, "pretty">): string {
  return opts.pretty === true ? JSON.stringify(payload, null, 2) : JSON.stringify(payload);
}

export function err(message: string): void {
  process.stderr.write(`[gda] ${message}\n`);
}

export function outputFormat(opts: Pick<GlobalOptions, "formatText">): "json" {
  const value = opts.formatText;
  if (value === undefined || value === "json") return "json";
  throw new CliError(
    ExitCode.Usage,
    `--format must be json, got: ${value}. Use --export-format for trace exports.`,
  );
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

function baseMeta(opts?: GlobalOptions): Record<string, unknown> {
  return {
    request_id: "",
    instance_id: opts?.id ?? "",
    project: opts?.project ?? "",
    duration_ms: 0,
  };
}

export function successEnvelope(
  command: string,
  data: unknown,
  opts?: GlobalOptions,
  meta: Record<string, unknown> = {},
  warnings: unknown[] = [],
): CliEnvelope {
  return {
    schema_version: ENVELOPE_SCHEMA_VERSION,
    ok: true,
    command,
    data: data === undefined ? null : data,
    error: null,
    meta: { ...baseMeta(opts), ...meta },
    warnings,
  };
}

export function failureEnvelope(
  command: string,
  error: unknown,
  opts?: GlobalOptions,
  meta: Record<string, unknown> = {},
): CliEnvelope {
  const cliError = error instanceof CliError ? error : undefined;
  const exitCode = cliError?.code ?? ExitCode.ToolFailure;
  const info: CliEnvelopeError = {
    code: errorCodeForExit(exitCode),
    message: error instanceof Error ? error.message : String(error),
    retryable: exitCode === ExitCode.Transport,
    request_sent: isIpcRequestSent(error),
  };
  if (cliError?.suggestion !== undefined) info.suggestion = cliError.suggestion;
  if (cliError?.details !== undefined) {
    for (const [key, value] of Object.entries(cliError.details)) info[key] = value;
  }
  return {
    schema_version: ENVELOPE_SCHEMA_VERSION,
    ok: false,
    command,
    data: null,
    error: info,
    meta: { ...baseMeta(opts), ...meta },
    warnings: [],
  };
}

function normalizeError(value: unknown, ok: boolean): CliEnvelopeError | null {
  if (value === undefined || value === null) {
    return ok ? null : {
      code: errorCodeForExit(ExitCode.ToolFailure),
      message: "tool failure",
      retryable: false,
      request_sent: false,
    };
  }
  if (typeof value === "string") {
    return {
      code: errorCodeForExit(ok ? ExitCode.Success : ExitCode.ToolFailure),
      message: value,
      retryable: false,
      request_sent: false,
    };
  }
  if (!isRecord(value)) return null;
  const record = value;
  const fallback = errorCodeForExit(ok ? ExitCode.Success : ExitCode.ToolFailure);
  const info: CliEnvelopeError = {
    code: typeof record.code === "string" && record.code !== "" ? record.code : fallback,
    message: typeof record.message === "string" ? record.message : "",
    retryable: record.retryable === true,
    request_sent: record.request_sent === true || record.requestSent === true,
  };
  if (typeof record.suggestion === "string") info.suggestion = record.suggestion;
  for (const [key, entry] of Object.entries(record)) {
    if (key !== "code" && key !== "message" && key !== "retryable" &&
        key !== "request_sent" && key !== "requestSent" && key !== "suggestion") {
      info[key] = entry;
    }
  }
  return info;
}

export function isEnvelopeLike(value: unknown): boolean {
  return isRecord(value) && "schema_version" in value && "ok" in value;
}

export function normalizeEnvelope(
  payload: unknown,
  command: string,
  opts?: GlobalOptions,
  ok = true,
): CliEnvelope {
  if (isEnvelopeLike(payload)) {
    const raw = payload as Record<string, unknown>;
    const normalizedOk = typeof raw.ok === "boolean" ? raw.ok : ok;
    const resolvedCommand = typeof raw.command === "string" && raw.command !== "" ? raw.command : command;
    const controlKeys = new Set(["schema_version", "ok", "command", "data", "error", "meta", "warnings"]);
    const residual: Record<string, unknown> = {};
    for (const [key, value] of Object.entries(raw)) {
      if (!controlKeys.has(key)) residual[key] = value;
    }
    const data = raw.data !== undefined
      ? raw.data
      : Object.keys(residual).length > 0 ? residual : null;
    return {
      schema_version: typeof raw.schema_version === "number"
        ? raw.schema_version
        : ENVELOPE_SCHEMA_VERSION,
      ok: normalizedOk,
      command: resolvedCommand,
      data,
      error: normalizeError(raw.error, normalizedOk),
      meta: isRecord(raw.meta) ? { ...baseMeta(opts), ...raw.meta } : baseMeta(opts),
      warnings: Array.isArray(raw.warnings) ? raw.warnings : [],
    };
  }
  if (ok) return successEnvelope(command, payload, opts);
  const message = typeof payload === "string" ? payload : "tool failure";
  return failureEnvelope(command, new CliError(ExitCode.ToolFailure, message), opts);
}

export function writeEnvelope(envelope: CliEnvelope, opts: GlobalOptions): void {
  outputFormat(opts);
  process.stdout.write(`${renderJson(envelope, opts)}\n`);
}

export function writeJson(
  payload: unknown,
  opts: GlobalOptions,
  command?: string,
  ok = true,
): void {
  if (opts.legacyOutput === true) {
    process.stdout.write(`${renderJson(payload, opts)}\n`);
    return;
  }
  writeEnvelope(normalizeEnvelope(payload, command ?? activeCommandName(), opts, ok), opts);
}

export function writeFailure(
  command: string,
  error: unknown,
  opts: GlobalOptions,
): number {
  const exitCode = error instanceof CliError ? error.code : ExitCode.ToolFailure;
  writeEnvelope(failureEnvelope(command, error, opts), opts);
  err(error instanceof Error ? error.message : String(error));
  if (error instanceof CliError && error.suggestion !== undefined) {
    err(`suggestion: ${error.suggestion}`);
  }
  return exitCode;
}

export function confirmationText(summary: ConfirmationSummary): string {
  const target = summary.target === "" ? "(none)" : summary.target;
  const items = summary.items === undefined ? "" : ` items=${summary.items}`;
  return `confirm ${summary.command}: tool=${summary.tool} target=${target} risk=${summary.risk} args=${JSON.stringify(summary.args)}${items}`;
}

export function isConfirmed(opts: GlobalOptions): boolean {
  return opts.confirm === true;
}

export function recordConfirmation(summary: ConfirmationSummary): void {
  err(confirmationText(summary));
}

export function requireConfirmation(summary: ConfirmationSummary, opts: GlobalOptions): void {
  if (isConfirmed(opts)) {
    recordConfirmation(summary);
    return;
  }
  throw new ConfirmationRequiredError(
    `gda ${summary.command} requires --confirm; no tool call was sent.`,
    { ...summary, args: { ...summary.args } },
    "review the tool, target, risk and arguments above, then re-run with --confirm.",
  );
}

export async function promptConfirmation(
  summary: ConfirmationSummary,
  opts: GlobalOptions,
): Promise<void> {
  if (isConfirmed(opts)) {
    recordConfirmation(summary);
    return;
  }
  if (process.stdin.isTTY === true) {
    await promptInteractive(summary);
    return;
  }
  requireConfirmation(summary, opts);
}

export function confirmOrDecline(answer: string, summary: ConfirmationSummary): void {
  if (!/^y(es)?$/i.test(answer.trim())) {
    throw new UserDeclinedError(
      "confirmation declined; no tool call was sent.",
      { ...summary, args: { ...summary.args } },
    );
  }
  recordConfirmation(summary);
}

export async function promptInteractive(summary: ConfirmationSummary): Promise<void> {
  if (process.stdin.isTTY !== true) {
    throw new ConfirmationRequiredError(
      `gda ${summary.command} requires --confirm in a non-interactive session; no tool call was sent.`,
      { ...summary, args: { ...summary.args } },
      "re-run with --confirm after reviewing the tool, target, risk and arguments.",
    );
  }
  const rl = readline.createInterface({ input: process.stdin, output: process.stderr });
  let answer = "";
  try {
    answer = await rl.question(`Confirm ${summary.command}? [y/N] `);
  } finally {
    rl.close();
  }
  confirmOrDecline(answer, summary);
}

export function interrupted(): InterruptedError {
  return new InterruptedError();
}

export function batchStatus(results: readonly { ok: boolean }[]): BatchStatus {
  const failed = results.filter((result) => !result.ok).length;
  const succeeded = results.length - failed;
  return {
    total: results.length,
    succeeded,
    failed,
    partial: failed > 0 && succeeded > 0,
  };
}
