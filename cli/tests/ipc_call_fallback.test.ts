import net from "node:net";

import { describe, expect, test } from "bun:test";

import { run as runCall } from "../src/commands/call";
import { CliError, ExitCode, isIpcRequestSent } from "../src/errors";
import { callTool } from "../src/rpc/client";
import { startNativeServer, sendHandshake } from "./support/native-ipc";

function captureOutput(): { stdout(): string; stderr(): string; restore(): void } {
  const originalOut = process.stdout.write;
  const originalErr = process.stderr.write;
  let stdout = "";
  let stderr = "";
  process.stdout.write = ((chunk: unknown) => {
    stdout += String(chunk);
    return true;
  }) as typeof process.stderr.write;
  process.stderr.write = ((chunk: unknown) => {
    stderr += String(chunk);
    return true;
  }) as typeof process.stderr.write;
  return {
    stdout: () => stdout,
    stderr: () => stderr,
    restore: () => {
      process.stdout.write = originalOut;
      process.stderr.write = originalErr;
    },
  };
}

describe("call 原生通道安全语义", () => {
  test("IPC 超时后不重发，工具调用仅一次", async () => {
    let ipcCalls = 0;
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      if (message.method === "tools/call") ipcCalls += 1;
    });
    const output = captureOutput();
    let caught: unknown;
    try {
      caught = await callTool("mutating_tool", {}, {
        ipcEndpoint: { kind: "tcp", host: "127.0.0.1", port: server.port },
        port: 0,
        timeoutSec: 0.2,
      }).catch((error: unknown) => error);
    } finally {
      output.restore();
      await server.close();
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Transport);
    expect(isIpcRequestSent(caught)).toBe(true);
    expect((caught as CliError).message).toContain("timed out");
    expect(ipcCalls).toBe(1);
    expect(output.stderr()).not.toContain("falling back");
  });

  test("IPC 收到工具调用后断开不重发，工具调用仅一次", async () => {
    let ipcCalls = 0;
    const server = await startNativeServer((message, socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      if (message.method === "tools/call") {
        ipcCalls += 1;
        socket.destroy();
      }
    });
    const output = captureOutput();
    let caught: unknown;
    try {
      caught = await callTool("mutating_tool", {}, {
        ipcEndpoint: { kind: "tcp", host: "127.0.0.1", port: server.port },
        port: 0,
        timeoutSec: 2,
      }).catch((error: unknown) => error);
    } finally {
      output.restore();
      await server.close();
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Transport);
    expect(isIpcRequestSent(caught)).toBe(true);
    expect((caught as CliError).message).toContain("execution result is unknown");
    expect(ipcCalls).toBe(1);
    expect(output.stderr()).not.toContain("falling back");
  });

  test("runCall 的 --port 目标只连接原生 IPC", async () => {
    const seen: string[] = [];
    const server = await startNativeServer((message, _socket, send) => {
      seen.push(String(message.method));
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      send({
        jsonrpc: "2.0",
        id: message.id,
        result: { content: [{ type: "text", text: JSON.stringify({ ok: true }) }] },
      });
    });
    const output = captureOutput();
    let code: number = ExitCode.Success;
    try {
      code = await runCall(["ping"], { portText: String(server.port) });
    } finally {
      output.restore();
      await server.close();
    }
    expect(code).toBe(ExitCode.Success);
    expect(seen).toEqual(["initialize", "notifications/initialized", "tools/call"]);
    expect(JSON.parse(output.stdout()).data).toEqual({ ok: true });
    expect(output.stderr()).not.toContain("falling back");
  });
});
