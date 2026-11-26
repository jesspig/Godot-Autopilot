import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import type { GlobalOptions } from "../src/args";
import { run as runEditor, editorPing, editorStatus } from "../src/commands/editor";
import { CliError } from "../src/errors";

interface FakeNative {
  port: number;
  requests: Array<Record<string, unknown>>;
  close(): Promise<void>;
}

async function serveNative(handler: (request: Record<string, unknown>) => unknown): Promise<FakeNative> {
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
        const result = handler(request);
        const body = Buffer.from(JSON.stringify({
          v: 1,
          kind: "response",
          id: `r-${String(request.id)}`,
          instance_id: request.instance_id ?? "",
          method: request.method ?? "",
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

function writePluginIndex(tmp: string, port: number, instanceId = "native-1"): void {
  const dir = path.join(tmp, ".godot-autopilot");
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, "instances.json"), JSON.stringify({
    instances: [{
      instance_id: instanceId,
      port,
      pid: process.pid,
      project_path: path.join(tmp, "proj"),
      endpoints: {
        mcp: { host: "127.0.0.1", port, path: "/mcp", protocol: "mcp", auth: "none" },
        ipc: { kind: "tcp", host: "127.0.0.1", port, protocol: "mcp-jsonrpc", framing: "length-prefixed-json", auth: "token", version: 1 },
      },
      started_at: Date.now(),
      version: "0.3.5",
      port_source: "persisted",
    }],
  }));
}

function capture(): { stdout(): string; restore(): void } {
  const original = process.stdout.write;
  let output = "";
  process.stdout.write = ((chunk: unknown) => {
    output += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  return { stdout: () => output, restore: () => { process.stdout.write = original; } };
}

async function captureRun(fn: () => Promise<number>): Promise<{ code: number; stdout: string }> {
  const cap = capture();
  try {
    const code = await fn();
    return { code, stdout: cap.stdout() };
  } finally {
    cap.restore();
  }
}

let tmp: string;
let prevCliHome: string | undefined;
let prevGdaHome: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-status-test-"));
  prevCliHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  prevGdaHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmp;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
  delete process.env.GODOT_AUTOPILOT_PORT;
});

afterEach(() => {
  if (prevCliHome === undefined) delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  else process.env.GODOT_AUTOPILOT_CLI_HOME = prevCliHome;
  if (prevGdaHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = prevGdaHome;
  fs.rmSync(tmp, { recursive: true, force: true });
});

describe("editor status 原生状态", () => {
  test("输出 instance/project/version/ipc_kind/ready/tool_count/capabilities，无 mcp_url/mcp/port", async () => {
    const native = await serveNative((request) => {
      if (request.method === "hello") {
        return { instance_id: "native-1", capabilities: { protocol_version: 1, methods: ["hello"], features: [] } };
      }
      if (request.method === "capabilities.get") {
        return { protocol_version: 1, methods: ["hello", "catalog.list"], features: ["native"] };
      }
      return [{ name: "scene_open", category: "Scene" }];
    });
    try {
      writePluginIndex(tmp, native.port);
      const opts: GlobalOptions = { portText: String(native.port) };
      const { code, stdout } = await captureRun(() => editorStatus(opts));
      expect(code).toBe(0);
      const body = JSON.parse(stdout);
      expect(body.ok).toBe(true);
      expect(body.data.instance.id).toBe("native-1");
      expect(typeof body.data.instance.pid).toBe("number");
      expect(body.data.project).toBe(path.join(tmp, "proj"));
      expect(body.data.version).toBe("0.3.5");
      expect(body.data.ipc_kind).toBe("tcp");
      expect(body.data.ready).toBe(true);
      expect(body.data.tool_count).toBe(1);
      expect(body.data.capabilities.protocol_version).toBe(1);
      for (const forbidden of ["mcp_url", "mcp", "port", "port_source", "transport"]) {
        expect(Object.keys(body.data)).not.toContain(forbidden);
      }
      for (const request of native.requests) {
        expect(request.v).toBe(1);
        expect(request.kind).toBe("request");
        expect(request.method).not.toBe("initialize");
        expect("jsonrpc" in request).toBe(false);
      }
      expect(native.requests.map((request) => request.method)).not.toContain("initialize");
    } finally {
      await native.close();
    }
  });

  test("editor status 子命令经 run 分发且无实例时返回结构化错误", async () => {
    const { code, stdout } = await captureRun(() => runEditor(["status"], {}));
    expect(code).toBe(2);
    const body = JSON.parse(stdout);
    expect(body.ok).toBe(false);
    expect(body.data.ready).toBe(false);
    expect(body.data.tool_count).toBe(0);
    expect(body.error.message).toContain("no running Godot Autopilot instance");
  });

  test("editor ping 走原生 hello，不发送 MCP initialize", async () => {
    const native = await serveNative((request) =>
      request.method === "hello" ? { instance_id: "native-1", capabilities: {} } : {},
    );
    try {
      writePluginIndex(tmp, native.port);
      const { code, stdout } = await captureRun(() => editorPing({ portText: String(native.port) }));
      expect(code).toBe(0);
      expect(JSON.parse(stdout).data.pong).toBe(true);
      expect(native.requests.map((request) => request.method)).toEqual(["hello"]);
      expect(native.requests.every((request) => request.kind === "request" && request.v === 1)).toBe(true);
    } finally {
      await native.close();
    }
  });

  test("未知 editor 子命令报用法错误", async () => {
    let caught: unknown;
    try {
      await runEditor(["nope"], {});
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).message).toContain("unknown editor subcommand");
  });
});
