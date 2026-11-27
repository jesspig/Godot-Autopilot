import fs from "node:fs";
import path from "node:path";

import { afterEach, describe, expect, test } from "bun:test";

import { applyManagedTree, extractZip } from "../src/archive";
import { CliError, ExitCode } from "../src/errors";

const tempRoots: string[] = [];

afterEach(() => {
  while (tempRoots.length > 0) {
    fs.rmSync(tempRoots.pop()!, { recursive: true, force: true });
  }
});

function makeTemp(): string {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  const dir = fs.mkdtempSync(path.join(base, "gda-archive-"));
  tempRoots.push(dir);
  return dir;
}

function makeArchive(root: string): string {
  const src = path.join(root, "src");
  fs.mkdirSync(path.join(src, "inside"), { recursive: true });
  fs.writeFileSync(path.join(src, "inside", "safe.txt"), "hello", "utf8");
  const zip = path.join(root, "safe.zip");
  const tar = Bun.spawnSync(["tar", "-cf", zip, "inside"], {
    cwd: src,
    stdout: "pipe",
    stderr: "pipe",
  });
  expect(tar.exitCode).toBe(0);
  return zip;
}

describe("extractZip zip-slip 纵深防御", () => {
  test("正常压缩包解压后内容位于目标目录内", () => {
    const root = makeTemp();
    const zip = makeArchive(root);
    const dest = path.join(root, "dest");
    extractZip(zip, dest);
    expect(fs.readFileSync(path.join(dest, "inside", "safe.txt"), "utf8")).toBe("hello");
  });

  test("解压项经符号链接越出目标目录时拒绝", () => {
    const root = makeTemp();
    const zip = makeArchive(root);
    const dest = path.join(root, "dest");
    const outside = path.join(root, "outside");
    fs.mkdirSync(dest, { recursive: true });
    fs.mkdirSync(outside, { recursive: true });
    fs.symlinkSync(outside, path.join(dest, "escape"), "junction");

    let caught: unknown;
    try {
      extractZip(zip, dest);
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Transport);
    expect((caught as CliError).message).toContain("unsafe path in archive");
  });
});

describe("applyManagedTree 受管文件同步", () => {
  test("dry-run 只返回计划不写盘", () => {
    const root = makeTemp();
    const target = path.join(root, "skills");
    fs.mkdirSync(path.join(target, "godot-autopilot"), { recursive: true });
    fs.writeFileSync(path.join(target, "godot-autopilot", "old.md"), "old", "utf8");
    const result = applyManagedTree(
      target,
      {
        writes: { "godot-autopilot/new.md": "new" },
        deletes: ["godot-autopilot/old.md"],
      },
      { dryRun: true },
    );
    expect(result.applied).toBe(false);
    expect(result.written).toEqual(["godot-autopilot/new.md"]);
    expect(result.deleted).toEqual(["godot-autopilot/old.md"]);
    expect(fs.existsSync(path.join(target, "godot-autopilot", "new.md"))).toBe(false);
    expect(fs.readFileSync(path.join(target, "godot-autopilot", "old.md"), "utf8")).toBe("old");
  });

  test("越界删除被拒绝", () => {
    const root = makeTemp();
    const outside = path.join(root, "outside.md");
    fs.writeFileSync(outside, "keep", "utf8");
    let caught: unknown;
    try {
      applyManagedTree(root, { writes: {}, deletes: ["../outside.md"] });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Transport);
    expect(fs.readFileSync(outside, "utf8")).toBe("keep");
  });

  test("同内容未加 force 可跳过，异内容未加 force 拒绝且不写盘", () => {
    const root = makeTemp();
    const target = path.join(root, "skills");
    applyManagedTree(target, { writes: { "godot-autopilot/SKILL.md": "v1" }, deletes: [] });
    applyManagedTree(target, { writes: { "godot-autopilot/SKILL.md": "v1" }, deletes: [] });
    expect(fs.readFileSync(path.join(target, "godot-autopilot", "SKILL.md"), "utf8")).toBe("v1");
    let caught: unknown;
    try {
      applyManagedTree(target, { writes: { "godot-autopilot/SKILL.md": "v2" }, deletes: [] });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).suggestion).toContain("--force");
    expect(fs.readFileSync(path.join(target, "godot-autopilot", "SKILL.md"), "utf8")).toBe("v1");
    applyManagedTree(target, { writes: { "godot-autopilot/SKILL.md": "v2" }, deletes: [] }, { force: true });
    expect(fs.readFileSync(path.join(target, "godot-autopilot", "SKILL.md"), "utf8")).toBe("v2");
  });
});
