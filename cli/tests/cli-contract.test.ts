import { describe, expect, test } from "bun:test";

import { CliError, ExitCode } from "../src/errors";
import {
  batchStatus,
  failureEnvelope,
  normalizeEnvelope,
  outputFormat,
  renderJson,
} from "../src/output";

function captureStdout(fn: () => void): string {
  const original = process.stdout.write;
  let text = "";
  process.stdout.write = ((chunk: string | Uint8Array) => {
    text += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  try {
    fn();
  } finally {
    process.stdout.write = original;
  }
  return text;
}

const FIELDS = ["schema_version", "ok", "command", "data", "error", "meta", "warnings"];

describe("CLI envelope 契约", () => {
  test("原始业务负载被包装成同形 envelope", () => {
    const envelope = normalizeEnvelope({ pong: true }, "editor.ping");
    expect(Object.keys(envelope).sort()).toEqual([...FIELDS].sort());
    expect(envelope.schema_version).toBe(1);
    expect(envelope.ok).toBe(true);
    expect(envelope.command).toBe("editor.ping");
    expect(envelope.data).toEqual({ pong: true });
    expect(envelope.error).toBeNull();
    expect(envelope.meta).toBeDefined();
    expect(envelope.warnings).toEqual([]);
  });

  test("已带 envelope 的输出被归一化：error 始终存在且 data 不丢失", () => {
    const envelope = normalizeEnvelope({
      schema_version: 1,
      ok: false,
      command: "job.cancel",
      error: { code: "CONFIRMATION_REQUIRED", message: "missing confirm" },
      meta: { task_id: "t1" },
    }, "ignored");
    expect(envelope.command).toBe("job.cancel");
    expect(envelope.ok).toBe(false);
    expect(envelope.data).toBeNull();
    expect(envelope.error?.code).toBe("CONFIRMATION_REQUIRED");
    expect(envelope.error?.retryable).toBe(false);
    expect(envelope.error?.request_sent).toBe(false);
    expect(envelope.meta.task_id).toBe("t1");
    expect(envelope.warnings).toEqual([]);
  });

  test("字符串 error 被结构化为稳定 code", () => {
    const envelope = normalizeEnvelope({
      schema_version: 1,
      ok: false,
      command: "editor.status",
      error: "boom",
    }, "editor.status");
    expect(envelope.error?.code).toBe("TOOL_FAILURE");
    expect(envelope.error?.message).toBe("boom");
  });

  test("退出码到 envelope error.code 的矩阵", () => {
    const cases: ReadonlyArray<readonly [number, string]> = [
      [ExitCode.Success, "OK"],
      [ExitCode.ToolFailure, "TOOL_FAILURE"],
      [ExitCode.Transport, "TRANSPORT_FAILURE"],
      [ExitCode.Usage, "USAGE_ERROR"],
      [ExitCode.ConfirmationRequired, "CONFIRMATION_REQUIRED"],
      [ExitCode.UserDeclined, "USER_DECLINED"],
      [ExitCode.Interrupted, "INTERRUPTED"],
    ];
    for (const [code, expected] of cases) {
      const envelope = failureEnvelope("tool.call", new CliError(code, "message"));
      expect(envelope.ok).toBe(false);
      expect(envelope.error?.code).toBe(expected);
    }
  });

  test("stdout 只输出 envelope，stderr 不承担机器数据", () => {
    const text = captureStdout(() => {
      process.stdout.write(`${renderJson(normalizeEnvelope({ ok: 1 }, "x"), { pretty: false })}\n`);
    });
    const parsed = JSON.parse(text.trim());
    expect(Object.keys(parsed).sort()).toEqual([...FIELDS].sort());
    expect(text.trim()).not.toContain("\n");
  });

  test("outputFormat 只接受 json，--json 别名归一到 json", () => {
    expect(outputFormat({})).toBe("json");
    expect(outputFormat({ formatText: "json" })).toBe("json");
    expect(() => outputFormat({ formatText: "jsonl" })).toThrow(/--format must be json/);
  });

  test("batch 部分失败标记 partial", () => {
    expect(batchStatus([{ ok: true }, { ok: true }]).partial).toBe(false);
    expect(batchStatus([{ ok: false }, { ok: false }]).partial).toBe(false);
    const partial = batchStatus([{ ok: true }, { ok: false }]);
    expect(partial.partial).toBe(true);
    expect(partial.total).toBe(2);
    expect(partial.succeeded).toBe(1);
    expect(partial.failed).toBe(1);
  });
});
