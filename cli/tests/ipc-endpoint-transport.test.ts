import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { afterEach, beforeEach, describe, expect, test } from "bun:test";

import type { GlobalOptions } from "../src/args";
import { run as runCategories } from "../src/commands/categories";
import { run as runDescribe } from "../src/commands/describe";
import { run as runPing } from "../src/commands/ping";
import { run as runSearch } from "../src/commands/search";
import { run as runTools } from "../src/commands/tools";
import { CliError, ExitCode } from "../src/errors";
import { pluginIndexPath } from "../src/instances";
import type { NativeRequest, NativeSend } from "./support/native-ipc";
import { contentResult, rpcError, rpcResult, sendHandshake, startNativeServer } from "./support/native-ipc";

interface RecordingServer {
  port: number;
  requests: NativeRequest[];
  close(): Promise<void>;
}

interface AuthServer extends RecordingServer {
  tokens: unknown[];
}

async function startRecordingServer(business: unknown): Promise<RecordingServer> {
  const server = await startNativeServer((message: NativeRequest, _socket: unknown, send: NativeSend) => {
    if (message.method === "initialize") {
      sendHandshake(message, send);
      return;
    }
    if (message.method === "notifications/initialized") return;
    send(contentResult(message.id, business));
  });
  return { port: server.port, requests: server.requests, close: server.close };
}

async function startAuthServer(token: string): Promise<AuthServer> {
  const tokens: unknown[] = [];
  const server = await startNativeServer((message: NativeRequest, _socket: unknown, send: NativeSend) => {
    if (message.method === "initialize") {
      const params = message.params ?? {};
      const meta = params["_meta"];
      const presented = meta !== null && typeof meta === "object" && !Array.isArray(meta)
        ? (meta as Record<string, unknown>)["gda_token"]
        : undefined;
      tokens.push(presented);
      if (presented !== token) {
        send(rpcError(message.id, -32001, "IPC token rejected"));
        return;
      }
      send(rpcResult(message.id, {
        protocolVersion: "1",
        capabilities: {},
        serverInfo: { name: "gda-testbed", version: "1" },
      }));
      return;
    }
    if (message.method === "notifications/initialized") return;
    send(contentResult(message.id, { ok: true }));
  });
  return { port: server.port, requests: server.requests, tokens, close: server.close };
}

function writeIndex(project: string, legacyPort: number, ipc: Record<string, unknown>): void {
  const file = pluginIndexPath();
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, JSON.stringify([{
    instance_id: "gda-t3",
    port: legacyPort,
    pid: process.pid,
    project_path: project,
    endpoints: { ipc },
    started_at: Date.now(),
    version: "0.3.5",
    port_source: "persisted",
  }]), "utf8");
}

function captureRun(fn: () => Promise<number>): Promise<{ code: number; stdout: string; stderr: string }> {
  const originalOut = process.stdout.write;
  const originalErr = process.stderr.write;
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
  const restore = (): void => {
    process.stdout.write = originalOut;
    process.stderr.write = originalErr;
  };
  return fn().then(
    (code) => {
      restore();
      return { code, stdout, stderr };
    },
    (error: unknown) => {
      restore();
      throw error;
    },
  );
}

function toolCalls(server: RecordingServer): NativeRequest[] {
  return server.requests.filter((request) => request.method === "tools/call");
}

function callParams(request: NativeRequest): Record<string, unknown> {
  return request.params ?? {};
}

let tmp: string;
let prevHome: string | undefined;
let prevCliHome: string | undefined;
let prevPort: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-ipc-endpoint-transport-"));
  prevHome = process.env.GODOT_AUTOPILOT_HOME;
  prevCliHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  prevPort = process.env.GODOT_AUTOPILOT_PORT;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmp;
  delete process.env.GODOT_AUTOPILOT_PORT;
});

afterEach(() => {
  if (prevHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = prevHome;
  if (prevCliHome === undefined) delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  else process.env.GODOT_AUTOPILOT_CLI_HOME = prevCliHome;
  if (prevPort === undefined) delete process.env.GODOT_AUTOPILOT_PORT;
  else process.env.GODOT_AUTOPILOT_PORT = prevPort;
  fs.rmSync(tmp, { recursive: true, force: true });
});

describe("顶层命令 IPC 端点透传", () => {
  test("登记端点端口与 legacy 端口不同且带 token 时 ping 连到登记端点并完成鉴权", async () => {
    const legacy = await startRecordingServer({ route: "legacy" });
    const registered = await startAuthServer("registered-token");
    try {
      expect(registered.port).not.toBe(legacy.port);
      const project = path.join(tmp, "proj");
      writeIndex(project, legacy.port, {
        kind: "tcp",
        host: "127.0.0.1",
        port: registered.port,
        token: "registered-token",
      });
      const result = await captureRun(() => runPing([], { project, timeoutText: "5" }));
      expect(result.code).toBe(ExitCode.Success);
      expect(JSON.parse(result.stdout).data).toEqual({ ok: true });
      expect(registered.tokens).toEqual(["registered-token"]);
      expect(toolCalls(registered).map(callParams)).toEqual([
        { name: "ping", arguments: {} },
      ]);
      expect(legacy.requests).toHaveLength(0);
    } finally {
      await legacy.close();
      await registered.close();
    }
  });

  test("tools list 连到登记端点并按请求形状调用 search_tools", async () => {
    const legacy = await startRecordingServer({ route: "legacy" });
    const registered = await startAuthServer("registered-token");
    try {
      const project = path.join(tmp, "proj");
      writeIndex(project, legacy.port, {
        kind: "tcp",
        host: "127.0.0.1",
        port: registered.port,
        token: "registered-token",
      });
      const result = await captureRun(() => runTools(["list"], { project, timeoutText: "5" }));
      expect(result.code).toBe(ExitCode.Success);
      expect(registered.tokens).toEqual(["registered-token"]);
      expect(toolCalls(registered).map(callParams)).toEqual([
        { name: "search_tools", arguments: { limit: 0 } },
      ]);
      expect(legacy.requests).toHaveLength(0);
    } finally {
      await legacy.close();
      await registered.close();
    }
  });

  const cases: Array<{ name: string; run(opts: GlobalOptions): Promise<number>; tool: string; args: Record<string, unknown> }> = [
    { name: "categories", run: (opts) => runCategories([], opts), tool: "list_categories", args: {} },
    { name: "search categories", run: (opts) => runSearch([], opts), tool: "list_categories", args: {} },
    { name: "search tools", run: (opts) => runSearch(["needle"], opts), tool: "search_tools", args: { query: "needle" } },
    { name: "describe", run: (opts) => runDescribe(["ping"], opts), tool: "get_tool_detail", args: { name: "ping" } },
  ];

  for (const item of cases) {
    test(item.name + " 透传登记端点且不触碰 legacy 端口", async () => {
      const legacy = await startRecordingServer({ route: "legacy" });
      const registered = await startAuthServer("registered-token");
      try {
        const project = path.join(tmp, "proj");
        writeIndex(project, legacy.port, {
          kind: "tcp",
          host: "127.0.0.1",
          port: registered.port,
          token: "registered-token",
        });
        const result = await captureRun(() => item.run({ project, timeoutText: "5" }));
        expect(result.code).toBe(ExitCode.Success);
        expect(registered.tokens).toEqual(["registered-token"]);
        expect(toolCalls(registered).map(callParams)).toEqual([
          { name: item.tool, arguments: item.args },
        ]);
        expect(legacy.requests).toHaveLength(0);
      } finally {
        await legacy.close();
        await registered.close();
      }
    });
  }

  test("UNIX kind 登记端点把 socket 路径透传到传输层", async () => {
    const legacy = await startRecordingServer({ route: "legacy" });
    try {
      const project = path.join(tmp, "proj");
      const socketPath = path.join(tmp, "gda-t3.sock");
      writeIndex(project, legacy.port, {
        kind: "unix",
        path: socketPath,
        token: "unix-token",
      });
      let caught: unknown;
      try {
        await runPing([], { project, timeoutText: "5" });
      } catch (error) {
        caught = error;
      }
      expect(caught).toBeInstanceOf(CliError);
      expect((caught as CliError).code).toBe(ExitCode.Transport);
      expect((caught as CliError).message).toContain("socket " + socketPath);
      expect(legacy.requests).toHaveLength(0);
    } finally {
      await legacy.close();
    }
  });
});
