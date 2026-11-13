import fs from "node:fs";
import path from "node:path";

import { afterEach, beforeEach, describe, expect, test } from "bun:test";

import { pluginIndexPath, readPluginInstanceIndex } from "../src/instances";

function baseRecord(overrides: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    instance_id: "gda-test-9601",
    port: 9601,
    pid: 4242,
    project_path: "C:/projects/sample",
    api_token: "token",
    endpoints: {
      mcp: { host: "127.0.0.1", port: 9601, path: "/mcp", protocol: "mcp", auth: "none" },
      api: { host: "127.0.0.1", port: 9601, path: "/api", protocol: "http", auth: "bearer" },
    },
    gda_home: "C:/home",
    started_at: 1700000000000,
    version: "0.3.0",
    ...overrides,
  };
}

let tmpHome: string;
let prevHome: string | undefined;

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmpHome = fs.mkdtempSync(path.join(base, "gda-instances-ipc-"));
  prevHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmpHome;
});

afterEach(() => {
  if (prevHome === undefined) {
    delete process.env.GODOT_AUTOPILOT_HOME;
  } else {
    process.env.GODOT_AUTOPILOT_HOME = prevHome;
  }
  fs.rmSync(tmpHome, { recursive: true, force: true });
});

function writeIndex(records: unknown[]): void {
  const file = pluginIndexPath();
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, JSON.stringify(records), "utf8");
}

describe("instances ipc endpoint", () => {
  test("旧记录缺 ipc 字段时解析为 null", () => {
    writeIndex([baseRecord()]);
    const records = readPluginInstanceIndex();
    expect(records).toHaveLength(1);
    expect(records[0].ipcEndpoint).toBeNull();
    expect(records[0].port).toBe(9601);
  });

  test("新记录解析 unix ipc 端点字段", () => {
    const record = baseRecord();
    (record.endpoints as Record<string, unknown>).ipc = {
      kind: "unix",
      path: "C:/gda/run/sample-1234abcd.sock",
      token: "ipc-token",
      protocol: "mcp-jsonrpc",
      framing: "length-prefixed-json",
      auth: "token",
      version: 1,
    };
    writeIndex([record]);
    const records = readPluginInstanceIndex();
    expect(records).toHaveLength(1);
    expect(records[0].ipcEndpoint).toEqual({
      kind: "unix",
      path: "C:/gda/run/sample-1234abcd.sock",
      name: "",
      host: "",
      port: null,
      token: "ipc-token",
      protocol: "mcp-jsonrpc",
      framing: "length-prefixed-json",
      auth: "token",
      version: 1,
    });
  });

  test("新记录解析 tcp ipc 端点字段", () => {
    const record = baseRecord();
    (record.endpoints as Record<string, unknown>).ipc = {
      kind: "tcp",
      host: "127.0.0.1",
      port: 21437,
      token: "tcp-token",
      protocol: "mcp-jsonrpc",
      framing: "length-prefixed-json",
      auth: "token",
      version: 1,
    };
    writeIndex([record]);
    const records = readPluginInstanceIndex();
    expect(records).toHaveLength(1);
    expect(records[0].ipcEndpoint).toEqual({
      kind: "tcp",
      path: "",
      name: "",
      host: "127.0.0.1",
      port: 21437,
      token: "tcp-token",
      protocol: "mcp-jsonrpc",
      framing: "length-prefixed-json",
      auth: "token",
      version: 1,
    });
  });

  test("ipc 缺 path/tcp 地址时按无 IPC 处理", () => {
    const record = baseRecord();
    (record.endpoints as Record<string, unknown>).ipc = { protocol: "mcp-jsonrpc" };
    writeIndex([record]);
    const records = readPluginInstanceIndex();
    expect(records).toHaveLength(1);
    expect(records[0].ipcEndpoint).toBeNull();
  });
});