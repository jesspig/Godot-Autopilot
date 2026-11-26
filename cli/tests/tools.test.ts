import fs from "node:fs";
import net from "node:net";
import path from "node:path";

import { afterEach, beforeEach, describe, expect, test } from "bun:test";

import type { GlobalOptions } from "../src/args";
import { run as runTool } from "../src/commands/tool";
import { run as runTools } from "../src/commands/tools";
import {
  catalogPath,
  makeCatalogDocument,
  readOfflineCatalog,
  writeOfflineCatalog,
  type NativeCatalogEntry,
} from "../src/discovery";
import { CliError, ExitCode } from "../src/errors";

const servers: ReturnType<typeof Bun.serve>[] = [];

afterEach(() => {
  while (servers.length > 0) {
    servers.pop()!.stop();
  }
});

function serve(handler: (req: Request) => Response | Promise<Response>): number {
  const server = Bun.serve({ port: 0, fetch: handler });
  servers.push(server);
  return server.port!;
}

function sseResponse(payload: unknown): Response {
  return new Response(`event: message\ndata: ${JSON.stringify(payload)}\n\n`, {
    headers: { "content-type": "text/event-stream" },
  });
}

interface CapturedRequest {
  method: string;
  params: unknown;
}

function recordingServer(handler: (req: CapturedRequest) => Response | Promise<Response>) {
  const requests: CapturedRequest[] = [];
  const port = serve(async (req) => {
    const body = (await req.json()) as CapturedRequest;
    const params = body.params as Record<string, unknown> | undefined;
    if (params !== undefined && typeof params === "object" && params !== null) {
      const rest: Record<string, unknown> = { ...params };
      delete rest._meta;
      body.params = rest;
    }
    requests.push(body);
    return handler(body);
  });
  return { port, requests };
}

function searchEnvelope(names: string[]): unknown {
  const results = names.map((name, index) => ({ name, score: names.length - index }));
  return {
    jsonrpc: "2.0",
    id: 1,
    result: { content: [{ type: "text", text: JSON.stringify({ results }) }] },
  };
}

function captureOutput(): { stdout(): string; restore(): void } {
  const origOut = process.stdout.write;
  let outBuf = "";
  process.stdout.write = ((chunk: unknown) => {
    outBuf += String(chunk);
    return true;
  }) as typeof process.stdout.write;
  return {
    stdout: () => outBuf,
    restore: () => {
      process.stdout.write = origOut;
    },
  };
}

async function runCaptured(positional: string[], opts: GlobalOptions) {
  const cap = captureOutput();
  try {
    const code = await runTools(positional, opts);
    return { code, stdout: cap.stdout() };
  } finally {
    cap.restore();
  }
}

let tmpHome: string;
let prevHome: string | undefined;
let prevGdaHome: string | undefined;

beforeEach(() => {
  const base = process.env.GDA_TEST_TMPDIR ?? path.join(process.cwd(), "build", "cli-test-tmp");
  fs.mkdirSync(base, { recursive: true });
  tmpHome = fs.mkdtempSync(path.join(base, "gda-tools-test-"));
  prevHome = process.env.GODOT_AUTOPILOT_CLI_HOME;
  prevGdaHome = process.env.GODOT_AUTOPILOT_HOME;
  process.env.GODOT_AUTOPILOT_CLI_HOME = tmpHome;
  process.env.GODOT_AUTOPILOT_HOME = tmpHome;
});

afterEach(() => {
  if (prevHome === undefined) {
    delete process.env.GODOT_AUTOPILOT_CLI_HOME;
  } else {
    process.env.GODOT_AUTOPILOT_CLI_HOME = prevHome;
  }
  if (prevGdaHome === undefined) {
    delete process.env.GODOT_AUTOPILOT_HOME;
  } else {
    process.env.GODOT_AUTOPILOT_HOME = prevGdaHome;
  }
  fs.rmSync(tmpHome, { recursive: true, force: true });
});

async function runToolCaptured(positional: string[], opts: GlobalOptions) {
  const cap = captureOutput();
  try {
    const code = await runTool(positional, opts);
    return { code, stdout: cap.stdout() };
  } finally {
    cap.restore();
  }
}

const OFFLINE_TOOLS: NativeCatalogEntry[] = [
  {
    name: "scene_open",
    description: "Open a scene",
    category: "Scene",
    tags: ["node", "create"],
    aliases: ["open_scene"],
    mutating: true,
    high_risk: false,
  },
  {
    name: "script_read",
    description: "Read a script",
    category: "Script",
    tags: ["read"],
    aliases: [],
    mutating: false,
    high_risk: false,
  },
];

async function serveNativeCatalog(entries: NativeCatalogEntry[]) {
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
          params: [{ key: "result", value: JSON.stringify(entries) }],
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

function writePluginIndex(tmp: string, port: number): void {
  const dir = path.join(tmp, ".godot-autopilot");
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, "instances.json"), JSON.stringify({
    instances: [{
      instance_id: "catalog-1",
      port,
      pid: process.pid,
      project_path: path.join(tmp, "proj"),
      endpoints: {
        mcp: { host: "127.0.0.1", port, path: "/mcp", protocol: "mcp", auth: "none" },
        ipc: { kind: "tcp", host: "127.0.0.1", port, protocol: "mcp-jsonrpc", framing: "length-prefixed-json", auth: "token", version: 1 },
      },
      started_at: Date.now(),
      version: "0.3.5",
      port_source: "persisted",
    }],
  }));
}

describe("tool 子命令与离线 catalog", () => {
  test("离线 list 输出 source=offline 与 hash，过滤 category/tags", async () => {
    const doc = writeOfflineCatalog(OFFLINE_TOOLS);
    const result = await runToolCaptured(["list"], { offline: true, category: "Scene", tagsText: "node,create" });
    expect(result.code).toBe(ExitCode.Success);
    const body = JSON.parse(result.stdout);
    expect(body.source).toBe("offline");
    expect(body.hash).toBe(doc.hash);
    expect(body.tools).toEqual(["scene_open"]);
    expect(body.total).toBe(1);
  });

  test("离线 search/describe/categories 共用同一 catalog", async () => {
    const doc = writeOfflineCatalog(OFFLINE_TOOLS);
    const search = JSON.parse((await runToolCaptured(["search", "script"], { offline: true })).stdout);
    expect(search.source).toBe("offline");
    expect(search.results[0].name).toBe("script_read");

    const describe = JSON.parse((await runToolCaptured(["describe", "open_scene"], { offline: true })).stdout);
    expect(describe.found).toBe(true);
    expect(describe.name).toBe("scene_open");
    expect(describe.hash).toBe(doc.hash);

    const categories = JSON.parse((await runToolCaptured(["categories"], { offline: true })).stdout);
    expect(categories.categories).toEqual(["Scene", "Script"]);
  });

  test("catalog hash 被篡改时拒绝离线读取", async () => {
    writeOfflineCatalog(OFFLINE_TOOLS);
    const stored = JSON.parse(fs.readFileSync(catalogPath(), "utf8"));
    stored.tools[0].name = "tampered";
    fs.writeFileSync(catalogPath(), JSON.stringify(stored));
    let caught: unknown;
    try {
      await runTool(["list"], { offline: true });
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).message).toContain("hash mismatch");
    expect(makeCatalogDocument(stored.tools as NativeCatalogEntry[]).hash).not.toBe(stored.hash);
  });

  test("live catalog.list 不发送 MCP initialize 并写入离线缓存", async () => {
    const native = await serveNativeCatalog(OFFLINE_TOOLS);
    try {
      writePluginIndex(tmpHome, native.port);
      const result = await runToolCaptured(["list"], { portText: String(native.port) });
      expect(result.code).toBe(ExitCode.Success);
      const body = JSON.parse(result.stdout);
      expect(body.source).toBe("live");
      expect(body.tools).toContain("scene_open");
      expect(body.hash).toBe(readOfflineCatalog().hash);
      expect(native.requests.map((request) => request.method)).toContain("catalog.list");
      expect(native.requests.every((request) => request.kind === "request" && request.v === 1)).toBe(true);
      expect(native.requests.map((request) => request.method)).not.toContain("initialize");
    } finally {
      await native.close();
    }
  });

  test("未知 tool 子命令报用法错误", async () => {
    let caught: unknown;
    try {
      await runTool(["nope"], {});
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).message).toContain("unknown tool subcommand");
  });
});

describe("tools list (live)", () => {
  test("list 请求全量并过滤 5 个协议工具", async () => {
    const { port, requests } = recordingServer(() =>
      sseResponse(searchEnvelope(["create_scene_node", "ping", "batch_execute", "call_tool", "code_execute"])),
    );
    const result = await runCaptured(["list"], { portText: String(port) });
    expect(result.code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({
      name: "search_tools",
      arguments: { limit: 0 },
    });
    expect(JSON.parse(result.stdout)).toEqual({
      tools: ["create_scene_node", "batch_execute", "code_execute"],
      total: 3,
    });
  });

  test("list 传 category 与 tags", async () => {
    const { port, requests } = recordingServer(() =>
      sseResponse(searchEnvelope(["create_scene_node"])),
    );
    const result = await runCaptured(["list"], {
      portText: String(port),
      category: "Scene",
      tagsText: "node, ,create",
    });
    expect(result.code).toBe(ExitCode.Success);
    expect(requests[0].params).toEqual({
      name: "search_tools",
      arguments: { limit: 0, category: "Scene", tags: ["node", "create"] },
    });
  });

  test("tools search/describe 已删除", async () => {
    let caught: unknown;
    try {
      await runTools(["search", "spawn"], {});
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).code).toBe(ExitCode.Usage);
    expect((caught as CliError).message).toContain("unknown tools subcommand: search");
  });

  test("无 live 实例时报 live 错误而非离线 catalog", async () => {
    let caught: unknown;
    try {
      await runTools(["list"], {});
    } catch (error) {
      caught = error;
    }
    expect(caught).toBeInstanceOf(CliError);
    expect((caught as CliError).message).not.toContain("offline");
    expect((caught as CliError).message).not.toContain("catalog");
  });
});
