import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import { COMMAND_SPECS, findSpec } from "../src/command-spec";
import { runGenericToolCall } from "../src/commands/scene";
import { ExitCode } from "../src/errors";

interface CatalogEvidence {
  tool_names: string[];
  catalog_total: number;
}

const evidencePath = path.resolve(import.meta.dir, "..", "..", "evidence", "T-01", "tool-catalog.json");
const catalog = JSON.parse(fs.readFileSync(evidencePath, "utf8")) as CatalogEvidence;
const canonical = new Set(catalog.tool_names);

const DOMAINS = ["scene", "script", "resource", "game"];
const GENERIC_TOOL = "physics_query";

interface FakeNative {
  port: number;
  requests: Array<Record<string, unknown>>;
  close(): Promise<void>;
}

async function serveNative(): Promise<FakeNative> {
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
          instance_id: String(request.instance_id ?? ""),
          method: String(request.method ?? ""),
          params: [{ key: "result", value: JSON.stringify({ status: "ok", payload: { ok: true }, artifact_id: "", digest: "", bytes: 0, task_id: "" }) }],
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

let tmp: string;
let prevCliHome: string | undefined;
let prevGdaHome: string | undefined;
let native: FakeNative | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-coverage-test-"));
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

describe("T-09 工具覆盖", () => {
  test("canonical 工具目录可用", () => {
    expect(catalog.catalog_total).toBeGreaterThan(200);
    expect(canonical.size).toBeGreaterThan(200);
    expect(canonical.has(GENERIC_TOOL)).toBe(true);
  });

  test("scene/script/resource/game 的 spec.tools 全部指向 canonical 工具", () => {
    for (const spec of COMMAND_SPECS.filter((item) => DOMAINS.includes(item.group))) {
      for (const tool of spec.tools) {
        expect(canonical.has(tool)).toBe(true);
      }
    }
  });

  test("每个 canonical 工具有专用命令或通用 tool call 通路", () => {
    const lifted = new Set<string>();
    for (const spec of COMMAND_SPECS) {
      for (const tool of spec.tools) lifted.add(tool);
    }
    const toolCall = findSpec("tool.call");
    expect(toolCall).toBeDefined();
    expect(toolCall!.tools).toContain("tool_call");
    for (const flag of ["--args", "--args-file", "--confirm"]) {
      expect(toolCall!.flags.map((item) => item.name)).toContain(flag);
    }
    for (const tool of canonical) {
      if (!lifted.has(tool)) {
        expect(toolCall).toBeDefined();
      }
    }
    expect(lifted.size).toBeGreaterThan(20);
  });

  test("未提升工具经原生 call.start 仍可通用调用", async () => {
    native = await serveNative();
    const dir = path.join(tmp, ".godot-autopilot");
    fs.mkdirSync(dir, { recursive: true });
    fs.writeFileSync(path.join(dir, "instances.json"), JSON.stringify({
      instances: [{
        instance_id: "coverage-native",
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
    const original = process.stdout.write;
    process.stdout.write = (() => true) as typeof process.stdout.write;
    try {
      const code = await runGenericToolCall(GENERIC_TOOL, {
        portText: String(native.port),
        argsJson: '{"action":"ray"}',
      });
      expect(code).toBe(ExitCode.Success);
    } finally {
      process.stdout.write = original;
    }
    const params = (native.requests.find((request) => request.method === "call.start")?.params ?? []) as Array<{ key: string; value: string }>;
    const map: Record<string, string> = {};
    for (const entry of params) map[entry.key] = entry.value;
    expect(map.tool).toBe(GENERIC_TOOL);
    expect(map.action).toBe("ray");
  });

  test("T-09 二级动作数量达到任务要求", () => {
    const expected: Readonly<Record<string, string[]>> = {
      scene: ["tree", "node", "property", "signal", "save", "verify", "diff"],
      script: ["create", "patch", "compile", "attach", "exec", "reload", "test"],
      resource: ["inspect", "get", "set", "save", "import", "rename", "move", "references"],
      game: ["play", "stop", "status", "input", "capture", "eval", "scenario"],
    };
    for (const [group, subs] of Object.entries(expected)) {
      for (const sub of subs) {
        expect(findSpec(`${group}.${sub}`)).toBeDefined();
      }
    }
  });
});
