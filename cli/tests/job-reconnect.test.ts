import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import type { GlobalOptions } from "../src/args";
import { run as runJob } from "../src/commands/job";
import { ExitCode } from "../src/errors";

interface Answer {
  result?: unknown;
  status?: string;
  error?: number;
  error_message?: string;
}

interface FakeNative {
  port: number;
  requests: Array<{ method: string; params: Record<string, string> }>;
  closes: number;
  close(): Promise<void>;
}

function paramsOf(request: Record<string, unknown>): Record<string, string> {
  const params = Array.isArray(request.params) ? request.params as Array<{ key: string; value: string }> : [];
  const out: Record<string, string> = {};
  for (const entry of params) out[entry.key] = entry.value;
  return out;
}

async function serveNative(
  handler: (method: string, params: Record<string, string>, connection: number) => Answer | "drop",
): Promise<FakeNative> {
  const requests: Array<{ method: string; params: Record<string, string> }> = [];
  const sockets: net.Socket[] = [];
  let connections = 0;
  let closes = 0;
  const server = net.createServer((socket) => {
    connections += 1;
    const connection = connections;
    sockets.push(socket);
    socket.once("close", () => { closes += 1; });
    let buffer = Buffer.alloc(0);
    socket.on("data", (chunk: Buffer) => {
      buffer = Buffer.concat([buffer, chunk]);
      while (buffer.length >= 4) {
        const size = buffer.readUInt32BE(0);
        if (buffer.length < 4 + size) break;
        const text = buffer.subarray(4, 4 + size).toString("utf8");
        buffer = buffer.subarray(4 + size);
        const request = JSON.parse(text) as Record<string, unknown>;
        const method = String(request.method ?? "");
        const map = paramsOf(request);
        requests.push({ method, params: map });
        const answer = handler(method, map, connection);
        if (answer === "drop") {
          socket.destroy();
          return;
        }
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
    get closes() { return closes; },
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
        ipc: { kind: "tcp", host: "127.0.0.1", port, protocol: "gda-ipc-v1", framing: "length-prefixed-json", auth: "token", version: 1 },
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

async function captureRun(fn: () => Promise<number>): Promise<{ code: number; stdout: string; stderr: string }> {
  const cap = capture();
  try {
    return { code: await fn(), stdout: cap.stdout(), stderr: cap.stderr() };
  } finally {
    cap.restore();
  }
}

const TASK = (taskId: string): unknown => ({
  task_id: taskId,
  instance_id: "t10-reconnect",
  generation: 3,
  state: "succeeded",
  progress: 100,
  progress_message: "done",
  result: { ok: true, value: 42 },
  error: 0,
  error_symbol: "none",
  error_message: "",
  cancel_stage: "",
});

let tmp: string;
let native: FakeNative | undefined;
let prevCliHome: string | undefined;
let prevGdaHome: string | undefined;
let prevPort: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-t10-reconnect-"));
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

async function withNative(
  handler: (method: string, params: Record<string, string>, connection: number) => Answer | "drop",
): Promise<GlobalOptions> {
  native = await serveNative(handler);
  writePluginIndex(tmp, native.port, "t10-reconnect");
  return { portText: String(native.port) } as unknown as GlobalOptions;
}

describe("T-10 断线重连与过期", () => {
  test("job wait 在读取连接断开后重连，且不重发原始 call.start", async () => {
    const opts = await withNative((method, params, connection) => {
      if (method === "task.get" && connection === 1) return "drop";
      if (method === "task.get") return { result: TASK(params.task_id ?? "job-2") };
      return { result: {} };
    });
    const result = await captureRun(() => runJob(["wait", "job-2"], {
      ...opts, "--generation": 3, "--retries": 1, "--timeout": "2",
    } as unknown as GlobalOptions));
    expect(result.code).toBe(ExitCode.Success);
    const body = JSON.parse(result.stdout);
    expect(body.command).toBe("job.wait");
    expect(body.data.state).toBe("succeeded");
    expect(body.data.reconnects).toBe(1);
    expect(result.stderr).toContain("reconnect");
    expect(native!.requests.filter((request) => request.method === "call.start").length).toBe(0);
    expect(native!.requests.filter((request) => request.method === "task.get").length).toBe(2);
  });

  test("任务过期映射为 not_found，不陷入永久 working", async () => {
    const opts = await withNative((method) => {
      if (method === "task.get") {
        return { status: "error", error: 14, error_message: "task expired", result: {} };
      }
      return { result: {} };
    });
    const result = await captureRun(() => runJob(["status", "gone"], opts));
    expect(result.code).toBe(ExitCode.ToolFailure);
    const body = JSON.parse(result.stdout);
    expect(body.ok).toBe(false);
    expect(body.data.error.code).toBe("not_found");
    expect(body.data.state).toBe("not_found");
  });

  test("事件 cursor 过期映射为 resync_required", async () => {
    const opts = await withNative((method) => {
      if (method === "events.subscribe") {
        return { status: "error", error: 10, error_message: "cursor expired", result: {} };
      }
      return { result: {} };
    });
    const result = await captureRun(() => runJob(["logs"], {
      ...opts, "--task-id": "job-2", "--cursor": 3,
    } as unknown as GlobalOptions));
    expect(result.code).toBe(ExitCode.ToolFailure);
    expect(JSON.parse(result.stdout).data.error.code).toBe("resync_required");
  });

  test("编辑器重启后 generation/instance 变化报告 interrupted", async () => {
    const storeDir = path.join(tmp, "cli");
    fs.mkdirSync(storeDir, { recursive: true });
    fs.writeFileSync(path.join(storeDir, "jobs.json"), JSON.stringify({
      "old-instance/2/job-3": {
        task_id: "job-3",
        instance_id: "old-instance",
        generation: 2,
        state: "running",
        cancel_stage: "",
        progress: 10,
        progress_message: "running",
        result: null,
        error_symbol: "none",
        error_message: "",
        command: "test.run",
        started_at: "2026-11-26T00:00:00.000Z",
        updated_at: "2026-11-26T00:00:00.000Z",
        terminal: false,
      },
    }));
    const opts = await withNative(() => ({ result: {} }));
    const result = await captureRun(() => runJob(["status", "job-3"], {
      ...opts, "--generation": 3,
    } as unknown as GlobalOptions));
    expect(result.code).toBe(ExitCode.ToolFailure);
    const body = JSON.parse(result.stdout);
    expect(body.data.error.code).toBe("interrupted");
    expect(body.data.interrupted).toBe(true);
    expect(native!.requests.filter((request) => request.method === "task.get").length).toBe(0);
    const stored = JSON.parse(fs.readFileSync(path.join(storeDir, "jobs.json"), "utf8"));
    expect(stored["old-instance/2/job-3"].state).toBe("interrupted");
    expect(stored["old-instance/2/job-3"].terminal).toBe(true);
  });
});