import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { afterEach, describe, expect, test } from "bun:test";

import { prepareOwnedFile } from "./owned-files";

const tempDirs: string[] = [];

function makeTempDir(prefix: string): string {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), prefix));
  tempDirs.push(dir);
  return dir;
}

afterEach(() => {
  for (const dir of tempDirs.splice(0)) {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});

function writeText(file: string, content: string): void {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, content, "utf8");
}

describe("prepareOwnedFile", () => {
  test("removes a POSIX symlink leaf without touching its target", () => {
    if (process.platform === "win32") return;
    const root = makeTempDir("gda-owned-symlink-leaf-");
    const staging = path.join(root, "staging");
    const outside = path.join(root, "outside.txt");
    writeText(outside, "outside-data");
    fs.mkdirSync(staging, { recursive: true });
    fs.symlinkSync(outside, path.join(staging, "owned.cs"), "file");

    const target = prepareOwnedFile(staging, "owned.cs");
    writeText(target, "new-data");

    expect(fs.lstatSync(target).isFile()).toBe(true);
    expect(fs.readFileSync(target, "utf8")).toBe("new-data");
    expect(fs.readFileSync(outside, "utf8")).toBe("outside-data");
  });

  test("rejects a POSIX symlink parent directory", () => {
    if (process.platform === "win32") return;
    const root = makeTempDir("gda-owned-symlink-parent-");
    const staging = path.join(root, "staging");
    const outside = path.join(root, "outside");
    writeText(path.join(outside, "secret.txt"), "outside-data");
    fs.mkdirSync(staging, { recursive: true });
    fs.symlinkSync(outside, path.join(staging, "linked"), "dir");

    expect(() => prepareOwnedFile(staging, "linked/owned.cs")).toThrow("unsafe parent path");
    expect(fs.readFileSync(path.join(outside, "secret.txt"), "utf8")).toBe("outside-data");
    expect(fs.existsSync(path.join(outside, "owned.cs"))).toBe(false);
  });

  test("removes a Windows junction leaf without touching its target", () => {
    if (process.platform !== "win32") return;
    const root = makeTempDir("gda-owned-junction-leaf-");
    const staging = path.join(root, "staging");
    const outside = path.join(root, "outside");
    writeText(path.join(outside, "secret.txt"), "outside-data");
    fs.mkdirSync(staging, { recursive: true });
    fs.symlinkSync(outside, path.join(staging, "owned.dll"), "junction");

    const target = prepareOwnedFile(staging, "owned.dll");
    writeText(target, "new-data");

    expect(fs.lstatSync(target).isFile()).toBe(true);
    expect(fs.readFileSync(target, "utf8")).toBe("new-data");
    expect(fs.readFileSync(path.join(outside, "secret.txt"), "utf8")).toBe("outside-data");
    expect(fs.existsSync(path.join(outside, "owned.dll"))).toBe(false);
  });

  test("rejects a Windows junction parent directory", () => {
    if (process.platform !== "win32") return;
    const root = makeTempDir("gda-owned-junction-parent-");
    const staging = path.join(root, "staging");
    const outside = path.join(root, "outside");
    writeText(path.join(outside, "secret.txt"), "outside-data");
    fs.mkdirSync(staging, { recursive: true });
    fs.symlinkSync(outside, path.join(staging, "linked"), "junction");

    expect(() => prepareOwnedFile(staging, "linked/owned.cs")).toThrow("unsafe parent path");
    expect(fs.readFileSync(path.join(outside, "secret.txt"), "utf8")).toBe("outside-data");
    expect(fs.existsSync(path.join(outside, "owned.cs"))).toBe(false);
  });

  test("removes a non-file staging leaf before writing", () => {
    const root = makeTempDir("gda-owned-directory-leaf-");
    const staging = path.join(root, "staging");
    writeText(path.join(staging, "owned.cs", "stale.txt"), "stale-data");

    const target = prepareOwnedFile(staging, "owned.cs");
    writeText(target, "new-data");

    expect(fs.lstatSync(target).isFile()).toBe(true);
    expect(fs.readFileSync(target, "utf8")).toBe("new-data");
  });
});
