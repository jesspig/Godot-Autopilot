import fs from "node:fs";

import type { GlobalOptions } from "../args";
import {
  nativeTaskGet,
  nativeToolInvoke,
  type NativeCallOutcome,
  type NativeTaskRecord,
} from "../discovery";
import { CliError, ExitCode } from "../errors";
import type { IpcEndpoint } from "../ipc/client";
import { findById, findLiveByProject } from "../registry";
import { resolveNativeTarget } from "../target";
import { writeJson } from "./json-output";
import {
  confirmed,
  flagBool,
  flagNumber,
  flagString,
  parseJsonValue,
  readArgsPayload,
} from "./scene";

const TARGET_CONTEXT = { findById, findLiveByProject };

interface TestCase {
  name: string;
  status: string;
  message: string;
  duration_ms: number;
}

interface TestSummary {
  total: number;
  passed: number;
  failed: number;
  skipped: number;
  cases: TestCase[];
}

interface Binding {
  endpoint: IpcEndpoint;
  instanceId: string;
}

function binding(opts: GlobalOptions): Binding {
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

function timeoutMs(opts: GlobalOptions): number | undefined {
  const seconds = flagString(opts, "--timeout") ?? opts.timeoutText;
  if (seconds === undefined) return undefined;
  const value = Number(seconds);
  return Number.isFinite(value) && value > 0 ? value * 1000 : undefined;
}

function progress(opts: GlobalOptions, command: string, message: string, taskId = ""): void {
  if (opts.progress !== true && !flagBool(opts, "--detach")) return;
  process.stderr.write(`${JSON.stringify({
    kind: "progress",
    command,
    message,
    task_id: taskId,
  })}\n`);
}

function asRecord(value: unknown): Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value)
    ? value as Record<string, unknown>
    : {};
}

export function normalizeTestSummary(payload: unknown): TestSummary {
  const record = asRecord(payload);
  const rawCases = Array.isArray(record.cases) ? record.cases : [];
  const cases: TestCase[] = rawCases.map((item, index) => {
    const entry = asRecord(item);
    const status = typeof entry.status === "string" ? entry.status : "pass";
    return {
      name: typeof entry.name === "string" && entry.name !== "" ? entry.name : `case_${index + 1}`,
      status,
      message: typeof entry.message === "string" ? entry.message : "",
      duration_ms: typeof entry.duration_ms === "number" && Number.isFinite(entry.duration_ms) ? entry.duration_ms : 0,
    };
  });
  const summary = asRecord(record.summary);
  const count = (key: string, fallback: number): number =>
    typeof summary[key] === "number" && Number.isFinite(summary[key]) ? summary[key] as number : fallback;
  return {
    total: count("total", cases.length),
    passed: count("passed", cases.filter((entry) => entry.status === "pass").length),
    failed: count("failed", cases.filter((entry) => entry.status === "fail").length),
    skipped: count("skipped", cases.filter((entry) => entry.status === "skip").length),
    cases,
  };
}

function xmlEscape(text: string): string {
  return text
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&apos;");
}

export function junitXml(label: string, summary: TestSummary): string {
  const name = xmlEscape(label === "" ? "gda.test" : label);
  const lines = [
    '<?xml version="1.0" encoding="UTF-8"?>',
    `<testsuite name="${name}" tests="${summary.total}" failures="${summary.failed}" skipped="${summary.skipped}" errors="0">`,
  ];
  for (const entry of summary.cases) {
    const attributes = `name="${xmlEscape(entry.name)}" classname="${name}" time="${(entry.duration_ms / 1000).toFixed(3)}"`;
    if (entry.status === "fail") {
      lines.push(`  <testcase ${attributes}><failure message="${xmlEscape(entry.message || "assertion failed")}"/></testcase>`);
    } else if (entry.status === "skip") {
      lines.push(`  <testcase ${attributes}><skipped message="${xmlEscape(entry.message || "skipped")}"/></testcase>`);
    } else {
      lines.push(`  <testcase ${attributes}/>`);
    }
  }
  lines.push("</testsuite>");
  return `${lines.join("\n")}\n`;
}

function writeJUnit(opts: GlobalOptions, summary: TestSummary): string {
  const file = flagString(opts, "--junit") ?? "";
  if (file === "") return "";
  fs.writeFileSync(file, junitXml(flagString(opts, "--label") ?? "", summary), "utf8");
  return file;
}

function emit(command: string, data: Record<string, unknown>, opts: GlobalOptions, ok: boolean): number {
  writeJson({
    schema_version: 1,
    ok,
    command,
    data,
    error: ok ? undefined : data.error ?? null,
    meta: {
      instance_id: typeof data.instance_id === "string" ? data.instance_id : "",
      task_id: typeof data.task_id === "string" ? data.task_id : "",
    },
    warnings: [],
  }, opts);
  return ok ? ExitCode.Success : ExitCode.ToolFailure;
}

function commonArgs(opts: GlobalOptions, args: Record<string, unknown>): Record<string, unknown> {
  const label = flagString(opts, "--label");
  if (label !== undefined) args.label = label;
  if (flagBool(opts, "--changed")) args.changed_only = true;
  if (flagBool(opts, "--failed-first")) args.failed_first = true;
  const shard = flagString(opts, "--shard");
  if (shard !== undefined) {
    const parts = shard.split("/");
    const index = Number(parts[0]);
    const count = Number(parts[1]);
    if (!Number.isInteger(index) || !Number.isInteger(count) || count <= 0 || index < 0 || index >= count) {
      throw new CliError(ExitCode.Usage, `--shard must look like index/count (0 <= index < count), got: ${shard}`);
    }
    args.shard_index = index;
    args.shard_count = count;
  }
  const retries = flagNumber(opts, "--retries");
  if (retries !== undefined) {
    if (!Number.isInteger(retries) || retries < 0) {
      throw new CliError(ExitCode.Usage, "--retries must be a non-negative integer");
    }
    args.retries = retries;
  }
  const budget = timeoutMs(opts);
  if (budget !== undefined) args.timeout_ms = budget;
  return args;
}

async function invokeTest(
  target: Binding,
  command: string,
  tool: string,
  args: Record<string, unknown>,
  opts: GlobalOptions,
): Promise<{ outcome: NativeCallOutcome; task?: NativeTaskRecord }> {
  const started = await nativeToolInvoke(target.endpoint, target.instanceId, tool, args, {
    timeoutMs: timeoutMs(opts),
  });
  progress(opts, command, `${tool} started`, started.task_id);
  let task: NativeTaskRecord | undefined;
  if (started.status === "pending" && started.task_id !== "" && !flagBool(opts, "--detach")) {
    const deadline = Date.now() + (timeoutMs(opts) ?? 300_000);
    for (;;) {
      task = await nativeTaskGet(target.endpoint, target.instanceId, started.task_id, 0, {
        timeoutMs: timeoutMs(opts),
      });
      progress(opts, command, `${task.state} ${task.progress}`, task.task_id);
      if (["succeeded", "failed", "cancelled", "interrupted"].includes(task.state)) break;
      if (Date.now() >= deadline) {
        throw new CliError(ExitCode.Transport, `test task ${started.task_id} did not finish before the timeout.`);
      }
      await new Promise((resolve) => setTimeout(resolve, 250));
    }
  }
  return { outcome: started, task };
}

function coverageOf(payload: unknown, enabled: boolean): unknown {
  if (!enabled) return null;
  const record = asRecord(payload);
  return record.coverage ?? null;
}

async function finish(
  command: string,
  target: Binding,
  opts: GlobalOptions,
  toolArgs: Record<string, unknown>,
  tool: string,
): Promise<number> {
  const { outcome, task } = await invokeTest(target, command, tool, toolArgs, opts);
  const payload = task !== undefined && task.state === "succeeded" ? task.result : outcome.payload;
  const summary = normalizeTestSummary(payload);
  const junitPath = writeJUnit(opts, summary);
  const failed = summary.failed > 0 || (task !== undefined && task.state === "failed");
  return emit(command, {
    instance_id: target.instanceId,
    task_id: task?.task_id ?? outcome.task_id,
    state: task?.state ?? outcome.status,
    detached: flagBool(opts, "--detach"),
    summary,
    junit_path: junitPath,
    coverage: coverageOf(payload, flagBool(opts, "--coverage")),
    request_id: outcome.request_id,
  }, opts, !failed && outcome.ok);
}

export async function run(positional: string[], opts: GlobalOptions): Promise<number> {
  const sub = positional[0] ?? "list";
  if (sub === "list") {
    const target = binding(opts);
    const { outcome } = await invokeTest(target, "test.list", "resource_query", { action: "list", kind: "tests" }, opts);
    return emit("test.list", { instance_id: target.instanceId, suites: outcome.payload }, opts, outcome.ok);
  }
  if (sub === "status") {
    const id = positional[1];
    if (id === undefined) throw new CliError(ExitCode.Usage, "test status requires a <run-id>");
    const target = binding(opts);
    const { outcome } = await invokeTest(target, "test.status", "run_gdscript_tests", {
      action: "status", task_id: id,
    }, opts);
    return emit("test.status", {
      instance_id: target.instanceId, task_id: id, result: outcome.payload,
    }, opts, outcome.ok);
  }
  const target = binding(opts);
  if (sub === "run") {
    const suite = positional[1];
    const args = commonArgs(opts, { ...readArgsPayload(opts) });
    if (suite !== undefined) args.suite = suite;
    const directory = flagString(opts, "--directory");
    const pattern = flagString(opts, "--pattern");
    if (directory !== undefined) args.directory = directory;
    if (pattern !== undefined) args.pattern = pattern;
    return finish("test.run", target, opts, args, "run_gdscript_tests");
  }
  if (sub === "script") {
    const source = flagString(opts, "--source");
    const argsFile = flagString(opts, "--args-file");
    let tests: unknown;
    if (source !== undefined) {
      tests = parseJsonValue(source, "--source");
    } else {
      const payload = readArgsPayload(opts);
      tests = payload.tests ?? payload;
    }
    if (argsFile === undefined && source === undefined && tests === undefined) {
      throw new CliError(ExitCode.Usage, "test script requires --source or --args-file");
    }
    return finish("test.script", target, opts, commonArgs(opts, { tests }), "run_gdscript_tests");
  }
  if (sub === "scenario") {
    if (!confirmed(opts)) {
      throw new CliError(
        ExitCode.Usage,
        "gda test scenario is high-risk and requires --confirm; no tool call was sent.",
        "re-run with --confirm after reviewing the scenario.",
      );
    }
    const args = commonArgs(opts, { ...readArgsPayload(opts), action: "sequence" });
    const { outcome } = await invokeTest(target, "test.scenario", "game_input", args, opts);
    const errors = await nativeToolInvoke(target.endpoint, target.instanceId, "get_debugger_stream", {
      action: "errors", limit: 50,
    }, { timeoutMs: timeoutMs(opts) });
    const label = flagString(opts, "--label") ?? "scenario";
    const summary: TestSummary = {
      total: 1,
      passed: errors.payload === null || asRecord(errors.payload).errors === undefined ? 1 : 0,
      failed: asRecord(errors.payload).errors === undefined ? 0 : 1,
      skipped: 0,
      cases: [{ name: label, status: asRecord(errors.payload).errors === undefined ? "pass" : "fail", message: "", duration_ms: 0 }],
    };
    const junitPath = writeJUnit(opts, summary);
    return emit("test.scenario", {
      instance_id: target.instanceId,
      task_id: outcome.task_id,
      summary,
      junit_path: junitPath,
      coverage: coverageOf(outcome.payload, flagBool(opts, "--coverage")),
      request_id: outcome.request_id,
    }, opts, outcome.ok && summary.failed === 0);
  }
  if (sub === "watch") {
    const deadline = Date.now() + (timeoutMs(opts) ?? 60_000);
    let rounds = 0;
    let last: NativeCallOutcome | undefined;
    while (Date.now() < deadline) {
      rounds += 1;
      last = await nativeToolInvoke(target.endpoint, target.instanceId, "get_game_state", { action: "status" }, {
        timeoutMs: timeoutMs(opts),
      });
      progress(opts, "test.watch", `poll ${rounds}`, last.task_id);
      if (last.task_id !== "" && flagBool(opts, "--detach")) break;
      const interval = flagNumber(opts, "--interval") ?? Number(opts.intervalText ?? "1");
      await new Promise((resolve) => setTimeout(resolve, Math.max(0, Math.min(1000, interval * 1000))));
    }
    const summary = normalizeTestSummary(last?.payload);
    const junitPath = writeJUnit(opts, summary);
    return emit("test.watch", {
      instance_id: target.instanceId,
      task_id: last?.task_id ?? "",
      detached: flagBool(opts, "--detach"),
      rounds,
      summary,
      junit_path: junitPath,
      coverage: coverageOf(last?.payload, flagBool(opts, "--coverage")),
    }, opts, last === undefined || last.ok);
  }
  throw new CliError(ExitCode.Usage, "unknown test subcommand: use run, script, scenario, watch, list, or status");
}
