import { describe, expect, test } from "bun:test";

import type { GlobalOptions } from "../src/args";
import { run as runSkills } from "../src/commands/skills";
import { CliError, ExitCode } from "../src/errors";

function captureOutput(): { stdout(): string; stderr(): string; restore(): void } {
  const origOut = process.stdout.write;
  const origErr = process.stderr.write;
  let outBuf = "";
  let errBuf = "";
  process.stdout.write = ((chunk: unknown) => {
    outBuf += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  process.stderr.write = ((chunk: unknown) => {
    errBuf += String(chunk);
    return true;
  }) as typeof process.stderr.write;
  return {
    stdout: () => outBuf,
    stderr: () => errBuf,
    restore: () => {
      process.stdout.write = origOut;
      process.stderr.write = origErr;
    },
  };
}

async function runCaptured(positional: string[], opts: GlobalOptions) {
  const cap = captureOutput();
  try {
    const code = await runSkills(positional, opts);
    return { code, stdout: cap.stdout(), stderr: cap.stderr() };
  } finally {
    cap.restore();
  }
}

describe("skills", () => {
  test("list 离线列出 9 册并带路径与文件数", async () => {
    const result = await runCaptured(["list"], {});
    expect(result.code).toBe(ExitCode.Success);
    const books = JSON.parse(result.stdout) as Array<{ name: string; path: string; files: number }>;
    expect(Array.isArray(books)).toBe(true);
    expect(books.length).toBe(9);
    for (const book of books) {
      expect(typeof book.name).toBe("string");
      expect(book.name.startsWith("godot-autopilot")).toBe(true);
      expect(typeof book.path).toBe("string");
      expect(book.files).toBeGreaterThan(0);
    }
  });

  test("show 输出 SKILL.md 正文", async () => {
    const result = await runCaptured(["show", "godot-autopilot-tools"], {});
    expect(result.code).toBe(ExitCode.Success);
    expect(result.stdout).toContain("# Godot Autopilot Tools");
  });

  test("show --file 读取引用文件", async () => {
    const result = await runCaptured(["show", "godot-autopilot-tools"], {
      file: "references/cli-access.md",
    });
    expect(result.code).toBe(ExitCode.Success);
    expect(result.stdout).toContain("# The GDA CLI");
  });

  test("未知册名报用法错误", async () => {
    let caught: unknown;
    try {
      await runCaptured(["show", "no-such-book"], {});
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Usage);
  });

  test("--file 逃出技能目录被拒", async () => {
    let caught: unknown;
    try {
      await runCaptured(["show", "godot-autopilot-tools"], { file: "../../README.md" });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Usage);
  });

  test("缺子命令报用法错误", async () => {
    let caught: unknown;
    try {
      await runCaptured([], {});
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Usage);
  });
});