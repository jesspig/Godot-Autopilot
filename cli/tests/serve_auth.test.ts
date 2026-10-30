import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { checkBearer, controlTokenPath, ensureControlToken, extractBearer } from "../src/serve/auth";
import { removeDirWithRetry } from "./support/cleanup";

let tmp: string;
let prevHome: string | undefined;
const posixTest = process.platform === "win32" ? test.skip : test;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-serve-auth-"));
  prevHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
});

afterEach(async () => {
  if (prevHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = prevHome;
  await removeDirWithRetry(tmp);
});

describe("控制面令牌文件", () => {
  test("首次启动生成 64 位十六进制令牌", () => {
    const created = ensureControlToken();
    expect(created.created).toBe(true);
    expect(created.path).toBe(path.join(tmp, ".godot-autopilot", "control.token"));
    expect(created.token).toMatch(/^[0-9a-f]{64}$/);
    expect(fs.readFileSync(created.path, "utf8")).toBe(created.token);
  });

  test("令牌路径受 GODOT_AUTOPILOT_HOME 约束", () => {
    expect(controlTokenPath()).toBe(path.join(tmp, ".godot-autopilot", "control.token"));
  });

  test("已有非空令牌时原样复用", () => {
    const first = ensureControlToken();
    const second = ensureControlToken();
    expect(second.created).toBe(false);
    expect(second.token).toBe(first.token);
    expect(fs.readFileSync(first.path, "utf8")).toBe(first.token);
  });

  test("已有文件带换行时复用去空白后的令牌", () => {
    const tokenPath = controlTokenPath();
    fs.mkdirSync(path.dirname(tokenPath), { recursive: true });
    const token = "ab".repeat(32);
    fs.writeFileSync(tokenPath, `${token}\n`);
    const loaded = ensureControlToken();
    expect(loaded.created).toBe(false);
    expect(loaded.token).toBe(token);
  });

  test("已有空文件时重新生成", () => {
    const tokenPath = controlTokenPath();
    fs.mkdirSync(path.dirname(tokenPath), { recursive: true });
    fs.writeFileSync(tokenPath, "  \n");
    const created = ensureControlToken();
    expect(created.created).toBe(true);
    expect(created.token).toMatch(/^[0-9a-f]{64}$/);
    expect(fs.readFileSync(tokenPath, "utf8")).toBe(created.token);
  });

  posixTest("新建令牌文件权限为 0600（Windows 跳过）", () => {
    const created = ensureControlToken();
    const mode = fs.statSync(created.path).mode & 0o777;
    expect(mode).toBe(0o600);
  });

  posixTest("空文件重建后权限仍为 0600（Windows 跳过）", () => {
    const tokenPath = controlTokenPath();
    fs.mkdirSync(path.dirname(tokenPath), { recursive: true });
    fs.writeFileSync(tokenPath, "", { mode: 0o644 });
    const created = ensureControlToken();
    const mode = fs.statSync(created.path).mode & 0o777;
    expect(mode).toBe(0o600);
    expect(created.created).toBe(true);
  });
});

describe("Bearer 校验", () => {
  const token = "a".repeat(64);

  test("正确令牌通过", () => {
    expect(checkBearer(`Bearer ${token}`, token)).toBe(true);
  });

  test("scheme 大小写不敏感", () => {
    expect(checkBearer(`bearer ${token}`, token)).toBe(true);
    expect(checkBearer(`BEARER ${token}`, token)).toBe(true);
  });

  test("两侧空白被忽略", () => {
    expect(checkBearer(`  Bearer ${token}  `, token)).toBe(true);
  });

  test("缺失、非 Bearer 与原始令牌均不通过", () => {
    expect(checkBearer(null, token)).toBe(false);
    expect(checkBearer("", token)).toBe(false);
    expect(checkBearer(token, token)).toBe(false);
    expect(checkBearer(`Basic ${token}`, token)).toBe(false);
  });

  test("同长度错误令牌不通过", () => {
    expect(checkBearer(`Bearer ${"b".repeat(64)}`, token)).toBe(false);
    expect(checkBearer(`Bearer ${"a".repeat(63)}`, token)).toBe(false);
  });

  test("长度不等不抛异常", () => {
    expect(() => checkBearer("Bearer short", token)).not.toThrow();
    expect(checkBearer("Bearer short", token)).toBe(false);
    expect(checkBearer(`Bearer ${token}extra`, token)).toBe(false);
  });

  test("extractBearer 只接受 Bearer 形式", () => {
    expect(extractBearer(`Bearer ${token}`)).toBe(token);
    expect(extractBearer("Token abc")).toBeNull();
    expect(extractBearer(null)).toBeNull();
  });
});
