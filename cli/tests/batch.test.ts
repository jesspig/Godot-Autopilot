import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import { afterEach, beforeEach, describe, expect, test } from "bun:test";

import { run as batch } from "../src/commands/batch";
import { CliError, ExitCode } from "../src/errors";

function encodeFrame(value: unknown): Buffer {
  const payload = Buffer.from(JSON.stringify(value), "utf8");
  const header = Buffer.alloc(4);
  header.writeUInt32BE(payload.length, 0);
  return Buffer.concat([header, payload]);
}

function frameServer(handler: (message: any, socket: net.Socket) => void): (socket: net.Socket) => void {
  return (socket) => {
    let buffer = Buffer.alloc(0);
    socket.on("data", (chunk: Buffer) => {
      buffer = Buffer.concat([buffer, chunk]);
      while (buffer.length >= 4) {
        const size = buffer.readUInt32BE(0);
        if (buffer.length < 4 + size) return;
        const text = buffer.subarray(4, 4 + size).toString("utf8");
        buffer = buffer.subarray(4 + size);
        try {
          handler(JSON.parse(text), socket);
        } catch {
        }
      }
    });
  };
}

const activeSockets: net.Socket[] = [];
const servers: net.Server[] = [];

function listenLoopback(socketPath: string, handler: (socket: net.Socket) => void): Promise<net.Server> {
  return new Promise((resolve, reject) => {
    const server = net.createServer(handler);
    server.on("connection", (socket) => activeSockets.push(socket));
    server.once("error", reject);
    server.listen(socketPath, () => resolve(server));
  });
}

function closeServer(server: net.Server): Promise<void> {
  for (const socket of activeSockets.splice(0)) {
    socket.destroy();
  }
  return new Promise((resolve) => server.close(() => resolve()));
}

function captureOutput(): { stdout(): string; restore(): void } {
  const origOut = process.stdout.write;
  let outBuf = "";
  process.stdout.write = ((chunk: unknown) => {
    outBuf += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  return {
    stdout: () => outBuf,
    restore: () => {
      process.stdout.write = origOut;
    },
  };
}

async function expectSessionClosed(): Promise<void> {
  const socket = activeSockets[0];
  expect(socket).toBeDefined();
  if (socket!.destroyed) return;
  await new Promise<void>((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error("batch IPC session did not close")), 1000);
    socket!.once("close", () => {
      clearTimeout(timer);
      resolve();
    });
  });
  expect(Boolean(socket!.destroyed)).toBe(true);
}

interface ServerState {
  initializeCount: number;
  calls: string[];
  seenConfirm?: boolean;
}

interface BatchServerOptions {
  failTool?: string | null;
  hangTool?: string | null;
  disconnectTool?: string | null;
  invalidResponseTool?: string | null;
}

function batchServer(state: ServerState, options: BatchServerOptions = {}): (socket: net.Socket) => void {
  return frameServer((message, socket) => {
    if (message.id === undefined) return;
    let result: unknown;
    if (message.method === "initialize") {
      state.initializeCount += 1;
      result = { protocolVersion: "2025-11-25", capabilities: {}, serverInfo: { name: "mock", version: "1" } };
    } else if (message.method === "tools/call") {
      const params = message.params ?? {};
      const inner = params.name === "call_tool" ? params.arguments : params;
      const name = typeof inner?.name === "string" ? inner.name : "";
      state.calls.push(name);
      if (name === options.disconnectTool) {
        socket.destroy();
        return;
      }
      if (name === options.hangTool) {
        return;
      }
      if (name === options.failTool || name === "fail_tool") {
        state.seenConfirm = inner?.arguments?.confirm;
        result = { isError: true, content: [{ type: "text", text: '{"error":"boom"}' }] };
      } else if (name === options.invalidResponseTool) {
        result = {};
      } else {
        result = { content: [{ type: "text", text: JSON.stringify({ ok: true, tool: name }) }] };
      }
    } else {
      result = {};
    }
    socket.write(encodeFrame({ jsonrpc: "2.0", id: message.id, result }));
  });
}

let dir: string;
let socketPath: string;

beforeEach(() => {
  dir = fs.mkdtempSync(path.join(os.tmpdir(), "gb-"));
  socketPath = path.join(dir, "gbatch.sock");
});

afterEach(async () => {
  while (servers.length > 0) {
    await closeServer(servers.pop()!);
  }
  fs.rmSync(socketPath, { force: true });
  fs.rmSync(dir, { recursive: true, force: true });
});

function writeBatchFile(value: unknown, raw?: string): string {
  const file = path.join(dir, "b.json");
  fs.writeFileSync(file, raw ?? JSON.stringify(value));
  return file;
}

describe("gda batch", () => {
  test("两个操作全成功：exit 0、results 两项 ok、握手只做一次", async () => {
    const state: ServerState = { initializeCount: 0, calls: [] };
    const server = await listenLoopback(socketPath, batchServer(state));
    servers.push(server);
    const file = writeBatchFile([{ tool: "alpha", args: { a: 1 } }, { tool: "beta" }]);
    const cap = captureOutput();
    let code: number;
    try {
      code = await batch([], { argsFile: file, socketText: socketPath, timeoutText: "5" });
    } finally {
      cap.restore();
    }
    expect(code).toBe(ExitCode.Success);
    const output = JSON.parse(cap.stdout()) as { data: { results: unknown[] } };
    expect(output.data.results).toHaveLength(2);
    expect(output.data.results[0]).toMatchObject({ index: 0, tool: "alpha", ok: true });
    expect(output.data.results[1]).toMatchObject({ index: 1, tool: "beta", ok: true });
    expect(state.initializeCount).toBe(1);
    expect(state.calls).toEqual(["alpha", "beta"]);
  });

  test("一个 toolFailed：exit 1 且对应项 ok=false，confirm 写入参数", async () => {
    const state: ServerState = { initializeCount: 0, calls: [] };
    const server = await listenLoopback(socketPath, batchServer(state, { failTool: "fail_tool" }));
    servers.push(server);
    const file = writeBatchFile({
      operations: [{ tool: "alpha" }, { tool: "fail_tool", args: {}, confirm: true }],
    });
    const cap = captureOutput();
    let code: number;
    try {
      code = await batch([], { argsFile: file, socketText: socketPath, timeoutText: "5" });
    } finally {
      cap.restore();
    }
    expect(code).toBe(ExitCode.ToolFailure);
    const output = JSON.parse(cap.stdout()) as { data: { results: { ok: boolean }[] } };
    expect(output.data.results[0].ok).toBe(true);
    expect(output.data.results[1].ok).toBe(false);
    expect(state.seenConfirm).toBe(true);
    expect(state.calls).toEqual(["alpha", "fail_tool"]);
    expect(state.initializeCount).toBe(1);
  });

  test("IPC timeout 立即停止 batch、关闭 session 并返回 Transport", async () => {
    const state: ServerState = { initializeCount: 0, calls: [] };
    const server = await listenLoopback(socketPath, batchServer(state, { hangTool: "alpha" }));
    servers.push(server);
    const file = writeBatchFile([{ tool: "alpha" }, { tool: "beta" }]);
    const cap = captureOutput();
    let code: number;
    try {
      code = await batch([], { argsFile: file, socketText: socketPath, timeoutText: "0.2" });
    } finally {
      cap.restore();
    }
    expect(code).toBe(ExitCode.Transport);
    const output = JSON.parse(cap.stdout()) as { data: { results: { ok: boolean; error?: string }[] } };
    expect(output.data.results).toHaveLength(1);
    expect(output.data.results[0].ok).toBe(false);
    expect(output.data.results[0].error).toContain("timed out");
    expect(output.data.results[0].error).toContain("execution result is unknown");
    expect(state.calls).toEqual(["alpha"]);
    expect(state.initializeCount).toBe(1);
    await expectSessionClosed();
  });

  test("IPC 收到工具调用后断开立即停止 batch 并返回 Transport", async () => {
    const state: ServerState = { initializeCount: 0, calls: [] };
    const server = await listenLoopback(socketPath, batchServer(state, { disconnectTool: "alpha" }));
    servers.push(server);
    const file = writeBatchFile([{ tool: "alpha" }, { tool: "beta" }]);
    const cap = captureOutput();
    let code: number;
    try {
      code = await batch([], { argsFile: file, socketText: socketPath, timeoutText: "2" });
    } finally {
      cap.restore();
    }
    expect(code).toBe(ExitCode.Transport);
    const output = JSON.parse(cap.stdout()) as { data: { results: { ok: boolean; error?: string }[] } };
    expect(output.data.results).toHaveLength(1);
    expect(output.data.results[0].ok).toBe(false);
    expect(output.data.results[0].error).toContain("execution result is unknown");
    expect(state.calls).toEqual(["alpha"]);
    expect(state.initializeCount).toBe(1);
  });

  test("batch 遇到 Transport 错误立即停止并保留退出码", async () => {
    const state: ServerState = { initializeCount: 0, calls: [] };
    const server = await listenLoopback(socketPath, batchServer(state, { invalidResponseTool: "alpha" }));
    servers.push(server);
    const file = writeBatchFile([{ tool: "alpha" }, { tool: "beta" }]);
    const cap = captureOutput();
    let code: number;
    try {
      code = await batch([], { argsFile: file, socketText: socketPath, timeoutText: "2" });
    } finally {
      cap.restore();
    }
    expect(code).toBe(ExitCode.Transport);
    const output = JSON.parse(cap.stdout()) as { data: { results: { ok: boolean; error?: string }[] } };
    expect(output.data.results).toHaveLength(1);
    expect(output.data.results[0].ok).toBe(false);
    expect(output.data.results[0].error).toContain("no result.content");
    expect(state.calls).toEqual(["alpha"]);
    expect(state.initializeCount).toBe(1);
    await expectSessionClosed();
  });

  test("非法 batch 文件：exit 3 (Usage)", async () => {
    const file = writeBatchFile(null, '{"operations":');
    let caught: unknown;
    try {
      await batch([], { argsFile: file, socketText: socketPath });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Usage);
  });

  test("无可用 IPC endpoint：exit 2 (Transport)", async () => {
    const file = writeBatchFile([{ tool: "alpha" }]);
    let caught: unknown;
    try {
      await batch([], { argsFile: file, portText: "1" });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Transport);
  });
});
