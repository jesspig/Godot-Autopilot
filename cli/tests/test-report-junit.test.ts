import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import type { GlobalOptions } from "../src/args";
import { findSpec } from "../src/command-spec";
import { junitXml, normalizeTestSummary, run as runTest } from "../src/commands/test";
import { ExitCode } from "../src/errors";

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

async function serveNative(result: unknown): Promise<FakeNative> {
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
        const body = Buffer.from(JSON.stringify({
          v: 1,
          kind: "response",
          id: `r-${String(request.id)}`,
          instance_id: String(request.instance_id ?? ""),
          method: String(request.method ?? ""),
          params: [{ key: "result", value: JSON.stringify(result) }],
          status: "ok",
          error: 0,
          error_message: "",
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

function writePluginIndex(tmp: string, port: number): void {
  const dir = path.join(tmp, ".godot-autopilot");
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, "instances.json"), JSON.stringify({
    instances: [{
      instance_id: "t10-report",
      port,
      pid: process.pid,
      project_path: path.join(tmp, "proj"),
      endpoints: {
        ipc: { kind: "tcp", host: "127.0.0.1", port, protocol: "gda-ipc-v1", framing: "length-prefixed-json", auth: "token", version: 1 },
      },
      started_at: Date.now(),
      version: "0.3.6",
      port_source: "persisted",
    }],
  }));
}

function captureStdout(): { stdout(): string; restore(): void } {
  const original = process.stdout.write;
  let output = "";
  process.stdout.write = ((chunk: unknown) => { output += String(chunk); return true; }) as typeof process.stdout.write;
  return { stdout: () => output, restore: () => { process.stdout.write = original; } };
}

const REPORT_PAYLOAD = {
  summary: { total: 3, passed: 1, failed: 1, skipped: 1 },
  cases: [
    { name: "passes", status: "pass", duration_ms: 3 },
    { name: "fails \"quoted\"", status: "fail", message: "expected <a> & b", duration_ms: 4 },
    { name: "skips", status: "skip", message: "no fixture", duration_ms: 0 },
  ],
  coverage: { lines: 0.75, branches: 0.5 },
};

const INVOCATION = {
  status: "ok",
  payload: REPORT_PAYLOAD,
  artifact_id: "artifact-report",
  digest: "digest-report",
  bytes: 64,
  task_id: "",
};

let tmp: string;
let native: FakeNative | undefined;
let prevCliHome: string | undefined;
let prevGdaHome: string | undefined;
let prevPort: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-t10-junit-"));
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

describe("T-10 JUnit 报告输出", () => {
  test("summary/cases 字段命名归一且统计与用例一致", () => {
    const summary = normalizeTestSummary(REPORT_PAYLOAD);
    expect(summary).toMatchObject({ total: 3, passed: 1, failed: 1, skipped: 1 });
    expect(summary.cases.map((entry) => entry.status)).toEqual(["pass", "fail", "skip"]);
  });

  test("JUnit XML 使用 testsuite/testcase/failure/skipped 并转义特殊字符", () => {
    const xml = junitXml("run & smoke", normalizeTestSummary(REPORT_PAYLOAD));
    expect(xml.startsWith('<?xml version="1.0" encoding="UTF-8"?>')).toBe(true);
    expect(xml).toContain('<testsuite name="run &amp; smoke" tests="3" failures="1" skipped="1"');
    expect(xml).toContain('<failure message="expected &lt;a&gt; &amp; b"');
    expect(xml).toContain('<skipped message="no fixture"');
    expect(xml).toContain('name="fails &quot;quoted&quot;"');
  });

  test("test.run 将 JUnit 写入指定路径并在 envelope 中返回 junit_path 与 coverage", async () => {
    native = await serveNative(INVOCATION);
    writePluginIndex(tmp, native.port);
    const junit = path.join(tmp, "junit", "report.xml");
    fs.mkdirSync(path.dirname(junit), { recursive: true });
    const cap = captureStdout();
    let code: number;
    try {
      code = await runTest(["run"], {
        portText: String(native.port),
        "--junit": junit,
        "--label": "smoke",
        "--coverage": true,
      } as unknown as GlobalOptions);
    } finally {
      cap.restore();
    }
    expect(code).toBe(ExitCode.ToolFailure);
    const body = JSON.parse(cap.stdout());
    expect(body.command).toBe("test.run");
    expect(body.data.junit_path).toBe(junit);
    expect(body.data.summary.failed).toBe(1);
    expect(body.data.coverage).toEqual({ lines: 0.75, branches: 0.5 });
    const xml = fs.readFileSync(junit, "utf8");
    expect(xml).toContain('<testsuite name="smoke"');
    const call = native.requests.find((request) => request.method === "call.start");
    expect(paramsOf(call!).tool).toBe("run_gdscript_tests");
  });

  test("test scenario 未确认不发送工具调用", async () => {
    native = await serveNative(INVOCATION);
    writePluginIndex(tmp, native.port);
    let error: unknown;
    try {
      await runTest(["scenario"], { portText: String(native.port) } as unknown as GlobalOptions);
    } catch (caught) {
      error = caught;
    }
    expect(error).toBeDefined();
    expect(native.requests.filter((request) => request.method === "call.start").length).toBe(0);
    expect(findSpec("test.scenario")!.risk).toBe("high");
  });
});