import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import {
  collectTaskStorePoisoning,
  detectTaskStorePoisoning,
  gracefulRestartSuggestion,
  gracefulStopHint,
  stopFailureHint,
  TASK_STORE_POISON_SIGNATURE,
} from "../src/commands/stop-diagnosis";
import type { InstanceEntry } from "../src/registry";

const BASE_GRACEFUL_STOP_HINT = "the process is still running; retry with --force";
const BASE_RESTART_SUGGESTION =
  "retry with --force, or stop it manually before running gda restart again";
const FORCE_STOP_HINT = "the process survived a force stop";

let tmp: string;
let prevHome: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-stop-diagnosis-"));
  prevHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
});

afterEach(() => {
  if (prevHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = prevHome;
  fs.rmSync(tmp, { recursive: true, force: true });
});

function makeEntry(project: string): InstanceEntry {
  return {
    id: "p1234",
    project,
    port: 9530,
    pid: 1234,
    startedAt: new Date().toISOString(),
    headless: false,
    godot: "godot",
    log: path.join(tmp, "editor.log"),
  };
}

function seedProjectLog(project: string, logText: string): void {
  const projectDir = path.join(tmp, ".godot-autopilot", "projects", "demo-abc12345");
  const instancesDir = path.join(projectDir, "instances");
  const logsDir = path.join(projectDir, "logs");
  fs.mkdirSync(instancesDir, { recursive: true });
  fs.mkdirSync(logsDir, { recursive: true });
  fs.writeFileSync(
    path.join(instancesDir, "gda-demo-1234.json"),
    JSON.stringify({ project_path: project, pid: 1234 }),
  );
  fs.writeFileSync(path.join(logsDir, "gda-20261118_010101.log"), logText);
}

describe("任务存储中毒签名判定", () => {
  test("命中：日志含 SDK 持久化失败签名", () => {
    const line = `[1] [error] [tools] MCP SDK: ${TASK_STORE_POISON_SIGNATURE} C:/home/.godot-autopilot/tasks/tasks-1.jsonl`;
    expect(detectTaskStorePoisoning(line)).toBe(true);
  });

  test("命中：CreateTask 抛出的持久化失败文本", () => {
    expect(detectTaskStorePoisoning("task store: failed to persist task 7a2a4aaa")).toBe(true);
  });

  test("未命中：无关日志与空文本", () => {
    expect(detectTaskStorePoisoning("[1] [info] [transport] MCP server started")).toBe(false);
    expect(detectTaskStorePoisoning("")).toBe(false);
  });
});

describe("graceful 失败提示增补", () => {
  test("未命中时 stop 提示与基线一致", () => {
    expect(gracefulStopHint(false)).toBe(BASE_GRACEFUL_STOP_HINT);
    expect(stopFailureHint("graceful", false)).toBe(BASE_GRACEFUL_STOP_HINT);
  });

  test("命中时 stop 提示含中毒诊断与 force 丢编辑风险", () => {
    const hint = gracefulStopHint(true);
    expect(hint).toContain("poisoned");
    expect(hint).toContain("--force");
    expect(hint).toContain("discards unsaved scene edits");
    expect(stopFailureHint("graceful", true)).toBe(hint);
  });

  test("未命中时 restart 建议与基线一致", () => {
    expect(gracefulRestartSuggestion(false)).toBe(BASE_RESTART_SUGGESTION);
  });

  test("命中时 restart 建议含中毒诊断与 force 丢编辑风险", () => {
    const suggestion = gracefulRestartSuggestion(true);
    expect(suggestion).toContain("poisoned");
    expect(suggestion).toContain("--force");
    expect(suggestion).toContain("discards unsaved scene edits");
    expect(suggestion).toContain("stop it manually");
  });

  test("force 语义不变：无论是否命中都保留 force 失败提示", () => {
    expect(stopFailureHint("force", false)).toBe(FORCE_STOP_HINT);
    expect(stopFailureHint("force", true)).toBe(FORCE_STOP_HINT);
  });
});

describe("中毒证据采集（集中根插件日志）", () => {
  test("命中：项目日志含签名时判定中毒", () => {
    const project = path.join(tmp, "proj");
    seedProjectLog(project, `MCP SDK: ${TASK_STORE_POISON_SIGNATURE} tasks-1.jsonl\n`);
    expect(collectTaskStorePoisoning(makeEntry(project))).toBe(true);
  });

  test("未命中：项目日志无签名时不诊断", () => {
    const project = path.join(tmp, "proj");
    seedProjectLog(project, "[1] [info] [transport] MCP server started\n");
    expect(collectTaskStorePoisoning(makeEntry(project))).toBe(false);
  });

  test("未命中：无集中根项目目录时不伪造诊断", () => {
    const project = path.join(tmp, "missing-proj");
    expect(collectTaskStorePoisoning(makeEntry(project))).toBe(false);
  });

  test("未命中：项目路径不匹配时不串读其他项目日志", () => {
    const project = path.join(tmp, "proj");
    seedProjectLog(path.join(tmp, "other-proj"), `MCP SDK: ${TASK_STORE_POISON_SIGNATURE} x\n`);
    expect(collectTaskStorePoisoning(makeEntry(project))).toBe(false);
  });
});