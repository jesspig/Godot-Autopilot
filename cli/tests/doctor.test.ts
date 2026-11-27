import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import type { GlobalOptions } from "../src/args";
import { diagnose, doctor, run as runDoctor } from "../src/commands/doctor";
import { catalogPath, writeOfflineCatalog, type NativeCatalogEntry } from "../src/discovery";

interface NativeServer {
  port: number;
  requests: Array<Record<string, unknown>>;
  close(): Promise<void>;
}

async function serveHello(): Promise<NativeServer> {
  const requests: Array<Record<string, unknown>> = [];
  const sockets: net.Socket[] = [];
  const server = net.createServer((socket) => {
    sockets.push(socket);
    let buffer = Buffer.alloc(0);
    socket.on("data", (chunk: Buffer) => {
      buffer = Buffer.concat([buffer, chunk]);
      while (buffer.length >= 4) {
        const size = buffer.readUInt32BE(0);
        if (buffer.length < 4 + size) break;
        const text = buffer.subarray(4, 4 + size).toString("utf8");
        buffer = buffer.subarray(4 + size);
        const request = JSON.parse(text) as Record<string, unknown>;
        requests.push(request);
        const body = Buffer.from(JSON.stringify({
          v: 1,
          kind: "response",
          id: `r-${String(request.id)}`,
          instance_id: request.instance_id ?? "",
          method: request.method ?? "",
          params: [{ key: "result", value: JSON.stringify({ instance_id: "doctor-1", capabilities: {} }) }],
          status: "ok",
          error: 0,
          error_message: "",
        }));
        const header = Buffer.alloc(4);
        header.writeUInt32BE(body.length, 0);
        socket.write(Buffer.concat([header, body]));
      }
    });
  });
  await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
  return {
    port: (server.address() as net.AddressInfo).port,
    requests,
    close: async () => {
      for (const socket of sockets) socket.destroy();
      await new Promise<void>((resolve) => server.close(() => resolve()));
    },
  };
}

function writePluginIndex(tmp: string, port: number, pid = process.pid): void {
  const dir = path.join(tmp, ".godot-autopilot");
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, "instances.json"), JSON.stringify({
    instances: [{
      instance_id: "doctor-1",
      port,
      pid,
      project_path: path.join(tmp, "proj"),
      endpoints: {
        ipc: { kind: "tcp", host: "127.0.0.1", port, protocol: "gda-ipc-v1", framing: "length-prefixed-json", auth: "token", version: 1 },
      },
      started_at: 0,
      version: "0.3.5",
      port_source: "persisted",
    }],
  }));
}

function writeRegistry(tmp: string, pid: number): void {
  fs.mkdirSync(tmp, { recursive: true });
  fs.writeFileSync(path.join(tmp, "instances.json"), JSON.stringify({
    version: 1,
    instances: [{
      id: "doctor-1",
      project: path.join(tmp, "proj"),
      port: 6100,
      pid,
      startedAt: "",
      headless: true,
      godot: "godot",
      log: path.join(tmp, "editor.log"),
    }],
  }));
}

function capture(): { stdout(): string; restore(): void } {
  const original = process.stdout.write;
  let output = "";
  process.stdout.write = ((chunk: unknown) => {
    output += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  return { stdout: () => output, restore: () => { process.stdout.write = original; } };
}

async function captureRun(fn: () => Promise<number>): Promise<{ code: number; stdout: string }> {
  const cap = capture();
  try {
    const code = await fn();
    return { code, stdout: cap.stdout() };
  } finally {
    cap.restore();
  }
}

let tmp: string;
let prevCliHome: string | undefined;
let prevGdaHome: string | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-doctor-test-"));
  prevCliHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  prevGdaHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmp;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
  delete process.env.GODOT_AUTOPILOT_PORT;
});

afterEach(() => {
  if (prevCliHome === undefined) delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  else process.env.GODOT_AUTOPILOT_CLI_HOME = prevCliHome;
  if (prevGdaHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = prevGdaHome;
  fs.rmSync(tmp, { recursive: true, force: true });
});

const TOOLS: NativeCatalogEntry[] = [{
  name: "scene_open",
  description: "Open a scene",
  category: "Scene",
  tags: [],
  aliases: [],
  mutating: true,
  high_risk: false,
}];

describe("doctor --ci", () => {
  test("无实例与无缓存时给出稳定 code 与 remediation", async () => {
    const opts = { "--ci": true } as unknown as GlobalOptions;
    const { code, stdout } = await captureRun(() => doctor(opts));
    expect(code).toBe(0);
    const body = JSON.parse(stdout) as {
      ok: boolean;
      data: { mode: string; checks: Array<{ code: string; detail: string; remediation: string }> };
    };
    expect(body.data.mode).toBe("ci");
    expect(body.ok).toBe(true);
    const codes = body.data.checks.map((check) => check.code);
    expect(codes).toContain("GDA_NO_INSTANCE");
    expect(codes).toContain("GDA_CATALOG_MISSING");
    for (const check of body.data.checks) {
      expect(check.remediation.length).toBeGreaterThan(0);
      expect(check.detail).not.toContain(tmp);
    }
  });

  test("陈旧实例报 GDA_STALE_INSTANCE 且 exit 2", async () => {
    writeRegistry(tmp, 0);
    const { code, stdout } = await captureRun(() => doctor({ "--ci": true } as unknown as GlobalOptions));
    expect(code).toBe(2);
    const codes = (JSON.parse(stdout) as { data: { checks: Array<{ code: string }> } }).data.checks.map(
      (check) => check.code,
    );
    expect(codes).toContain("GDA_STALE_INSTANCE");
  });

  test("catalog hash 不匹配报 GDA_CATALOG_HASH_MISMATCH", async () => {
    writeOfflineCatalog(TOOLS);
    const stored = JSON.parse(fs.readFileSync(catalogPath(), "utf8"));
    stored.tools[0].name = "changed";
    fs.writeFileSync(catalogPath(), JSON.stringify(stored));
    const { code, stdout } = await captureRun(() => doctor({ "--ci": true } as unknown as GlobalOptions));
    expect(code).toBe(2);
    const codes = (JSON.parse(stdout) as { data: { checks: Array<{ code: string }> } }).data.checks.map(
      (check) => check.code,
    );
    expect(codes).toContain("GDA_CATALOG_HASH_MISMATCH");
  });

  test("doctor 子命令路由到 run", async () => {
    const { code, stdout } = await captureRun(() => runDoctor(["run"], { "--ci": true } as unknown as GlobalOptions));
    expect(code).toBe(0);
    expect((JSON.parse(stdout) as { data: { mode: string } }).data.mode).toBe("ci");
  });
});

describe("diagnose", () => {
  test("无目标时报 GDA_TARGET_FAILED 且给出可操作提示", async () => {
    const { code, stdout } = await captureRun(() => diagnose([], { "--ci": true } as unknown as GlobalOptions));
    expect(code).toBe(2);
    const body = JSON.parse(stdout) as {
      data: { checks: Array<{ id: string; code: string; remediation: string }> };
    };
    const targetCheck = body.data.checks.find((check) => check.id === "target");
    expect(targetCheck).toBeDefined();
    expect(targetCheck!.code).toBe("GDA_TARGET_FAILED");
    expect(targetCheck!.remediation).toContain("--id");
  });

  test("活实例经原生 hello 诊断成功且不发送 initialize 握手帧", async () => {
    const native = await serveHello();
    try {
      writePluginIndex(tmp, native.port);
      writeRegistry(tmp, process.pid);
      const { code, stdout } = await captureRun(() =>
        diagnose([], { portText: String(native.port), "--ci": true } as unknown as GlobalOptions),
      );
      expect(code).toBe(0);
      const body = JSON.parse(stdout) as { data: { checks: Array<{ code: string }> } };
      const codes = body.data.checks.map((check) => check.code);
      expect(codes).toContain("GDA_TARGET_OK");
      expect(codes).toContain("GDA_NATIVE_OK");
      expect(native.requests.map((request) => request.method)).not.toContain("initialize");
    } finally {
      await native.close();
    }
  });
});