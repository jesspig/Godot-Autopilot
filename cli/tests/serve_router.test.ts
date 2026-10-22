import { describe, expect, test } from "bun:test";

import type { PluginInstanceRecord } from "../src/instances";
import {
  checkInstanceReachable,
  controlInstancesPayload,
  instanceEndpoint,
  instanceLabel,
  instanceSummary,
  matchRoute,
  resolveInstanceByRef,
  selectDefaultInstance,
  upstreamUrl,
  type ControlRoute,
} from "../src/serve/router";

const TOKEN = "0123456789abcdef".repeat(4);

function record(overrides: Partial<PluginInstanceRecord> = {}): PluginInstanceRecord {
  const port = overrides.port ?? 9530;
  return {
    instanceId: "p1",
    port,
    pid: process.pid,
    projectPath: "D:/projects/demo",
    apiToken: TOKEN,
    mcpEndpoint: { host: "127.0.0.1", port, path: "/mcp", protocol: "mcp", auth: "none" },
    apiEndpoint: { host: "127.0.0.1", port, path: "/api", protocol: "http", auth: "bearer" },
    startedAt: 1,
    version: "0.3.0",
    ...overrides,
  };
}

const ROUTE_CASES: Array<[string, ControlRoute]> = [
  ["/control/health", { kind: "health" }],
  ["/control/instances", { kind: "instances" }],
  ["/api", { kind: "default", endpoint: "api", restPath: "/api" }],
  ["/mcp", { kind: "default", endpoint: "mcp", restPath: "/mcp" }],
  ["/api/tools/x", { kind: "default", endpoint: "api", restPath: "/api/tools/x" }],
  ["/api/", { kind: "default", endpoint: "api", restPath: "/api/" }],
  ["/mcp/", { kind: "default", endpoint: "mcp", restPath: "/mcp/" }],
  ["/i/p1/api/health", { kind: "instance", ref: "p1", endpoint: "api", restPath: "/api/health" }],
  ["/i/9527/mcp", { kind: "instance", ref: "9527", endpoint: "mcp", restPath: "/mcp" }],
  ["/i/p1/api", { kind: "instance", ref: "p1", endpoint: "api", restPath: "/api" }],
  ["/i/p1/mcp/", { kind: "instance", ref: "p1", endpoint: "mcp", restPath: "/mcp/" }],
];

const INVALID_PATHS = [
  "/",
  "/apix",
  "/api2/tools",
  "/i/p1/other",
  "/i/p1/api2",
  "/i//api",
  "/i/p1",
  "/control/health/x",
  "/control/instances/",
  "/control",
];

describe("路由解析", () => {
  for (const [path, expected] of ROUTE_CASES) {
    test(`${path} 命中 ${expected.kind}`, () => {
      expect(matchRoute(path)).toEqual(expected);
    });
  }

  for (const path of INVALID_PATHS) {
    test(`${path} 返回 not_found`, () => {
      expect(matchRoute(path)).toEqual({ kind: "not_found" });
    });
  }
});

describe("实例寻址", () => {
  test("按 instanceId 精确匹配", () => {
    const target = record({ instanceId: "p1" });
    expect(resolveInstanceByRef([target], "p1", () => true)).toBe(target);
  });

  test("按命令行实例标识的进程号匹配", () => {
    const target = record({ instanceId: "gda-2a2f6e58-43210-1", pid: 43210 });
    expect(resolveInstanceByRef([target], "p43210", () => true)).toBe(target);
  });

  test("进程号标识与端口号标识不互相干扰", () => {
    const byPid = record({ instanceId: "gda-a", pid: 43210, port: 9530 });
    const byPort = record({ instanceId: "gda-b", pid: 1234, port: 43210 });
    expect(resolveInstanceByRef([byPid, byPort], "p43210", () => true)?.instanceId).toBe("gda-a");
    expect(resolveInstanceByRef([byPid, byPort], "43210", () => true)?.instanceId).toBe("gda-b");
  });

  test("同形时 id 优先于端口", () => {
    const byPort = record({ instanceId: "other", port: 9527 });
    const byId = record({ instanceId: "9527", port: 9600 });
    expect(resolveInstanceByRef([byPort, byId], "9527", () => true)?.instanceId).toBe("9527");
  });

  test("按端口匹配共享端口与端点端口", () => {
    const shared = record({ instanceId: "shared", port: 9530 });
    const endpointOnly = record({
      instanceId: "endpoint-only",
      port: null,
      mcpEndpoint: null,
      apiEndpoint: { host: "127.0.0.1", port: 9540, path: "/api", protocol: "http", auth: "bearer" },
    });
    expect(resolveInstanceByRef([shared, endpointOnly], "9530", () => true)?.instanceId).toBe("shared");
    expect(resolveInstanceByRef([shared, endpointOnly], "9540", () => true)?.instanceId).toBe("endpoint-only");
  });

  test("同端口有存活与不存活记录时优先存活项", () => {
    const dead = record({ instanceId: "dead", port: 9550, pid: 101 });
    const live = record({ instanceId: "live", port: 9550, pid: 202 });
    const alive = (pid: number): boolean => pid === 202;
    expect(resolveInstanceByRef([dead, live], "9550", alive)?.instanceId).toBe("live");
  });

  test("未知 id、非数字端口与越界端口返回 undefined", () => {
    const target = record();
    expect(resolveInstanceByRef([target], "p9", () => true)).toBeUndefined();
    expect(resolveInstanceByRef([target], "abc", () => true)).toBeUndefined();
    expect(resolveInstanceByRef([target], "70000", () => true)).toBeUndefined();
  });
});

describe("缺省实例选择", () => {
  test("零个活跃实例", () => {
    expect(selectDefaultInstance([], () => true)).toEqual({ kind: "none" });
  });

  test("全部不存活视为零个", () => {
    expect(selectDefaultInstance([record({ pid: 101 })], () => false)).toEqual({ kind: "none" });
  });

  test("唯一活跃实例", () => {
    const target = record({ pid: 202 });
    expect(selectDefaultInstance([target], (pid) => pid === 202)).toEqual({ kind: "single", record: target });
  });

  test("多个活跃实例返回候选且候选不含令牌", () => {
    const first = record({ instanceId: "p1", port: 9530, pid: 101 });
    const second = record({ instanceId: "p2", port: 9531, pid: 202 });
    const selection = selectDefaultInstance([first, second], () => true);
    expect(selection.kind).toBe("ambiguous");
    if (selection.kind !== "ambiguous") return;
    expect(selection.candidates).toHaveLength(2);
    expect(selection.candidates.map((candidate) => candidate.id)).toEqual(["p1", "p2"]);
    expect(JSON.stringify(selection)).not.toContain(TOKEN);
  });
});

describe("可达性探测", () => {
  test("存活且探测通过时可达", async () => {
    const target = record({ pid: 202 });
    const targets: string[] = [];
    const reachable = await checkInstanceReachable(
      target,
      "api",
      async (probe) => {
        targets.push(probe.url);
        return true;
      },
      () => true,
    );
    expect(reachable).toBe(true);
    expect(targets).toEqual(["http://127.0.0.1:9530/api"]);
  });

  test("进程不存活时直接判不可达且不探测", async () => {
    let probed = false;
    const reachable = await checkInstanceReachable(
      record({ pid: 202 }),
      "mcp",
      async () => {
        probed = true;
        return true;
      },
      () => false,
    );
    expect(reachable).toBe(false);
    expect(probed).toBe(false);
  });

  test("探测失败判不可达", async () => {
    expect(await checkInstanceReachable(record(), "api", async () => false, () => true)).toBe(false);
  });

  test("无端口与端点时判不可达", async () => {
    const target = record({ port: null, mcpEndpoint: null, apiEndpoint: null });
    expect(await checkInstanceReachable(target, "api", async () => true, () => true)).toBe(false);
  });
});

describe("实例清单", () => {
  test("只输出活跃实例且字段齐全", () => {
    const target = record({ instanceId: "p1" });
    const payload = controlInstancesPayload([target], () => true);
    expect(payload.instances).toHaveLength(1);
    const item = payload.instances[0];
    for (const key of ["id", "port", "pid", "project", "version", "live", "endpoints"]) {
      expect(key in item).toBe(true);
    }
    expect(item.id).toBe("p1");
    expect(item.project).toBe("D:/projects/demo");
    expect(item.live).toBe(true);
    expect(item.endpoints.api?.url).toBe("http://127.0.0.1:9530/api");
    expect(item.endpoints.mcp?.url).toBe("http://127.0.0.1:9530/mcp");
  });

  test("输出绝不包含 apiToken", () => {
    const payload = controlInstancesPayload([record()], () => true);
    const text = JSON.stringify(payload);
    expect(text).not.toContain(TOKEN);
    expect(text).not.toContain("apiToken");
    expect(text).not.toContain("api_token");
  });

  test("不存活实例不出现在清单里", () => {
    expect(controlInstancesPayload([record()], () => false).instances).toEqual([]);
  });

  test("summary 不暴露令牌", () => {
    const summary = instanceSummary(record(), () => true);
    expect(JSON.stringify(summary)).not.toContain(TOKEN);
    expect(summary.live).toBe(true);
  });
});

describe("转发目标构造", () => {
  test("共享端口无尾斜杠时保留原路径", () => {
    const target = record({ port: 9530 });
    const endpoint = instanceEndpoint(target, "api");
    expect(endpoint).not.toBeNull();
    expect(upstreamUrl(endpoint!, "/api")).toBe("http://127.0.0.1:9530/api");
    expect(upstreamUrl(endpoint!, "/api/tools")).toBe("http://127.0.0.1:9530/api/tools");
  });

  test("IPv6 主机加方括号", () => {
    const endpoint = { host: "::1", port: 9530, path: "/api", protocol: "http", auth: "bearer" };
    expect(upstreamUrl(endpoint, "/api")).toBe("http://[::1]:9530/api");
  });

  test("缺少 instanceId 时以端口生成标签", () => {
    expect(instanceLabel(record({ instanceId: "", port: 9530 }))).toBe("port:9530");
    expect(instanceLabel(record({ instanceId: "p1" }))).toBe("p1");
  });
});
