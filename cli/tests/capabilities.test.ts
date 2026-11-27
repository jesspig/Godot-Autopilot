import { describe, expect, test } from "bun:test";

import type { GlobalOptions } from "../src/args";
import { run as call } from "../src/commands/call";
import { run as status } from "../src/commands/status";
import { ConfirmationRequiredError, ExitCode } from "../src/errors";
import { rpcResult, startNativeServer, sendHandshake } from "./support/native-ipc";

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

describe("native capabilities", () => {
  test("status 输出统一 envelope（协议专属字段已删除）", async () => {
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send, "1");
        return;
      }
      if (message.method === "notifications/initialized") return;
      send(rpcResult(message.id, {
        content: [{ type: "text", text: JSON.stringify({ version: "0.3.3", tool_count: 459 }) }],
      }));
    });
    try {
      const cap = capture();
      const code = await status([], { portText: String(server.port) });
      cap.restore();
      expect(code).toBe(ExitCode.Success);
      const envelope = JSON.parse(cap.stdout()) as {
        schema_version: number;
        ok: boolean;
        command: string;
        data: { port: number; server: Record<string, unknown> };
      };
      expect(envelope.schema_version).toBe(1);
      expect(envelope.ok).toBe(true);
      expect(envelope.command).toBe("editor.status");
      expect(envelope.data.port).toBe(server.port);
      expect(envelope.data.server).toEqual({ version: "0.3.3", tool_count: 459 });
    } finally {
      await server.close();
    }
  });

  test("input_required 抛 ConfirmationRequiredError（exit 4，提示 --confirm）且不重发", async () => {
    let calls = 0;
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      if (message.method === "notifications/initialized") return;
      calls += 1;
      send(rpcResult(message.id, {
        input_required: { input_requests: { confirm: { method: "elicitation/create", params: {} } } },
      }));
    });
    try {
      const cap = capture();
      let caught: unknown;
      try {
        await call(["do_risky_thing"], { portText: String(server.port) });
      } catch (error) {
        caught = error;
      } finally {
        cap.restore();
      }
      expect(caught).toBeInstanceOf(ConfirmationRequiredError);
      const failure = caught as ConfirmationRequiredError;
      expect(failure.code).toBe(ExitCode.ConfirmationRequired);
      expect(failure.suggestion).toContain("--confirm");
      expect(calls).toBe(1);
      expect(cap.stdout()).toBe("");
    } finally {
      await server.close();
    }
  });

  test("input_required 下旧 --input 不再触发重试（calls 保持 1）", async () => {
    let calls = 0;
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      if (message.method === "notifications/initialized") return;
      calls += 1;
      send(rpcResult(message.id, {
        input_required: { input_requests: { confirm: { method: "elicitation/create", params: {} } } },
      }));
    });
    try {
      const cap = capture();
      let caught: unknown;
      try {
        await call(["do_risky_thing"], {
          portText: String(server.port),
          inputText: '{"confirm":{"action":"accept"}}',
        } as unknown as GlobalOptions);
      } catch (error) {
        caught = error;
      } finally {
        cap.restore();
      }
      expect(caught).toBeInstanceOf(ConfirmationRequiredError);
      expect((caught as ConfirmationRequiredError).code).toBe(ExitCode.ConfirmationRequired);
      expect(calls).toBe(1);
      expect(cap.stdout()).toBe("");
    } finally {
      await server.close();
    }
  });

  test("input_required 带 --confirm 仍不重发，确认摘要写入 stderr", async () => {
    let calls = 0;
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      if (message.method === "notifications/initialized") return;
      calls += 1;
      send(rpcResult(message.id, {
        input_required: { input_requests: { confirm: { method: "elicitation/create", params: {} } } },
      }));
    });
    try {
      const cap = capture();
      let caught: unknown;
      try {
        await call(["do_risky_thing"], { portText: String(server.port), confirm: true });
      } catch (error) {
        caught = error;
      } finally {
        cap.restore();
      }
      expect(caught).toBeInstanceOf(ConfirmationRequiredError);
      expect((caught as ConfirmationRequiredError).code).toBe(ExitCode.ConfirmationRequired);
      expect(calls).toBe(1);
      expect(cap.stderr()).toContain("confirm tool.call");
      expect(cap.stdout()).toBe("");
    } finally {
      await server.close();
    }
  });

  test("call --progress 写 stderr，stdout 保持 JSON", async () => {
    const server = await startNativeServer((message, _socket, send) => {
      if (message.method === "initialize") {
        sendHandshake(message, send);
        return;
      }
      if (message.method === "notifications/initialized") return;
      send({
        jsonrpc: "2.0",
        method: "notifications/progress",
        params: { progressToken: "p-1", progress: 2, total: 5, message: "step" },
      });
      send(rpcResult(message.id, {
        content: [{ type: "text", text: JSON.stringify({ ok: true }) }],
      }));
    });
    try {
      const cap = capture();
      const code = await call(["batch_execute"], {
        portText: String(server.port),
        progress: true,
        argsJson: '{"operations":[]}',
      });
      cap.restore();
      expect(code).toBe(ExitCode.Success);
      expect(cap.stderr()).toContain("progress 2/5 step");
      expect(JSON.parse(cap.stdout()).data).toEqual({ ok: true });
    } finally {
      await server.close();
    }
  });
});
