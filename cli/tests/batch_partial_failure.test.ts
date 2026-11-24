import { describe, expect, test } from "bun:test";

import { batchExitCode, batchFailureEntry } from "../src/commands/batch";
import { ExitCode } from "../src/errors";

describe("batch 部分失败语义", () => {
  test("传输异常被记录为失败条目而非抛出", () => {
    const entry = batchFailureEntry(2, "mutating_tool", "IPC request timed out");
    expect(entry.index).toBe(2);
    expect(entry.tool).toBe("mutating_tool");
    expect(entry.ok).toBe(false);
    expect(entry.error).toContain("timed out");
    expect(entry.display).toBeDefined();
  });

  test("全部成功返回 Success，任一失败返回 ToolFailure", () => {
    expect(batchExitCode([{ ok: true }, { ok: true }])).toBe(ExitCode.Success);
    expect(batchExitCode([{ ok: true }, { ok: false }])).toBe(ExitCode.ToolFailure);
    expect(batchExitCode([])).toBe(ExitCode.Success);
  });

  test("汇总退出码优先保留 Usage 与 Transport", () => {
    expect(batchExitCode([{ ok: false }], [ExitCode.ToolFailure, ExitCode.Transport])).toBe(ExitCode.Transport);
    expect(batchExitCode([{ ok: false }], [ExitCode.Transport, ExitCode.Usage])).toBe(ExitCode.Usage);
    expect(batchExitCode([{ ok: true }, { ok: false }], [ExitCode.Usage, ExitCode.ToolFailure])).toBe(ExitCode.Usage);
  });
});
