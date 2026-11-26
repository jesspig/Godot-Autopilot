import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import type { GlobalOptions } from "../src/args";
import { run as runResource } from "../src/commands/resource";
import { run as runScene } from "../src/commands/scene";
import { CliError, ExitCode } from "../src/errors";

interface FakeNative {
  port: number;
  requests: Array<{ method: string; params: Record<string, string> }>;
  callTools: string[];
  close(): Promise<void>;
}

function paramsOf(request: Record<string, unknown>): Record<string, string> {
  const params = Array.isArray(request.params) ? request.params as Array<{ key: string; value: string }> : [];
  const out: Record<string, string> = {};
  for (const entry of params) out[entry.key] = entry.value;
  return out;
}

function invocation(payload: unknown): Record<string, unknown> {
  return { status: "ok", payload, artifact_id: "a", digest: "d", bytes: 1, task_id: "" };
}

async function serveNative(store: Map<string, string>): Promise<FakeNative> {
  const requests: Array<{ method: string; params: Record<string, string> }> = [];
  const callTools: string[] = [];
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
        const params = paramsOf(request);
        requests.push({ method: String(request.method ?? ""), params });

        let payload: unknown = { ok: true };
        if (request.method === "call.start") {
          callTools.push(params.tool ?? "");
          if (params.tool === "property_set") {
            store.set(`${params.path}.${params.property}`, params.value);
            payload = { ok: true, scene_path: "res://main.tscn", revision: "rev-1" };
          } else if (params.tool === "get_scene_tree") {
            payload = { ok: true, properties: Object.fromEntries(store) };
          } else if (params.tool === "resource_property" && params.action === "set") {
            store.set(`${params.path}.${params.property}`, params.value);
            payload = { ok: true, revision: "rev-r" };
          } else if (params.tool === "resource_property" && params.action === "get") {
            payload = { ok: true, value: store.get(`${params.path}.${params.property}`) ?? null };
          } else if (params.tool === "resource_file") {
            store.set(`${params.path}->${params.new_path}`, "renamed");
            payload = { ok: true, updated_files: [params.path, params.new_path], uid_preserved: true };
          } else if (params.tool === "resource_query") {
            payload = { ok: true, references: [...store.keys()] };
          }
        }
        const body = Buffer.from(JSON.stringify({
          v: 1,
          kind: "response",
          id: `r-${String(request.id)}`,
          instance_id: String(request.instance_id ?? ""),
          method: String(request.method ?? ""),
          params: [{ key: "result", value: JSON.stringify(invocation(payload)) }],
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
    callTools,
    close: async () => {
      for (const socket of sockets) socket.destroy();
      await new Promise<void>((resolve) => server.close(() => resolve()));
    },
  };
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

async function captureRun(fn: () => Promise<number>): Promise<{ code: number; stdout: string; error?: CliError }> {
  const cap = capture();
  try {
    const code = await fn();
    return { code, stdout: cap.stdout() };
  } catch (error) {
    return { code: -1, stdout: cap.stdout(), error: error as CliError };
  } finally {
    cap.restore();
  }
}

let tmp: string;
let prevCliHome: string | undefined;
let prevGdaHome: string | undefined;
let native: FakeNative | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-readback-test-"));
  prevCliHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  prevGdaHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmp;
  process.env.GODOT_AUTOPILOT_HOME = tmp;
  delete process.env.GODOT_AUTOPILOT_PORT;
});

afterEach(async () => {
  if (native !== undefined) {
    await native.close();
    native = undefined;
  }
  if (prevCliHome === undefined) delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  else process.env.GODOT_AUTOPILOT_CLI_HOME = prevCliHome;
  if (prevGdaHome === undefined) delete process.env.GODOT_AUTOPILOT_HOME;
  else process.env.GODOT_AUTOPILOT_HOME = prevGdaHome;
  fs.rmSync(tmp, { recursive: true, force: true });
});

async function withNative(store: Map<string, string>): Promise<GlobalOptions> {
  native = await serveNative(store);
  const dir = path.join(tmp, ".godot-autopilot");
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, "instances.json"), JSON.stringify({
    instances: [{
      instance_id: "readback-native",
      port: native.port,
      pid: process.pid,
      project_path: path.join(tmp, "proj"),
      endpoints: {
        ipc: { kind: "tcp", host: "127.0.0.1", port: native.port, protocol: "mcp-jsonrpc", framing: "length-prefixed-json", auth: "token", version: 1 },
      },
      started_at: Date.now(),
      version: "0.3.5",
      port_source: "persisted",
    }],
  }));
  return { portText: String(native.port) };
}

describe("T-09 写后读回", () => {
  test("scene.property 写入后 scene.tree 读到相同值且调用顺序为先写后读", async () => {
    const store = new Map<string, string>();
    const opts = await withNative(store);
    const write = await captureRun(() => runScene(
      ["property", "Root/Label", "text", '"saved"'], opts,
    ));
    expect(write.code).toBe(ExitCode.Success);
    const writeBody = JSON.parse(write.stdout);
    expect(writeBody.scene_path).toBe("res://main.tscn");
    expect(writeBody.revision).toBe("rev-1");
    expect(writeBody.receipt.request_id).toBeString();

    const read = await captureRun(() => runScene(["tree"], opts));
    expect(read.code).toBe(ExitCode.Success);
    const readBody = JSON.parse(read.stdout);
    expect(readBody.result.properties["Root/Label.text"]).toBe("saved");

    expect(native!.callTools).toEqual(["property_set", "get_scene_tree"]);
  });

  test("resource.set 写入后 resource.get 读回写入值", async () => {
    const store = new Map<string, string>();
    const opts = await withNative(store);
    const write = await captureRun(() => runResource(["set"], {
      ...opts, "--path": "res://a.tres", "--property": "speed", "--value": "12",
    } as GlobalOptions));
    expect(write.code).toBe(ExitCode.Success);
    expect(JSON.parse(write.stdout).revision).toBe("rev-r");

    const read = await captureRun(() => runResource(["get"], {
      ...opts, "--path": "res://a.tres", "--property": "speed",
    } as GlobalOptions));
    expect(read.code).toBe(ExitCode.Success);
    expect(JSON.parse(read.stdout).result.value).toBe("12");
    expect(native!.callTools).toEqual(["resource_property", "resource_property"]);
  });

  test("高危资源事务经 --confirm 执行后可经 references 读回且不绕过确认", async () => {
    const store = new Map<string, string>();
    const opts = await withNative(store);

    const denied = await captureRun(() => runResource(["rename"], {
      ...opts, "--path": "res://a.tres", "--to": "res://b.tres",
    } as GlobalOptions));
    expect(denied.error).toBeInstanceOf(CliError);
    expect(denied.error!.message).toContain("--confirm");

    const accepted = await captureRun(() => runResource(["rename"], {
      ...opts, "--path": "res://a.tres", "--to": "res://b.tres", "--confirm": true,
    } as GlobalOptions));
    expect(accepted.code).toBe(ExitCode.Success);

    const read = await captureRun(() => runResource(["references"], {
      ...opts, "--path": "res://b.tres",
    } as GlobalOptions));
    expect(read.code).toBe(ExitCode.Success);
    expect(JSON.parse(read.stdout).result.references).toContain("res://a.tres->res://b.tres");

    const startRequests = native!.requests.filter((request) => request.method === "call.start");
    expect(startRequests).toHaveLength(2);
    expect(startRequests[0].params.confirm).toBe("true");
    expect(startRequests[1].params.confirm).toBeUndefined();
  });
});
