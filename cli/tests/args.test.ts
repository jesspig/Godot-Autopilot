import { describe, expect, test } from "bun:test";
import { parseArgv, usageText } from "../src/args";
import { CliError, ExitCode } from "../src/errors";

function catchUsage(argv: string[]): CliError {
  try {
    parseArgv(argv);
  } catch (e) {
    expect(e).toBeInstanceOf(CliError);
    return e as CliError;
  }
  throw new Error("expected a CliError but none was thrown");
}

describe("全局选项", () => {
  test("--quiet 不再被识别，报用法错误", () => {
    const err = catchUsage(["--quiet", "ping"]);
    expect(err.code).toBe(ExitCode.Usage);
    expect(err.message).toContain("unknown option: --quiet");
  });

  test("帮助文本不含 --quiet", () => {
    expect(usageText()).not.toContain("--quiet");
  });

  test("--pretty 与 --raw 仍被识别", () => {
    const { opts } = parseArgv(["--pretty", "--raw", "ping"]);
    expect(opts.pretty).toBe(true);
    expect(opts.raw).toBe(true);
  });

  test("寻址三选项 --id/--port/--project 仍被识别", () => {
    const { opts } = parseArgv(["--id", "p1", "ping"]);
    expect(opts.id).toBe("p1");
    const withPort = parseArgv(["--port", "9527", "ping"]);
    expect(withPort.opts.portText).toBe("9527");
    const withProject = parseArgv(["--project", "D:/proj", "ping"]);
    expect(withProject.opts.project).toBe("D:/proj");
  });
});
