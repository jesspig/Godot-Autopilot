import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { afterEach, beforeEach, describe, expect, test } from "bun:test";

import { run as call } from "../src/commands/call";
import { run as tasks } from "../src/commands/tasks";
import { listTasks, tasksDir } from "../src/tasks-index";
import { ExitCode } from "../src/errors";

const servers: ReturnType<typeof Bun.serve>[] = [];

afterEach(() => {
  while (servers.length > 0) servers.pop()!.stop();
});

function sse(payload: unknown): Response {
  return new Response(`event: message\ndata: ${JSON.stringify(payload)}\n\n`, {
    headers: { "content-type": "text/event-stream" },
  });
}

function server(handler: (method: string, params: unknown) => unknown) {
  const instance = Bun.serve({
    port: 0,
    fetch: async (req) => {
      const body = (await req.json()) as { method: string; id: number; params?: unknown };
      if (body.method === "initialize") {
        return sse({ jsonrpc: "2.0", id: body.id, result: { protocolVersion: "2025-11-25", capabilities: {} } });
      }
      if (body.method === "notifications/initialized") return new Response(null, { status: 202 });
      return sse({ jsonrpc: "2.0", id: body.id, result: handler(body.method, body.params) });
    },
  });
  servers.push(instance);
  return instance.port!;
}

function capture(): { stdout(): string; stderr(): string; restore(): void } {
  const out = process.stdout.write;
  const errw = process.stderr.write;
  let stdout = "";
  let stderr = "";
  process.stdout.write = ((chunk: unknown) => {
    stdout += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  process.stderr.write = ((chunk: unknown) => {
    stderr += String(chunk);
    return true;
  }) as typeof process.stderr.write;
  return {
    stdout: () => stdout,
    stderr: () => stderr,
    restore: () => {
      process.stdout.write = out;
      process.stderr.write = errw;
    },
  };
}

let home: string;

beforeEach(() => {
  home = fs.mkdtempSync(path.join(process.env.GDA_TEST_TMPDIR ?? os.tmpdir(), "gda-tasks-"));
  process.env.GODOT_AUTOPILOT_HOME = home;
  process.env.GODOT_AUTOPILOT_CLI_HOME = home;
});

afterEach(() => {
  delete process.env.GODOT_AUTOPILOT_HOME;
  delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  fs.rmSync(home, { recursive: true, force: true });
});

describe("gda tasks", () => {
  test("call --async 写入本地索引", async () => {
    const port = server(() => ({ resultType: "task", taskId: "t-1", status: "working" }));
    const cap = capture();
    const code = await call(["ping"], { portText: String(port), async: true });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(JSON.parse(cap.stdout()).task_id).toBe("t-1");
    const entries = listTasks();
    expect(entries).toHaveLength(1);
    expect(entries[0].task_id).toBe("t-1");
    expect(fs.existsSync(path.join(tasksDir(), "t-1.json"))).toBe(true);
  });

  test("call --async 同步退化时提示", async () => {
    const port = server(() => ({ content: [{ type: "text", text: JSON.stringify({ result: "pong" }) }] }));
    const cap = capture();
    const code = await call(["ping"], { portText: String(port), async: true });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(cap.stderr()).toContain("executed synchronously");
    expect(JSON.parse(cap.stdout())).toEqual({ result: "pong" });
  });

  test("wait 轮询到 completed", async () => {
    let polls = 0;
    const port = server((method) => {
      if (method === "tasks/get") {
        polls += 1;
        return { taskId: "t-2", status: polls > 1 ? "completed" : "working", result: { ok: true } };
      }
      return {};
    });
    const cap = capture();
    const code = await tasks(["wait", "t-2"], { portText: String(port), timeoutText: "5" });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(cap.stderr()).toContain("working");
    expect(JSON.parse(cap.stdout()).status).toBe("completed");
    expect(polls).toBeGreaterThan(1);
  });

  test("cancel 发 tasks/cancel", async () => {
    let seen = "";
    const port = server((method, params) => {
      seen = `${method}:${JSON.stringify(params)}`;
      return { taskId: "t-3", status: "cancelled" };
    });
    const cap = capture();
    const code = await tasks(["cancel", "t-3"], { portText: String(port) });
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(seen).toContain("tasks/cancel");
    expect(seen).toContain("t-3");
  });

  test("clean 只删除终态记录", async () => {
    fs.mkdirSync(tasksDir(), { recursive: true });
    fs.writeFileSync(path.join(tasksDir(), "done.json"), JSON.stringify({ task_id: "done", tool: "ping", args_hash: "", started_at: "", port: 1, instance_id: "", status: "completed" }));
    fs.writeFileSync(path.join(tasksDir(), "work.json"), JSON.stringify({ task_id: "work", tool: "ping", args_hash: "", started_at: "", port: 1, instance_id: "", status: "working" }));
    const cap = capture();
    const code = await tasks(["clean"], {});
    cap.restore();
    expect(code).toBe(ExitCode.Success);
    expect(JSON.parse(cap.stdout()).removed).toEqual(["done"]);
    expect(listTasks().map((entry) => entry.task_id)).toEqual(["work"]);
  });
});
