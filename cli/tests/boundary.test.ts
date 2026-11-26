import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { describe, expect, test } from "bun:test";

import { parseArgv, usageText } from "../src/args";
import { batchExitCode, batchFailureEntry } from "../src/commands/batch";
import { CliError, ExitCode, InterruptedError } from "../src/errors";
import { loadToolArguments } from "../src/payload";

describe("边界：退出码、stdin 与 batch", () => {
  test("退出码矩阵完整覆盖 0/1/2/3/4/5/130", () => {
    expect(ExitCode.Success).toBe(0);
    expect(ExitCode.ToolFailure).toBe(1);
    expect(ExitCode.Transport).toBe(2);
    expect(ExitCode.Usage).toBe(3);
    expect(ExitCode.ConfirmationRequired).toBe(4);
    expect(ExitCode.UserDeclined).toBe(5);
    expect(new InterruptedError().code).toBe(130);
    expect(usageText()).toContain("130  interrupted");
  });

  test("--args-file - 被解析为 stdin 标记", () => {
    const { opts } = parseArgv(["--args-file", "-", "batch"]);
    expect(opts.argsFile).toBe("-");
    expect(parseArgv(["--args-file=-", "batch"]).opts.argsFile).toBe("-");
  });

  test("文件型 --args-file 仍加载对象参数，非法对象报错", () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-boundary-"));
    try {
      const file = path.join(dir, "args.json");
      fs.writeFileSync(file, JSON.stringify({ a: 1 }));
      expect(loadToolArguments({ argsFile: file })).toEqual({ a: 1 });
      fs.writeFileSync(file, "[1,2]");
      expect(() => loadToolArguments({ argsFile: file })).toThrow(/JSON object/);
      expect(() => loadToolArguments({ argsFile: path.join(dir, "missing.json") })).toThrow(/cannot read/);
    } finally {
      fs.rmSync(dir, { recursive: true, force: true });
    }
  });

  test("batch 失败条目保留索引与错误，且不吞掉结果", () => {
    const entry = batchFailureEntry(2, "mutating", "CONFIRMATION_REQUIRED");
    expect(entry.index).toBe(2);
    expect(entry.ok).toBe(false);
    expect(entry.error).toContain("CONFIRMATION_REQUIRED");
  });

  test("batch 汇总退出码优先级：Usage > Transport > Interrupted > Declined > Confirm > Tool", () => {
    expect(batchExitCode([{ ok: true }, { ok: false }])).toBe(ExitCode.ToolFailure);
    expect(batchExitCode([{ ok: true }], [ExitCode.ConfirmationRequired])).toBe(ExitCode.ConfirmationRequired);
    expect(batchExitCode([{ ok: true }], [ExitCode.UserDeclined])).toBe(ExitCode.UserDeclined);
    expect(batchExitCode([{ ok: true }], [ExitCode.Interrupted])).toBe(ExitCode.Interrupted);
    expect(batchExitCode([{ ok: true }], [ExitCode.Transport, ExitCode.ConfirmationRequired])).toBe(ExitCode.Transport);
    expect(batchExitCode([{ ok: true }], [ExitCode.Usage, ExitCode.Transport])).toBe(ExitCode.Usage);
    expect(batchExitCode([{ ok: true }])).toBe(ExitCode.Success);
  });

  test("用法错误仍是 CliError 且 code=3", () => {
    expect(() => parseArgv(["--unknown"])).toThrow(CliError);
    try {
      parseArgv(["--unknown"]);
    } catch (error) {
      expect((error as CliError).code).toBe(ExitCode.Usage);
    }
  });
});
