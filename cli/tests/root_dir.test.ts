import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { gdaHome, gdaRoot } from "../src/platform";
import { loadRegistry, saveRegistry, stateDir, type InstanceEntry } from "../src/registry";
import { removeDirWithRetry } from "./support/cleanup";

let tmp: string;
let prevHome: string | undefined;
let prevCliHome: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-root-test-"));
  prevHome = process.env.GODOT_AUTOPILOT_HOME;
  prevCliHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  delete process.env.GODOT_AUTOPILOT_CLI_HOME;
});

afterEach(async () => {
  if (prevHome === undefined) {
    delete process.env.GODOT_AUTOPILOT_HOME;
  } else {
    process.env.GODOT_AUTOPILOT_HOME = prevHome;
  }
  if (prevCliHome === undefined) {
    delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  } else {
    process.env.GODOT_AUTOPILOT_CLI_HOME = prevCliHome;
  }
  await removeDirWithRetry(tmp);
});

function makeEntry(overrides: Partial<InstanceEntry> = {}): InstanceEntry {
  return {
    id: "p1",
    project: tmp,
    port: 9527,
    pid: process.pid,
    startedAt: "2026-10-20T00:00:00.000Z",
    headless: false,
    godot: "godot",
    log: path.join(tmp, "editor.log"),
    ...overrides,
  };
}

describe("gdaHome", () => {
  test("环境变量覆写生效", () => {
    process.env.GODOT_AUTOPILOT_HOME = tmp;
    expect(gdaHome()).toBe(tmp);
    expect(gdaRoot()).toBe(path.join(tmp, ".godot-autopilot"));
  });

  test("覆写为相对路径时解析为绝对路径", () => {
    process.env.GODOT_AUTOPILOT_HOME = "relative-root";
    const home = gdaHome();
    expect(path.isAbsolute(home)).toBe(true);
    expect(home).toBe(path.resolve(process.cwd(), "relative-root"));
    expect(gdaRoot()).toBe(path.join(path.resolve(process.cwd(), "relative-root"), ".godot-autopilot"));
  });

  test("未设覆写时回退到家目录下的 .godot-autopilot", () => {
    delete process.env.GODOT_AUTOPILOT_HOME;
    const home = path.resolve(os.homedir());
    expect(path.resolve(gdaHome())).toBe(home);
    expect(path.resolve(gdaRoot())).toBe(path.join(home, ".godot-autopilot"));
  });

  test("覆写为空白字符串时按未设处理", () => {
    process.env.GODOT_AUTOPILOT_HOME = "   ";
    const home = path.resolve(os.homedir());
    expect(path.resolve(gdaHome())).toBe(home);
    expect(path.resolve(gdaRoot())).toBe(path.join(home, ".godot-autopilot"));
  });
});

describe("stateDir", () => {
  test("默认落在集中根的 cli 子目录", () => {
    process.env.GODOT_AUTOPILOT_HOME = tmp;
    expect(stateDir()).toBe(path.join(tmp, ".godot-autopilot", "cli"));
  });
});

describe("注册表", () => {
  test("写入条目后能从集中根下读出且含扩展字段", () => {
    process.env.GODOT_AUTOPILOT_HOME = tmp;
    const entry = makeEntry({ projectPath: tmp, instancePort: 9527, apiAvailable: false });
    saveRegistry({ version: 1, instances: [entry] });
    const file = path.join(tmp, ".godot-autopilot", "cli", "instances.json");
    expect(fs.existsSync(file)).toBe(true);
    const loaded = loadRegistry();
    expect(loaded.instances).toHaveLength(1);
    expect(loaded.instances[0].projectPath).toBe(tmp);
    expect(loaded.instances[0].instancePort).toBe(9527);
    expect(loaded.instances[0].apiAvailable).toBe(false);
  });

  test("读取缺扩展字段的旧结构条目不抛异常且取默认值", () => {
    process.env.GODOT_AUTOPILOT_HOME = tmp;
    const dir = path.join(tmp, ".godot-autopilot", "cli");
    fs.mkdirSync(dir, { recursive: true });
    const legacy = {
      id: "p2",
      project: tmp,
      port: 9531,
      pid: process.pid,
      startedAt: "2026-10-20T00:00:00.000Z",
      headless: false,
      godot: "godot",
      log: path.join(tmp, "legacy.log"),
    };
    fs.writeFileSync(path.join(dir, "instances.json"), JSON.stringify({ version: 1, instances: [legacy] }, null, 2));
    const loaded = loadRegistry();
    expect(loaded.instances).toHaveLength(1);
    expect(loaded.instances[0].projectPath).toBe(tmp);
    expect(loaded.instances[0].instancePort).toBe(9531);
    expect(loaded.instances[0].apiAvailable).toBe(false);
  });
});
