import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import path from "node:path";

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
});
