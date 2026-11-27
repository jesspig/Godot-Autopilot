import { describe, expect, test } from "bun:test";

import type { GlobalOptions } from "../src/args";
import { run as runSkills } from "../src/commands/skills";
import { CliError, ExitCode } from "../src/errors";
import { builtinRetiredSkills, normalizeRetiredSkills } from "../src/skills";

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

describe("retired skills", () => {
  test("内置退役清单为 9 个领域册且不含入口", () => {
    const retired = builtinRetiredSkills();
    expect(retired.length).toBe(9);
    expect(retired).not.toContain("godot-autopilot");
    expect(retired).toContain("godot-autopilot-tools");
    expect(retired).toEqual([...retired].sort((a, b) => a.localeCompare(b)));
  });

  test("旧 asset 缺少 retired 或含非法名时安全归一", () => {
    expect(normalizeRetiredSkills(undefined)).toEqual([]);
    expect(normalizeRetiredSkills("x")).toEqual([]);
    expect(normalizeRetiredSkills(["b", "a", "a", "../x", 1])).toEqual(["a", "b"]);
  });
});

describe("skills", () => {
  test("list 离线列出 1 张入口技能卡并带路径与文件数", async () => {
    const result = await runCaptured(["list"], {});
    expect(result.code).toBe(ExitCode.Success);
    const books = (JSON.parse(result.stdout) as { data: Array<{ name: string; path: string; files: number }> }).data;
    expect(Array.isArray(books)).toBe(true);
    expect(books.length).toBe(1);
    for (const book of books) {
      expect(typeof book.name).toBe("string");
      expect(book.name.startsWith("godot-autopilot")).toBe(true);
      expect(typeof book.path).toBe("string");
      expect(book.files).toBeGreaterThan(0);
    }
  });

  test("--zh 列出中文入口卡", async () => {
    const result = await runCaptured(["list"], { zh: true });
    expect(result.code).toBe(ExitCode.Success);
    const books = (JSON.parse(result.stdout) as { data: Array<{ name: string; path: string; files: number }> }).data;
    expect(books.length).toBe(1);
    expect(books[0]?.name).toBe("godot-autopilot");
  });

  test("show 输出 SKILL.md 正文", async () => {
    const result = await runCaptured(["show", "godot-autopilot"], {});
    expect(result.code).toBe(ExitCode.Success);
    expect(result.stdout).toContain("# Godot Autopilot");
  });

  test("show --file 读取 references 文件", async () => {
    const result = await runCaptured(["show", "godot-autopilot"], {
      file: "references/tools.md",
    });
    expect(result.code).toBe(ExitCode.Success);
    expect(result.stdout).toContain("# Tools");
  });

  test("--zh show 输出中文正文", async () => {
    const result = await runCaptured(["show", "godot-autopilot"], { zh: true });
    expect(result.code).toBe(ExitCode.Success);
    expect(result.stdout).toContain("# Godot Autopilot");
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
      await runCaptured(["show", "godot-autopilot"], { file: "../../README.md" });
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