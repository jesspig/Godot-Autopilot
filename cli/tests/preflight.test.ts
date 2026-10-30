import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

import { detectActiveInstance, multiInstanceAllowed, MULTI_INSTANCE_ENV } from "../src/preflight";
import { saveRegistry, type InstanceEntry } from "../src/registry";
import { removeDirWithRetry } from "./support/cleanup";

let tmp: string;
let projectDir: string;
let prevHome: string | undefined;
let prevCliHome: string | undefined;
let prevAllow: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-preflight-"));
  projectDir = path.join(tmp, "proj");
  fs.mkdirSync(projectDir, { recursive: true });
  prevHome = process.env.GODOT_AUTOPILOT_HOME;
  prevCliHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  prevAllow = process.env[MULTI_INSTANCE_ENV];
  process.env.GODOT_AUTOPILOT_HOME = tmp;
  process.env.GODOT_AUTOPILOT_CLI_HOME = path.join(tmp, "cli");
  delete process.env[MULTI_INSTANCE_ENV];
});

afterEach(async () => {
  if (prevHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = prevHome;
  if (prevCliHome === undefined) delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  else process.env.GODOT_AUTOPILOT_CLI_HOME = prevCliHome;
  if (prevAllow === undefined) delete process.env[MULTI_INSTANCE_ENV];
  else process.env[MULTI_INSTANCE_ENV] = prevAllow;
  await removeDirWithRetry(tmp);
});

function makeEntry(overrides: Partial<InstanceEntry> = {}): InstanceEntry {
  return {
    id: "p1",
    project: projectDir,
    port: 9527,
    pid: process.pid,
    startedAt: "2026-10-22T00:00:00.000Z",
    headless: true,
    godot: "godot",
    log: path.join(tmp, "editor.log"),
    ...overrides,
  };
}

function writePluginIndex(records: unknown[]): void {
  const dir = path.join(tmp, ".godot-autopilot");
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, "instances.json"), JSON.stringify({ instances: records }, null, 2));
}

function pluginRecord(port: number, pid: number): Record<string, unknown> {
  return {
    instance_id: `gda-abcdef12-${pid}-1760000000000`,
    port,
    pid,
    project_path: projectDir,
    api_token: "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
    endpoints: {
      mcp: { host: "127.0.0.1", port, path: "/mcp", protocol: "mcp", auth: "none" },
      api: { host: "127.0.0.1", port, path: "/api", protocol: "http", auth: "bearer" },
    },
    started_at: 1760000000000,
    version: "0.3.0",
  };
}

describe("detectActiveInstance", () => {
  test("无任何登记时检测不到活跃实例", async () => {
    expect(await detectActiveInstance(projectDir)).toBeUndefined();
  });

  test("CLI 注册表有存活实例时命中并带出端口与进程号", async () => {
    saveRegistry({ version: 1, instances: [makeEntry({ port: 9545, pid: process.pid })] });
    const active = await detectActiveInstance(projectDir);
    expect(active).not.toBeUndefined();
    expect(active!.source).toBe("cli-registry");
    expect(active!.port).toBe(9545);
    expect(active!.pid).toBe(process.pid);
  });

  test("CLI 注册表条目进程已死时忽略", async () => {
    saveRegistry({ version: 1, instances: [makeEntry({ pid: 999999 })] });
    expect(await detectActiveInstance(projectDir)).toBeUndefined();
  });

  test("插件实例索引的存活且端口可达记录被命中", async () => {
    const server = Bun.serve({ port: 0, hostname: "127.0.0.1", fetch: () => new Response("x") });
    try {
      const port = server.port!;
      writePluginIndex([pluginRecord(port, process.pid)]);
      const active = await detectActiveInstance(projectDir);
      expect(active).not.toBeUndefined();
      expect(active!.source).toBe("plugin-index");
      expect(active!.port).toBe(port);
      expect(active!.pid).toBe(process.pid);
    } finally {
      server.stop(true);
    }
  });

  test("插件实例索引记录端口不可达时忽略", async () => {
    writePluginIndex([pluginRecord(9599, process.pid)]);
    expect(await detectActiveInstance(projectDir)).toBeUndefined();
  });

  test("插件实例索引记录进程已死时忽略", async () => {
    const server = Bun.serve({ port: 0, hostname: "127.0.0.1", fetch: () => new Response("x") });
    try {
      writePluginIndex([pluginRecord(server.port!, 999999)]);
      expect(await detectActiveInstance(projectDir)).toBeUndefined();
    } finally {
      server.stop(true);
    }
  });

  test("其它工程的插件实例索引记录不被计入", async () => {
    const server = Bun.serve({ port: 0, hostname: "127.0.0.1", fetch: () => new Response("x") });
    try {
      const record = pluginRecord(server.port!, process.pid);
      record["project_path"] = path.join(tmp, "other-proj");
      writePluginIndex([record]);
      expect(await detectActiveInstance(projectDir)).toBeUndefined();
    } finally {
      server.stop(true);
    }
  });
});

describe("multiInstanceAllowed", () => {
  test("未设置时为关闭", () => {
    expect(multiInstanceAllowed()).toBe(false);
  });

  test("空串与关闭类取值视为关闭", () => {
    for (const value of ["", "   ", "0", "false", "off", "no"]) {
      process.env[MULTI_INSTANCE_ENV] = value;
      expect(multiInstanceAllowed()).toBe(false);
    }
  });

  test("其余取值视为开启", () => {
    for (const value of ["1", "true", "on", "yes"]) {
      process.env[MULTI_INSTANCE_ENV] = value;
      expect(multiInstanceAllowed()).toBe(true);
    }
  });
});
