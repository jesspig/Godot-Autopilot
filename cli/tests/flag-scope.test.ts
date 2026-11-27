import { describe, expect, test } from "bun:test";

import { GLOBAL_SCOPE_FLAGS, parseArgv, usageText } from "../src/args";
import {
  COMMAND_SPECS,
  FLAG_MIGRATIONS,
  findSpec,
  globalFlags,
  parseCommandArgs,
} from "../src/command-spec";
import { CliError, ExitCode } from "../src/errors";

function spec(path: string) {
  const found = findSpec(path);
  if (found === undefined) throw new Error(`missing spec: ${path}`);
  return found;
}

function catchUsage(fn: () => unknown): CliError {
  try {
    fn();
  } catch (error) {
    expect(error).toBeInstanceOf(CliError);
    return error as CliError;
  }
  throw new Error("expected CliError but none was thrown");
}

const REMOVED_FLAGS = ["--direct", "--input", "--async"];

describe("Flag 作用域与已退役 flag 删除", () => {
  test("已退役 flag 不再被全局解析", () => {
    for (const removed of REMOVED_FLAGS) {
      expect(catchUsage(() => parseArgv([removed, "value", "ping"])).code).toBe(ExitCode.Usage);
      expect(GLOBAL_SCOPE_FLAGS.has(removed)).toBe(false);
    }
  });

  test("帮助文本不暴露已退役 flag", () => {
    const help = usageText();
    for (const removed of REMOVED_FLAGS) {
      expect(help).not.toMatch(new RegExp(`(^|\\s)${removed}(?=\\s|$)`));
    }
    expect(usageText()).not.toContain("--force ");
    expect(usageText()).not.toContain("--all ");
    expect(usageText()).not.toContain("--probe ");
  });

  test("命令 spec 不声明已删除 flag", () => {
    for (const entry of COMMAND_SPECS) {
      for (const flag of entry.flags) {
        expect(REMOVED_FLAGS).not.toContain(flag.name);
      }
    }
  });

  test("--json 归一为 --format json 兼容 alias", () => {
    const { opts } = parseArgv(["--json", "ping"]);
    expect(opts.formatText).toBe("json");
  });

  test("--tag/--include 可重复，单值 flag 重复报错", () => {
    const repeated = parseArgv(["--tag", "a", "--tag", "b", "--include", "x", "--include", "y", "ping"]);
    expect(repeated.opts.tag).toEqual(["a", "b"]);
    expect(repeated.opts.include).toEqual(["x", "y"]);
    expect(catchUsage(() => parseArgv(["--port", "1", "--port", "2", "ping"])).message)
      .toContain("specified more than once");
    expect(parseArgv(["--args-file", "-", "batch"]).opts.argsFile).toBe("-");
  });

  test("命令局部 flag 由 spec 校验：tool.call 拒绝已退役 flag", () => {
    for (const removed of REMOVED_FLAGS) {
      const args = [removed, "x"];
      expect(catchUsage(() => parseCommandArgs(spec("tool.call"), ["tool", ...args])).code)
        .toBe(ExitCode.Usage);
    }
  });

  test("多义 flag 已按命令拆分", () => {
    expect(parseCommandArgs(spec("editor.stop"), ["id", "--force-stop"]).options["--force-stop"])
      .toBe(true);
    expect(catchUsage(() => parseCommandArgs(spec("editor.stop"), ["id", "--force"])).code)
      .toBe(ExitCode.Usage);
    expect(parseCommandArgs(spec("editor.list"), ["--probe-port", "9527"]).options["--probe-port"])
      .toBe("9527");
    expect(catchUsage(() => parseCommandArgs(spec("editor.list"), ["--probe", "9527"])).code)
      .toBe(ExitCode.Usage);
    expect(parseCommandArgs(spec("docs.clear"), ["--all-buckets", "--force-clear"]).options["--all-buckets"])
      .toBe(true);
    expect(parseCommandArgs(spec("skill.show"), ["name", "--skill-file", "ref.md"]).options["--skill-file"])
      .toBe("ref.md");
    expect(parseCommandArgs(spec("trace.export"), ["--export-format", "otlp"]).options["--export-format"])
      .toBe("otlp");
  });

  test("全局 flag 清单与迁移表一致", () => {
    const names = globalFlags().map((flag) => flag.name);
    expect(names).toContain("--format");
    expect(names).toContain("--json");
    expect(names).toContain("--tag");
    expect(names).toContain("--include");
    for (const removed of REMOVED_FLAGS) expect(names).not.toContain(removed);
    const migrations = FLAG_MIGRATIONS.map((entry) => entry.from);
    for (const removed of REMOVED_FLAGS) expect(migrations).toContain(removed);
  });
});
