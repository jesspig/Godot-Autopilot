import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";

import { usageText, type GlobalOptions } from "../src/args";
import { run as runInstall } from "../src/commands/install";

let tmp: string;
let previousHome: string | undefined;

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmp = fs.mkdtempSync(path.join(base, "gda-install-"));
  previousHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
});

afterEach(() => {
  if (previousHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = previousHome;
  fs.rmSync(tmp, { recursive: true, force: true });
});

function captureOutput(): { stdout(): string; restore(): void } {
  const orig = process.stdout.write;
  let out = "";
  process.stdout.write = ((chunk: unknown) => {
    out += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  return {
    stdout: () => out,
    restore: () => {
      process.stdout.write = orig;
    },
  };
}

async function runCaptured(positional: string[], opts: GlobalOptions) {
  const cap = captureOutput();
  try {
    const code = await runInstall(positional, opts);
    return { code, stdout: cap.stdout() };
  } finally {
    cap.restore();
  }
}

describe("install skills", () => {
  test("builtin 源写入完整 skills 树", async () => {
    const target = path.join(tmp, "skills");
    const code = await runInstall(["skills"], {
      sourceText: "builtin",
      pathText: target,
      force: true,
    });
    expect(code).toBe(0);
    const dirs = fs.readdirSync(target, { withFileTypes: true }).filter((entry) => entry.isDirectory());
    expect(dirs.length).toBe(10);
    expect(fs.existsSync(path.join(target, "godot-autopilot", "SKILL.md"))).toBe(true);
    expect(fs.existsSync(path.join(target, "godot-autopilot-tools", "SKILL.md"))).toBe(true);
    expect(fs.existsSync(path.join(target, "godot-autopilot-setup", "SKILL.md"))).toBe(true);
  });

  test("zh 内置源与 --offline 写入中文 10 册", async () => {
    const target = path.join(tmp, "skills-zh");
    const code = await runInstall(["skills"], {
      zh: true,
      offline: true,
      pathText: target,
      force: true,
    });
    expect(code).toBe(0);
    const dirs = fs.readdirSync(target, { withFileTypes: true }).filter((entry) => entry.isDirectory());
    expect(dirs.length).toBe(10);
    expect(fs.existsSync(path.join(target, "godot-autopilot-setup", "SKILL.md"))).toBe(true);
    expect(fs.readFileSync(path.join(target, "godot-autopilot-setup", "SKILL.md"), "utf8")).toContain(
      "Godot Autopilot 安装与连接",
    );
  });

  test("已存在且内容不同时未传 force 报错", async () => {
    const target = path.join(tmp, "skills");
    await runInstall(["skills"], { sourceText: "builtin", pathText: target, force: true });
    const skillFile = path.join(target, "godot-autopilot", "SKILL.md");
    fs.writeFileSync(skillFile, "changed", "utf8");
    let caught: unknown;
    try {
      await runInstall(["skills"], { sourceText: "builtin", pathText: target, force: false });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeDefined();
  });

  test("未加 --prune 只报告 stale，不删除退役目录", async () => {
    const target = path.join(tmp, "skills");
    const result = await runCaptured(["skills"], {
      sourceText: "builtin",
      pathText: target,
      force: true,
    });
    const payload = JSON.parse(result.stdout) as { stale: string[]; removed: string[] };
    expect(result.code).toBe(0);
    expect(payload.stale.length).toBe(9);
    expect(payload.removed).toEqual([]);
    for (const name of payload.stale) {
      expect(fs.existsSync(path.join(target, name, "SKILL.md"))).toBe(true);
    }
  });

  test("--prune 删除退役目录并保留自建目录", async () => {
    const target = path.join(tmp, "skills");
    fs.mkdirSync(path.join(target, "my-notes"), { recursive: true });
    const result = await runCaptured(["skills"], {
      sourceText: "builtin",
      pathText: target,
      force: true,
      prune: true,
    });
    const payload = JSON.parse(result.stdout) as { stale: string[]; removed: string[] };
    expect(result.code).toBe(0);
    expect(payload.removed.length).toBe(9);
    expect(fs.existsSync(path.join(target, "my-notes"))).toBe(true);
    expect(fs.existsSync(path.join(target, "godot-autopilot", "SKILL.md"))).toBe(true);
    for (const name of payload.removed) {
      expect(fs.existsSync(path.join(target, name))).toBe(false);
    }
    const dirs = fs.readdirSync(target, { withFileTypes: true }).filter((entry) => entry.isDirectory());
    expect(dirs.length).toBe(2);
  });

  test("目标不存在时 --prune 正常创建并报告删除项", async () => {
    const target = path.join(tmp, "new-skills");
    const result = await runCaptured(["skills"], {
      sourceText: "builtin",
      pathText: target,
      force: true,
      prune: true,
    });
    const payload = JSON.parse(result.stdout) as { stale: string[]; removed: string[] };
    expect(result.code).toBe(0);
    expect(payload.stale.length).toBe(9);
    expect(payload.removed.length).toBe(9);
    expect(fs.existsSync(path.join(target, "godot-autopilot", "SKILL.md"))).toBe(true);
  });

  test("--help 列出 --prune", () => {
    expect(usageText()).toContain("--prune");
  });
});
