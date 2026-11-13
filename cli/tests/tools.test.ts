import fs from "node:fs";
import path from "node:path";

import { afterEach, beforeEach, describe, expect, test } from "bun:test";

import type { GlobalOptions } from "../src/args";
import { run as runTools } from "../src/commands/tools";
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
