import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import type { GlobalOptions } from "../src/args";
import { findSpec } from "../src/command-spec";
import { run as runDebug, pngPath } from "../src/commands/debug";
import { run as runJob } from "../src/commands/job";
import { run as runLogs, selectTail } from "../src/commands/logs";
import { run as runMonitor, withImageExtension } from "../src/commands/monitor";
import { run as runTest, junitXml, normalizeTestSummary } from "../src/commands/test";
import { run as runTrace } from "../src/commands/trace";
import { CliError, ExitCode } from "../src/errors";

interface Answer {
  result?: unknown;
  status?: string;
  error?: number;
  error_message?: string;
}

interface FakeNative {
  port: number;
  requests: Array<Record<string, unknown>>;
  close(): Promise<void>;
}

function paramsOf(request: Record<string, unknown>): Record<string, string> {
  const params = Array.isArray(request.params) ? request.params as Array<{ key: string; value: string }> : [];
  const out: Record<string, string> = {};
  for (const entry of params) out[entry.key] = entry.value;
  return out;
}

async function serveNative(handler: (method: string, params: Record<string, string>) => Answer): Promise<FakeNative> {
  const requests: Array<Record<string, unknown>> = [];
  const sockets: net.Socket[] = [];
  const server = net.createServer((socket) => {
    sockets.push(socket);
    let buffer = Buffer.alloc(0);
    socket.on("data", (chunk: Buffer) => {
      buffer = Buffer.concat([buffer, chunk]);
      while (buffer.length >= 4) {
        const size = buffer.readUInt32BE(0);
        if (buffer.length < 4 + size) break;
        const text = buffer.subarray(4, 4 + size).toString("utf8");
        buffer = buffer.subarray(4 + size);
        const request = JSON.parse(text) as Record<string, unknown>;
        requests.push(request);
        const method = String(request.method ?? "");
        const answer = handler(method, paramsOf(request)) ?? {};
        const body = Buffer.from(JSON.stringify({
          v: 1,
          kind: "response",
          id: `r-${String(request.id)}`,
          instance_id: String(request.instance_id ?? ""),
          method,
          params: [{ key: "result", value: JSON.stringify(answer.result ?? {}) }],
          status: answer.status ?? "ok",
          error: answer.error ?? 0,
          error_message: answer.error_message ?? "",
        }));
        const header = Buffer.alloc(4);
        header.writeUInt32BE(body.length, 0);
        socket.write(Buffer.concat([header, body]));
      }
    });
  });
  await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
  return {
    port: (server.address() as net.AddressInfo).port,
    requests,
    close: async () => {
      for (const socket of sockets) socket.destroy();
      await new Promise<void>((resolve) => server.close(() => resolve()));
    },
  };
}

function writePluginIndex(tmp: string, port: number, instanceId: string): void {
  const dir = path.join(tmp, ".godot-autopilot");
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, "instances.json"), JSON.stringify({
    instances: [{
      instance_id: instanceId,
      port,
      pid: process.pid,
      project_path: path.join(tmp, "proj"),
      endpoints: {
        ipc: { kind: "tcp", host: "127.0.0.1", port, protocol: "mcp-jsonrpc", framing: "length-prefixed-json", auth: "token", version: 1 },
      },
      started_at: Date.now(),
      version: "0.3.6",
      port_source: "persisted",
    }],
  }));
}

function capture(): { stdout(): string; stderr(): string; restore(): void } {
  const out = process.stdout.write;
  const err = process.stderr.write;
  let stdout = "";
  let stderr = "";
  process.stdout.write = ((chunk: unknown) => { stdout += String(chunk); return true; }) as typeof process.stdout.write;
  process.stderr.write = ((chunk: unknown) => { stderr += String(chunk); return true; }) as typeof process.stderr.write;
  return {
    stdout: () => stdout,
    stderr: () => stderr,
    restore: () => { process.stdout.write = out; process.stderr.write = err; },
  };
}

const INVOCATION = (payload: unknown, taskId = ""): unknown => ({
  status: "ok",
  payload,
  artifact_id: "artifact-t10",
  digest: "digest-t10",
  bytes: 16,
  task_id: taskId,
});

const TASK = (taskId: string, state: string, extra: Record<string, unknown> = {}): unknown => ({
  task_id: taskId,
  instance_id: "t10-native",
  generation: 2,
  state,
  progress: 100,
  progress_message: "done",
  result: { ok: true },
  error: 0,
  error_symbol: "none",
  error_message: "",
  cancel_stage: extra.cancel_stage ?? "",
});

function defaultValue(method: string, params: Record<string, string>): Answer {
  if (method === "call.start") {
    const tool = params.tool ?? "";
    if (tool === "capture_editor_viewport") return { result: INVOCATION({ path: "captures/frame.png", ok: true }) };
    if (tool === "get_debugger_stream") return { result: INVOCATION({ entries: [{ level: "error", message: "boom" }], new_errors_since_last_call: 1 }) };
    if (tool === "get_game_log_entries" || tool === "get_plugin_log") return { result: INVOCATION({ entries: [{ level: "info", time: "2026-11-26T00:00:00Z", source: "game", message: "ready" }] }) };
    if (tool === "run_gdscript_tests") {
      return { result: INVOCATION({
        summary: { total: 2, passed: 1, failed: 1, skipped: 0 },
        cases: [
          { name: "adds", status: "pass", duration_ms: 1 },
          { name: "fails <x&y>", status: "fail", message: "bad & <value>", duration_ms: 2 },
        ],
        coverage: { lines: 0.9 },
      }) };
    }
    return { result: INVOCATION({ ok: true, session_id: "session-1", sessions: [{ id: "session-1", frames: 4 }] }) };
  }
  if (method === "task.get") return { result: TASK(params.task_id ?? "job-1", "succeeded") };
  if (method === "task.list") return { result: [TASK("job-1", "succeeded")] };
  if (method === "task.cancel") return { result: TASK(params.task_id ?? "job-1", "cancelling", { cancel_stage: "requested" }) };
  if (method === "events.subscribe") return { result: { subscription_id: "sub-1" } };
  if (method === "events.resume") return { result: {
    delivered: 1,
    events: [{ seq: 7, instance_id: "t10-native", task_id: params.task_id ?? "job-1", event_type: "progress", payload: { progress: 50 }, traceparent: "00-abc-def-01" }],
  } };
  return { result: {} };
}

let tmp: string;
let native: FakeNative | undefined;
let prevCliHome: string | undefined;
let prevGdaHome: string | undefined;
let prevPort: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-t10-"));
  prevCliHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  prevGdaHome = process.env.GODOT_AUTOPILOT_HOME;
  prevPort = process.env.GODOT_AUTOPILOT_PORT;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmp;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
  delete process.env.GODOT_AUTOPILOT_PORT;
});

afterEach(async () => {
  if (native !== undefined) {
    await native.close();
    native = undefined;
  }
  if (prevCliHome === undefined) delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  else process.env.GODOT_AUTOPILOT_CLI_HOME = prevCliHome;
  if (prevGdaHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = prevGdaHome;
  if (prevPort === undefined) delete process.env.GODOT_AUTOPILOT_PORT;
  else process.env.GODOT_AUTOPILOT_PORT = prevPort;
  fs.rmSync(tmp, { recursive: true, force: true });
});

async function withNative(handler?: (method: string, params: Record<string, string>) => Answer): Promise<GlobalOptions> {
  native = await serveNative((method, params) => handler?.(method, params) ?? defaultValue(method, params));
  writePluginIndex(tmp, native.port, "t10-native");
  return { portText: String(native.port) } as unknown as GlobalOptions;
}

async function captureRun(fn: () => Promise<number>): Promise<{ code: number; stdout: string; stderr: string; error?: CliError }> {
  const cap = capture();
  try {
    return { code: await fn(), stdout: cap.stdout(), stderr: cap.stderr() };
  } catch (error) {
    return { code: -1, stdout: cap.stdout(), stderr: cap.stderr(), error: error as CliError };
  } finally {
    cap.restore();
  }
}

describe("T-10 命令契约", () => {
  test("debug/logs/test/job 子命令与 detach flags 在 spec 中登记", () => {
    const expected: ReadonlyArray<readonly [string, string[]]> = [
      ["debug", ["status", "errors", "output", "stack", "inspect", "trace", "repro", "report"]],
      ["logs", ["editor", "game", "plugin", "cli"]],
      ["test", ["run", "script", "scenario", "watch"]],
      ["job", ["list", "status", "wait", "cancel", "logs"]],
    ];
    for (const [group, subs] of expected) {
      for (const sub of subs) {
        const spec = findSpec(`${group}.${sub}`);
        expect(spec).toBeDefined();
        expect(spec!.tools.length).toBeGreaterThan(0);
      }
    }
    expect(findSpec("debug.repro")!.flags.map((flag) => flag.name)).toContain("--detach");
    expect(findSpec("test.run")!.flags.map((flag) => flag.name)).toContain("--junit");
    expect(findSpec("job.logs")!.flags.map((flag) => flag.name)).toContain("--cursor");
    expect(findSpec("trace.list")!.flags.map((flag) => flag.name)).toContain("--limit");
    expect(findSpec("tool.call")!.tools).toContain("tool_call");
  });

  test("tail 0 与 limit 0 返回空页，图片扩展名统一为 .png", async () => {
    expect(selectTail(["a", "b"], 0)).toEqual([]);
    expect(pngPath("capture.jpeg")).toBe("capture.jpeg.png");
    expect(withImageExtension("frame.jpG")).toBe("frame.jpG.png");
    const traceDir = path.join(tmp, ".godot-autopilot", "cli-traces");
    fs.mkdirSync(traceDir, { recursive: true });
    fs.writeFileSync(path.join(traceDir, "trace-20261126.jsonl"),
      `${JSON.stringify({ source: "cli", kind: "tool_call", tool: "property_get", trace_id: "t1", wall_end_ms: 1 })}\n`, "utf8");
    const result = await captureRun(() => runTrace(["list"], { "--limit": 0 } as unknown as GlobalOptions));
    expect(result.code).toBe(ExitCode.Success);
    expect(JSON.parse(result.stdout).data).toMatchObject({ count: 0, limit: 0, events: [] });
  });

  test("debug repro 编排多个步骤，缺 --confirm 不发送任何工具调用", async () => {
    const opts = await withNative();
    const denied = await captureRun(() => runDebug(["repro"], opts));
    expect(denied.error).toBeInstanceOf(CliError);
    expect(native!.requests.filter((request) => request.method === "call.start").length).toBe(0);
    const approved = await captureRun(() => runDebug(["repro"], {
      ...opts, confirm: true, "--path": "res://main.tscn",
    } as unknown as GlobalOptions));
    expect(approved.code).toBe(ExitCode.Success);
    const body = JSON.parse(approved.stdout);
    expect(body.command).toBe("debug.repro");
    expect(body.data.steps.length).toBe(5);
    expect(body.data.steps.map((step: { name: string }) => step.name)).toEqual(["play", "input", "sample", "capture", "errors"]);
    expect(body.data.image_path).toBe("captures/frame.png");
    const tools = native!.requests.filter((request) => request.method === "call.start")
      .map((request) => paramsOf(request).tool);
    expect(new Set(tools).size).toBeGreaterThanOrEqual(4);
  });

  test("logs editor/game/cli 过滤、导出并保留 tail 0 语义", async () => {
    const opts = await withNative();
    const logFile = path.join(tmp, "editor.log");
    fs.writeFileSync(logFile, "old\nready\nerror: fail\n");
    fs.writeFileSync(path.join(tmp, "instances.json"), JSON.stringify({
      instances: [{ id: "editor-1", log: logFile, project: tmp, port: native!.port, pid: process.pid, startedAt: String(Date.now()), headless: true, godot: "godot", latestStatus: "ok", statusTimestamp: 0 }],
    }));
    const empty = await captureRun(() => runLogs(["editor", "editor-1"], {
      ...opts, "--tail": 0,
    } as unknown as GlobalOptions));
    expect(empty.code).toBe(ExitCode.Success);
    expect(JSON.parse(empty.stdout).data.count).toBe(0);

    const exported = path.join(tmp, "exported.log");
    const game = await captureRun(() => runLogs(["game"], {
      ...opts, "--level": "info", "--source": "game", "--export": exported, "--tail": 5,
    } as unknown as GlobalOptions));
    expect(game.code).toBe(ExitCode.Success);
    expect(fs.readFileSync(exported, "utf8")).toContain("ready");
    expect(JSON.parse(game.stdout).data.export_path).toBe(exported);
    const cli = await captureRun(() => runLogs(["cli"], opts));
    expect(JSON.parse(cli.stdout).data.source).toBe("cli");
  });
});

describe("T-10 测试报告与任务状态", () => {
  test("JUnit XML 转义失败信息，coverage 与 shard/retries 参数进入 envelope", async () => {
    const summary = normalizeTestSummary({
      summary: { total: 2, passed: 1, failed: 1, skipped: 0 },
      cases: [{ name: "bad <x&y>", status: "fail", message: "bad & <value>" }],
    });
    const xml = junitXml("smoke", summary);
    expect(xml).toContain("&lt;x&amp;y&gt;");
    expect(xml).toContain("<failure message=\"bad &amp; &lt;value&gt;\"");
    const junit = path.join(tmp, "report.xml");
    const opts = await withNative();
    const result = await captureRun(() => runTest(["run"], {
      ...opts,
      "--junit": junit,
      "--label": "smoke",
      "--shard": "0/2",
      "--retries": 1,
      "--coverage": true,
    } as unknown as GlobalOptions));
    expect(result.code).toBe(ExitCode.ToolFailure);
    const body = JSON.parse(result.stdout);
    expect(body.command).toBe("test.run");
    expect(body.data.junit_path).toBe(junit);
    expect(body.data.coverage).toEqual({ lines: 0.9 });
    expect(fs.readFileSync(junit, "utf8")).toContain("<testsuite");
    const call = native!.requests.find((request) => request.method === "call.start");
    const callParams = paramsOf(call!);
    expect(callParams.tool).toBe("run_gdscript_tests");
    expect(callParams.shard_index).toBe("0");
    expect(callParams.shard_count).toBe("2");
    expect(callParams.retries).toBe("1");
    expect(callParams.changed_only).toBeUndefined();
  });

  test("detach 时 stdout 只有一份 envelope，进度写入 stderr", async () => {
    const opts = await withNative();
    const result = await captureRun(() => runTest(["watch"], {
      ...opts,
      "--detach": true,
      "--timeout": "1",
      "--interval": 1,
    } as unknown as GlobalOptions));
    expect(result.code).toBe(ExitCode.Success);
    expect(result.stdout.trim().split("\n").length).toBe(1);
    expect(JSON.parse(result.stdout).data.detached).toBe(true);
    expect(result.stderr).toContain("\"kind\":\"progress\"");
  });

  test("job status 绑定 generation 并持久化终态，cancel 报告真实阶段", async () => {
    const opts = await withNative();
    const status = await captureRun(() => runJob(["status", "job-1"], {
      ...opts, "--generation": 2,
    } as unknown as GlobalOptions));
    expect(status.code).toBe(ExitCode.Success);
    const body = JSON.parse(status.stdout);
    expect(body.data).toMatchObject({ task_id: "job-1", instance_id: "t10-native", generation: 2, state: "succeeded" });
    const storePath = path.join(tmp, "cli", "jobs.json");
    const stored = JSON.parse(fs.readFileSync(storePath, "utf8"));
    expect(Object.values(stored)[0]).toMatchObject({ terminal: true, cancel_stage: "" });
    const denied = await captureRun(() => runJob(["cancel", "job-1"], opts));
    expect(denied.error).toBeInstanceOf(CliError);
    const cancelled = await captureRun(() => runJob(["cancel", "job-1"], {
      ...opts, confirm: true, "--generation": 2,
    } as unknown as GlobalOptions));
    expect(cancelled.code).toBe(ExitCode.Success);
    expect(JSON.parse(cancelled.stdout).data.cancel_stage).toBe("requested");
    expect(JSON.parse(cancelled.stdout).data.cancelled).toBe(false);
  });

  test("monitor sessions limit 0 使用 session_id 字段命名", async () => {
    const opts = await withNative();
    const result = await captureRun(() => runMonitor(["sessions"], {
      ...opts, "--limit": 0,
    } as unknown as GlobalOptions));
    expect(result.code).toBe(ExitCode.Success);
    const data = JSON.parse(result.stdout).data;
    expect(data.sessions).toEqual([]);
    expect(data.count).toBe(0);
  });
});
