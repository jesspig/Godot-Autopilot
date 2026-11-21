import fs from "node:fs";
import path from "node:path";

import { afterEach, describe, expect, test } from "bun:test";

import { extractZip } from "../src/archive";
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
