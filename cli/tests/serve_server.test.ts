import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import { parseBind, parseTimeoutSec } from "../src/commands/serve";
import { CliError, ExitCode } from "../src/errors";
import { pluginIndexPath } from "../src/instances";
import { startControlServer } from "../src/serve/server";
import { CLI_VERSION } from "../src/version";

const CONTROL_TOKEN = "control-token-abcdef0123";
const INSTANCE_TOKEN = "instance-token-9876543210";
const TEST_TIMEOUT_MS = 15_000;

interface EchoRequest {
  headers: Record<string, string>;
  url: string;
}

let tmp: string;
let prevHome: string | undefined;
let prevCliHome: string | undefined;
let control: ReturnType<typeof startControlServer> | undefined;
let upstreams: ReturnType<typeof Bun.serve>[] = [];
let rawServers: net.Server[] = [];

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-serve-server-test-"));
  prevHome = process.env.GODOT_AUTOPILOT_HOME;
  prevCliHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmp;
  control = undefined;
  upstreams = [];
  rawServers = [];
});

afterEach(() => {
  control?.stop(true);
  while (upstreams.length > 0) upstreams.pop()!.stop(true);
  while (rawServers.length > 0) rawServers.pop()!.close();
  if (prevHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = prevHome;
  if (prevCliHome === undefined) delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  else process.env.GODOT_AUTOPILOT_CLI_HOME = prevCliHome;
  fs.rmSync(tmp, { recursive: true, force: true });
});

async function quiet<T>(fn: () => Promise<T>): Promise<T> {
  const orig = process.stdout.write;
  process.stdout.write = (() => true) as typeof process.stdout.write;
  try {
    return await fn();
  } finally {
    process.stdout.write = orig;
  }
}

function startControl(): ReturnType<typeof startControlServer> {
  control = startControlServer({ hostname: "127.0.0.1", port: 0, authToken: CONTROL_TOKEN, timeoutSec: 2 });
  return control;
}

function authorized(init: RequestInit = {}): RequestInit {
  const headers = { ...(init.headers as Record<string, string> | undefined), authorization: `Bearer ${CONTROL_TOKEN}` };
  return { ...init, headers };
}

function startUpstream(mode: "echo" | "hang"): { port: number; requests: EchoRequest[] } {
  const requests: EchoRequest[] = [];
  const server = Bun.serve({
    port: 0,
    hostname: "127.0.0.1",
    fetch: async (req) => {
      if (mode === "hang") return new Promise<Response>(() => {});
      const headers: Record<string, string> = {};
      req.headers.forEach((value, name) => {
        headers[name] = value;
      });
      requests.push({ headers, url: req.url });
      return new Response(JSON.stringify({ ok: true, upstream: true }), {
        status: 200,
        headers: { "content-type": "application/json" },
      });
    },
  });
  upstreams.push(server);
  return { port: server.port!, requests };
}

function writeIndex(records: unknown[]): void {
  fs.mkdirSync(path.dirname(pluginIndexPath()), { recursive: true });
  fs.writeFileSync(pluginIndexPath(), JSON.stringify({ instances: records }));
}

function instanceRecord(overrides: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    instance_id: "i-test",
    pid: process.pid,
    port: null,
    project_path: tmp,
    api_token: INSTANCE_TOKEN,
    endpoints: {},
    started_at: Date.now(),
    version: "0.3.0",
    ...overrides,
  };
}

function apiEndpointRecord(port: number, overrides: Record<string, unknown> = {}): Record<string, unknown> {
  return instanceRecord({
    endpoints: { api: { host: "127.0.0.1", port, path: "/api" } },
    ...overrides,
  });
}

describe.serial("serve 控制面 HTTP", () => {
  test("/control/health 免鉴权返回 200 与 version/uptimeSec", async () => {
    const s = startControl();
    const res = await quiet(() => fetch(`http://127.0.0.1:${s.port}/control/health`));
    expect(res.status).toBe(200);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.ok).toBe(true);
    expect(body.version).toBe(CLI_VERSION);
    expect(body.uptimeSec).toBeGreaterThanOrEqual(0);
  });

  test("无/错 Bearer 访问 /control/instances 得 401 并带 WWW-Authenticate: Bearer", async () => {
    const s = startControl();
    for (const headers of [{}, { authorization: "Bearer wrong-token" }] as Record<string, string>[]) {
      const res = await quiet(() => fetch(`http://127.0.0.1:${s.port}/control/instances`, { headers }));
      expect(res.status).toBe(401);
      expect(res.headers.get("www-authenticate")).toBe("Bearer");
      const body = (await res.json()) as Record<string, unknown>;
      expect(body.error).toBe("unauthorized");
    }
  });

  test("未知路径合法 token 得 404", async () => {
    const s = startControl();
    const res = await quiet(() => fetch(`http://127.0.0.1:${s.port}/nope`, authorized()));
    expect(res.status).toBe(404);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.error).toBe("not_found");
  });

  test("/control/instances 200 且响应不含 apiToken 字段", async () => {
    writeIndex([apiEndpointRecord(1)]);
    const s = startControl();
    const res = await quiet(() => fetch(`http://127.0.0.1:${s.port}/control/instances`, authorized()));
    expect(res.status).toBe(200);
    const text = await res.text();
    expect(text).not.toContain(INSTANCE_TOKEN);
    expect(text).not.toContain("apiToken");
    expect(text).not.toContain("api_token");
    const body = JSON.parse(text) as { instances: Array<Record<string, unknown>> };
    expect(body.instances).toHaveLength(1);
    expect(body.instances[0].id).toBe("i-test");
  });

  test("/i/<ref>/api/health 转发假上游：上游收到实例 apiToken 且响应带 X-GDA-Instance", async () => {
    const up = startUpstream("echo");
    writeIndex([apiEndpointRecord(up.port)]);
    const s = startControl();
    const res = await quiet(() => fetch(`http://127.0.0.1:${s.port}/i/i-test/api/health`, authorized()));
    expect(res.status).toBe(200);
    expect(res.headers.get("x-gda-instance")).toBe("i-test");
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.upstream).toBe(true);
    expect(up.requests).toHaveLength(1);
    expect(up.requests[0].headers["authorization"]).toBe(`Bearer ${INSTANCE_TOKEN}`);
    expect(up.requests[0].url).toContain("/api/health");
  });

  test("上游 accept 后立刻断开得 502 upstream_unreachable", async () => {
    const raw = net.createServer((socket) => socket.destroy());
    await new Promise<void>((resolve) => raw.listen(0, "127.0.0.1", resolve));
    rawServers.push(raw);
    const port = (raw.address() as net.AddressInfo).port;
    writeIndex([apiEndpointRecord(port)]);
    const s = startControl();
    const res = await quiet(() => fetch(`http://127.0.0.1:${s.port}/i/i-test/api/health`, authorized()));
    expect(res.status).toBe(502);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.error).toBe("upstream_unreachable");
  });

  test("索引为空默认路由得 503 no_active_instance", async () => {
    const s = startControl();
    const res = await quiet(() => fetch(`http://127.0.0.1:${s.port}/api/health`, authorized()));
    expect(res.status).toBe(503);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.error).toBe("no_active_instance");
  });

  test("两个存活实例默认路由得 409 instance_ambiguous 且 candidates 不含令牌", async () => {
    writeIndex([
      apiEndpointRecord(1, { instance_id: "i-one" }),
      apiEndpointRecord(2, { instance_id: "i-two", api_token: "second-token-zzz" }),
    ]);
    const s = startControl();
    const res = await quiet(() => fetch(`http://127.0.0.1:${s.port}/api/health`, authorized()));
    expect(res.status).toBe(409);
    const text = await res.text();
    const body = JSON.parse(text) as { error: string; candidates: Array<{ id: string }> };
    expect(body.error).toBe("instance_ambiguous");
    expect(body.candidates.map((c) => c.id).sort()).toEqual(["i-one", "i-two"]);
    expect(text).not.toContain(INSTANCE_TOKEN);
    expect(text).not.toContain("second-token-zzz");
  });

  test("剥离名单头（authorization/host/proxy-authorization/te）不到达上游，非名单头保留", async () => {
    const up = startUpstream("echo");
    writeIndex([apiEndpointRecord(up.port)]);
    const s = startControl();
    const res = await quiet(() =>
      fetch(`http://127.0.0.1:${s.port}/i/i-test/api/health`, {
        method: "GET",
        headers: {
          authorization: `Bearer ${CONTROL_TOKEN}`,
          "proxy-authorization": "Bearer sneak",
          te: "trailers",
          "x-gda-echo": "keep-me",
        },
      }),
    );
    expect(res.status).toBe(200);
    expect(up.requests).toHaveLength(1);
    const got = up.requests[0].headers;
    expect(got["proxy-authorization"]).toBeUndefined();
    expect(got["te"]).toBeUndefined();
    expect(got["authorization"]).toBe(`Bearer ${INSTANCE_TOKEN}`);
    expect(got["x-gda-echo"]).toBe("keep-me");
    expect(got["host"]).toBe(`127.0.0.1:${up.port}`);
  });

  test("假上游挂起时 timeoutSec 到点得 504 upstream_timeout", async () => {
    const up = startUpstream("hang");
    writeIndex([apiEndpointRecord(up.port)]);
    const s = startControl();
    const res = await quiet(() => fetch(`http://127.0.0.1:${s.port}/i/i-test/api/health`, authorized()));
    expect(res.status).toBe(504);
    const body = (await res.json()) as Record<string, unknown>;
    expect(body.error).toBe("upstream_timeout");
  }, 10_000);
});

describe("parseBind", () => {
  test.each(["127.0.0.1", "::1", "localhost"])("接受环回地址 %s", (value) => {
    expect(parseBind(value)).toBe(value);
  });

  test.each(["0.0.0.0", "192.168.1.1"])("拒绝非环回地址 %s", (value) => {
    try {
      parseBind(value);
      expect.unreachable();
    } catch (e) {
      expect(e).toBeInstanceOf(CliError);
      expect((e as CliError).code).toBe(ExitCode.Usage);
    }
  });
});

describe("parseTimeoutSec", () => {
  test.each(["0", "-1", "abc"])("拒绝非法值 %s", (value) => {
    try {
      parseTimeoutSec(value);
      expect.unreachable();
    } catch (e) {
      expect(e).toBeInstanceOf(CliError);
      expect((e as CliError).code).toBe(ExitCode.Usage);
    }
  });

  test("接受正数 30", () => {
    expect(parseTimeoutSec("30")).toBe(30);
  });
});
