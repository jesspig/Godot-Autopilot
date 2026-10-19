import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { resolveGodotBin } from "../src/process";

let savedPath: string | undefined;
let savedGodotPath: string | undefined;
let tempDirs: string[] = [];

beforeEach(() => {
  savedPath = process.env.PATH;
  savedGodotPath = process.env.GODOT_PATH;
  process.env.PATH = "";
  delete process.env.GODOT_PATH;
});

afterEach(() => {
  if (savedPath === undefined) delete process.env.PATH;
  else process.env.PATH = savedPath;
  if (savedGodotPath === undefined) delete process.env.GODOT_PATH;
  else process.env.GODOT_PATH = savedGodotPath;
  for (const dir of tempDirs) fs.rmSync(dir, { recursive: true, force: true });
  tempDirs = [];
});

describe("resolveGodotBin", () => {
  test("ignores GODOT_PATH which is reserved for the L2 runner", () => {
    process.env.GODOT_PATH = path.join(os.tmpdir(), "gda-missing-godot", "godot.exe");
    expect(() => resolveGodotBin({})).toThrow(/--godot/);
  });

  test("explicit --godot wins without touching the environment", () => {
    process.env.GODOT_PATH = path.join(os.tmpdir(), "gda-missing-godot", "godot.exe");
    const r = resolveGodotBin({ explicit: "custom-godot" });
    expect(r.source).toBe("explicit");
    expect(r.bin.endsWith("custom-godot")).toBe(true);
  });

  test("falls back to PATH lookup", () => {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "gda-godot-path-"));
    tempDirs.push(dir);
    const name = process.platform === "win32" ? "godot.exe" : "godot";
    fs.writeFileSync(path.join(dir, name), "");
    process.env.PATH = dir;
    const r = resolveGodotBin({});
    expect(r.source).toBe("path");
    expect(r.bin.endsWith(name)).toBe(true);
  });
});
