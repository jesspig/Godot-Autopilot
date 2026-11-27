import { afterAll, beforeAll, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import net from "node:net";
import { sendHandshake, startNativeServer, type NativeServer } from "./support/native-ipc";

const ROOT = path.resolve(fileURLToPath(new URL("../..", import.meta.url)));
const CLI_SCRIPT = path.join(ROOT, "cli", "bin", "gda.ts");
const BUN = Bun.which("bun") ?? process.execPath;
const CALL_TIMEOUT_MS = 30_000;
const TEST_TIMEOUT_MS = 30_000;
const TMP_ROOT = fs.mkdtempSync(path.join(os.tmpdir(), "gda-contract-"));
const PNG_B64 =
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQG" +
  "AhKmMIQAAAABJRU5ErkJggg==";
const SEARCH_RESULTS = [{ name: "mock_tool", score: 0.99 }];

interface MockState {
  initialized: boolean;
  methods: string[];
  lastSearchArgs: unknown;
  lastDirectName: unknown;
  lastWrappedParams: unknown;
}

const state: MockState = {
  initialized: false,
  methods: [],
  lastSearchArgs: undefined,
  lastDirectName: undefined,
  lastWrappedParams: undefined,
};

function errorEnvelope(_status: number, body: unknown): Record<string, unknown> {
  return body as Record<string, unknown>;
}

function frameEnvelope(envelope: Record<string, unknown>, _multiline = false): Record<string, unknown> {
  return envelope;
}

function okResponse(reqId: unknown, business: unknown): Record<string, unknown> {
  return frameEnvelope({
    jsonrpc: "2.0",
    id: reqId,
    result: { content: [{ type: "text", text: JSON.stringify(business) }] },
  });
}

function rpcErrorResponse(reqId: unknown, message: string): Record<string, unknown> {
  return frameEnvelope({
    jsonrpc: "2.0",
    id: reqId,
    error: { code: -32602, message },
  });
}

function routeDirect(reqId: unknown, name: unknown, args: Record<string, unknown>): Record<string, unknown> {
  if (name === "ping") {
    state.lastWrappedParams = { name: "call_tool", arguments: { name: "ping", arguments: args } };
    return routeWrapped(reqId, "ping", args);
  }
  if (name === "list_categories") return okResponse(reqId, { categories: ["mock-cat"] });
  if (name === "search_tools") {
    state.lastSearchArgs = args;
    return frameEnvelope(
      {
        jsonrpc: "2.0",
        id: reqId,
        result: {
          content: [
            { type: "text", text: JSON.stringify({ results: SEARCH_RESULTS }) },
          ],
        },
      },
      true,
    );
  }
  if (name === "get_tool_detail") {
    const tool = typeof args.name === "string" ? args.name : "mock_tool";
    return okResponse(reqId, {
      name: tool,
      description: "mock tool",
      inputSchema: { type: "object", properties: {} },
    });
  }
  if (name === "mock_503") {
    return errorEnvelope(503, { error: { code: -32603, message: "editor busy" } });
  }
  if (name === "mock_rpc_error") return rpcErrorResponse(reqId, "mock top-level JSON-RPC error");
  return okResponse(reqId, { error: { code: -32602, message: `unknown tool: ${String(name)}` } });
}

function routeWrapped(reqId: unknown, inner: unknown, innerArgs: unknown): Record<string, unknown> {
  if (inner === "ping") return okResponse(reqId, { pong: true });
  if (inner === "system_status") {
    return okResponse(reqId, { ok: true, engine: "mock", status: "ready" });
  }
  if (inner === "mock_echo") return okResponse(reqId, { echo: innerArgs });
  if (inner === "mock_image") {
    return frameEnvelope({
      jsonrpc: "2.0",
      id: reqId,
      result: { content: [{ type: "image", data: PNG_B64, mimeType: "image/png" }] },
    });
  }
  if (inner === "mock_503") {
    return errorEnvelope(503, { error: { code: -32603, message: "editor busy" } });
  }
  if (inner === "mock_rpc_error") return rpcErrorResponse(reqId, "mock top-level JSON-RPC error");
  return okResponse(reqId, { error: { code: -32602, message: `unknown tool: ${String(inner)}` } });
}

async function handle(message: Record<string, unknown>): Promise<Record<string, unknown> | null> {
  const body: unknown = message;
  if (body === null || typeof body !== "object" || Array.isArray(body)) {
    return errorEnvelope(400, { error: { code: -32600, message: "invalid request" } });
  }
  const record = body as Record<string, unknown>;
  const method = record.method;
  state.methods.push(typeof method === "string" ? method : String(method));
  const reqId = record.id;
  if (method === "initialize") {
    state.initialized = true;
    return frameEnvelope({
      jsonrpc: "2.0",
      id: reqId,
      result: {
        capabilities: { tools: {} },
        protocolVersion: "1",
        serverInfo: { name: "mock-gda", version: "0.3.7" },
      },
    });
  }
  if (method === "notifications/initialized") return null;
  if (method !== "tools/call") {
    return errorEnvelope(400, {
      jsonrpc: "2.0",
      id: reqId,
      error: { code: -32600, message: "bad method" },
    });
  }
  if (!state.initialized) {
    return errorEnvelope(400, {
      jsonrpc: "2.0",
      id: reqId,
      error: { code: -32600, message: "invalid request: tools/call" },
    });
  }
  const paramsRaw = record.params;
  const params: Record<string, unknown> =
    paramsRaw !== null && typeof paramsRaw === "object" && !Array.isArray(paramsRaw)
      ? (paramsRaw as Record<string, unknown>)
      : {};
  const argsRaw = params.arguments;
  const args: Record<string, unknown> =
    argsRaw !== null && typeof argsRaw === "object" && !Array.isArray(argsRaw)
      ? (argsRaw as Record<string, unknown>)
      : {};
  const name = params.name;
  if (name === "call_tool") {
    state.lastWrappedParams = Object.fromEntries(
      Object.entries(params).filter(([key]) => key !== "_meta"),
    );
    const innerName = args.name;
    const innerArgs = Object.prototype.hasOwnProperty.call(args, "arguments")
      ? args.arguments
      : {};
    return routeWrapped(reqId, innerName, innerArgs);
  }
  state.lastDirectName = name;
  return routeDirect(reqId, name, args);
}

let server: NativeServer | undefined;
let port = 0;

beforeAll(async () => {
  server = await startNativeServer(async (message, _socket, send) => {
    const response = await handle(message as Record<string, unknown>);
    if (response !== null) send({ ...response, id: message.id });
  });
  port = server.port;
});

afterAll(async () => {
  await server?.close();
  try {
    fs.rmSync(TMP_ROOT, { recursive: true, force: true });
  } catch {
    return;
  }
});

beforeEach(() => {
  state.lastSearchArgs = undefined;
  state.lastDirectName = undefined;
  state.lastWrappedParams = undefined;
});

interface RunOptions {
  envPort?: number;
  timeoutMs?: number;
}

interface RunResult {
  status: number;
  stdout: string;
  stderr: string;
}

function childEnv(options: RunOptions): Record<string, string> {
  const env: Record<string, string> = {};
  for (const [key, value] of Object.entries(process.env)) {
    if (value !== undefined) env[key] = value;
  }
  delete env.GODOT_AUTOPILOT_PORT;
  if (options.envPort !== undefined) env.GODOT_AUTOPILOT_PORT = String(options.envPort);
  env.GODOT_AUTOPILOT_HOME = TMP_ROOT;
  env.GODOT_AUTOPILOT_USERDATA = TMP_ROOT;
  env.GODOT_AUTOPILOT_CLI_HOME = path.join(TMP_ROOT, "cli");
  for (const key of [
    "HTTP_PROXY",
    "HTTPS_PROXY",
    "ALL_PROXY",
    "http_proxy",
    "https_proxy",
    "all_proxy",
  ]) {
    delete env[key];
  }
  env.NO_PROXY = "127.0.0.1,localhost";
  return env;
}

async function runCli(args: string[], options: RunOptions = {}): Promise<RunResult> {
  const timeoutMs = options.timeoutMs ?? CALL_TIMEOUT_MS;
  const proc = Bun.spawn([BUN, CLI_SCRIPT, ...args], {
    cwd: ROOT,
    env: childEnv(options),
    stdin: "ignore",
    stdout: "pipe",
    stderr: "pipe",
  });
  let timedOut = false;
  const timer = setTimeout(() => {
    timedOut = true;
    try {
      proc.kill();
    } catch {
      return;
    }
  }, timeoutMs);
  const [stdout, stderr, status] = (await Promise.all([
    new Response(proc.stdout).text(),
    new Response(proc.stderr).text(),
    proc.exited,
  ])) as [string, string, number];
  clearTimeout(timer);
  if (timedOut) throw new Error(`CLI call timed out after ${timeoutMs} ms: ${args.join(" ")}`);
  return { status, stdout, stderr };
}

async function cliRun(args: string[], options: RunOptions = {}): Promise<RunResult> {
  return runCli([...args, "--port", String(port)], options);
}

async function freePort(): Promise<number> {
  const probe = net.createServer();
  await new Promise<void>((resolve) => probe.listen(0, "127.0.0.1", resolve));
  const found = (probe.address() as net.AddressInfo).port;
  await new Promise<void>((resolve) => probe.close(() => resolve()));
  return found;
}

function parseJson(text: string): unknown {
  try {
    return JSON.parse(text);
  } catch (error) {
    throw new Error(`stdout is not JSON (${(error as Error).message}): ${text.slice(-500)}`);
  }
}

function checkStatus(result: RunResult, status: number): void {
  if (result.status !== status) {
    throw new Error(
      `expected exit ${status}, got ${result.status}; stdout=${result.stdout.slice(-300)} stderr=${result.stderr.slice(-500)}`,
    );
  }
}

describe("CLI native IPC contract", () => {
  test(
    "ping: direct tools/call name=ping, compact stdout",
    async () => {
      state.initialized = false;
      state.methods = [];
      const result = await cliRun(["ping"]);
      checkStatus(result, 0);
      expect((parseJson(result.stdout) as { data: unknown }).data).toEqual({ pong: true });
      expect(result.stdout.trim()).not.toContain("\n");
      expect(state.lastDirectName).toBe("ping");
      expect(state.lastWrappedParams).toEqual({
        name: "call_tool",
        arguments: { name: "ping", arguments: {} },
      });
      expect(state.methods).toEqual([
        "initialize",
        "notifications/initialized",
        "tools/call",
      ]);
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "status: rich status body wraps system_status",
    async () => {
      const result = await cliRun(["status"]);
      checkStatus(result, 0);
      const body = parseJson(result.stdout) as {
        ok: boolean;
        command: string;
        data: { server: Record<string, unknown> };
      };
      expect(body.ok).toBe(true);
      expect(body.command).toBe("editor.status");
      expect(body.data.server).toEqual({ ok: true, engine: "mock", status: "ready" });
      expect(state.lastWrappedParams).toEqual({
        name: "call_tool",
        arguments: { name: "system_status", arguments: {} },
      });
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "categories: direct meta tool",
    async () => {
      const result = await cliRun(["categories"]);
      checkStatus(result, 0);
      expect((parseJson(result.stdout) as { data: unknown }).data).toEqual({ categories: ["mock-cat"] });
      expect(state.lastDirectName).toBe("list_categories");
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "search: projection of tools/call payload",
    async () => {
      const result = await cliRun(["search", "q"]);
      checkStatus(result, 0);
      expect((parseJson(result.stdout) as { data: unknown }).data).toEqual({ results: SEARCH_RESULTS });
      expect(state.lastDirectName).toBe("search_tools");
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "search filters: category and tags forwarded",
    async () => {
      const result = await cliRun(["search", "mock", "--category", "mock-cat", "--tags", "a,b"]);
      checkStatus(result, 0);
      expect((parseJson(result.stdout) as { data: unknown }).data).toEqual({ results: SEARCH_RESULTS });
      const seen = state.lastSearchArgs as Record<string, unknown> | undefined;
      expect(seen?.category).toBe("mock-cat");
      expect(seen?.tags).toEqual(["a", "b"]);
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "describe: direct get_tool_detail",
    async () => {
      const result = await cliRun(["describe", "mock_tool"]);
      checkStatus(result, 0);
      const body = parseJson(result.stdout) as { data: Record<string, unknown> };
      expect(body.data.name).toBe("mock_tool");
      expect("inputSchema" in body.data).toBe(true);
      expect(state.lastDirectName).toBe("get_tool_detail");
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "call: wraps call_tool and ignores traceparent _meta",
    async () => {
      const result = await cliRun(["call", "mock_echo", "--args", '{"k":1}']);
      checkStatus(result, 0);
      expect((parseJson(result.stdout) as { data: unknown }).data).toEqual({ echo: { k: 1 } });
      expect(state.lastWrappedParams).toEqual({
        name: "call_tool",
        arguments: { name: "mock_echo", arguments: { k: 1 } },
      });
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "call: --args and --args-file are equivalent",
    async () => {
      const payload = { x: 1, s: "hi" };
      const argsFile = path.join(TMP_ROOT, "args.json");
      fs.writeFileSync(argsFile, JSON.stringify(payload));
      const first = await cliRun(["call", "mock_echo", "--args", JSON.stringify(payload)]);
      const second = await cliRun(["call", "mock_echo", "--args-file", argsFile]);
      checkStatus(first, 0);
      checkStatus(second, 0);
      expect((parseJson(first.stdout) as { data: unknown }).data).toEqual({ echo: payload });
      expect((parseJson(second.stdout) as { data: unknown }).data).toEqual({ echo: payload });
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "call: meta tool goes direct",
    async () => {
      const result = await cliRun(["call", "ping"]);
      checkStatus(result, 0);
      expect((parseJson(result.stdout) as { data: unknown }).data).toEqual({ pong: true });
      expect(state.lastDirectName).toBe("ping");
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "tool-level failure exits 1",
    async () => {
      const result = await cliRun(["call", "no_such_tool_xyz"]);
      checkStatus(result, 1);
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "top-level JSON-RPC error exits 1 and surfaces the message",
    async () => {
      const result = await cliRun(["call", "mock_rpc_error"]);
      checkStatus(result, 1);
      expect(result.stderr).toContain("mock top-level JSON-RPC error");
      const body = parseJson(result.stdout) as {
        ok: boolean;
        data: unknown;
        error: { code: string };
      };
      expect(body.ok).toBe(false);
      expect(body.data).toBeNull();
      expect(body.error.code).toBe("TOOL_FAILURE");
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "native IPC disconnect exits 2",
    async () => {
      const local = await startNativeServer((message, socket) => {
        if (message.method === "initialize") {
          sendHandshake(message, () => {});
          socket.destroy();
        }
      });
      try {
        const result = await runCli(["--port", String(local.port), "ping"]);
        checkStatus(result, 2);
        expect(result.stderr).toContain("IPC");
      } finally {
        await local.close();
      }
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "connection refused exits 2",
    async () => {
      const closed = await freePort();
      const result = await runCli(["--port", String(closed), "ping"]);
      checkStatus(result, 2);
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "usage errors exit 3",
    async () => {
      for (const args of [["describe"], ["call"], ["boguscmd"]]) {
        const result = await runCli(args);
        checkStatus(result, 3);
      }
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "image payload is saved without leaking base64",
    async () => {
      const imageDir = fs.mkdtempSync(path.join(TMP_ROOT, "images-"));
      const result = await cliRun(["call", "mock_image", "--save-images", imageDir]);
      checkStatus(result, 0);
      expect(result.stdout).not.toContain(PNG_B64.slice(0, 40));
      const body = parseJson(result.stdout) as {
        data: { images?: Array<{ saved?: boolean }> };
      };
      expect(Array.isArray(body.data.images)).toBe(true);
      expect(body.data.images?.[0]?.saved).toBe(true);
      const pngs = fs.readdirSync(imageDir).filter((name) => name.endsWith(".png"));
      expect(pngs).toHaveLength(1);
      const magic = fs.readFileSync(path.join(imageDir, pngs[0])).subarray(0, 8);
      const expected = Buffer.from(PNG_B64, "base64").subarray(0, 8);
      expect(magic.equals(expected)).toBe(true);
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "--port wins over GODOT_AUTOPILOT_PORT",
    async () => {
      const closed = await freePort();
      const result = await runCli(["--port", String(port), "ping"], { envPort: closed });
      checkStatus(result, 0);
      expect(result.stdout).toContain("pong");
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "GODOT_AUTOPILOT_PORT is used when --port is absent",
    async () => {
      const result = await runCli(["ping"], { envPort: port });
      checkStatus(result, 0);
      expect(result.stdout).toContain("pong");
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "pretty output indents but keeps the same payload",
    async () => {
      const result = await cliRun(["ping", "--pretty"]);
      checkStatus(result, 0);
      expect(result.stdout.trim().split(/\r?\n/).length).toBeGreaterThan(1);
      expect(result.stdout).toContain('  "pong": true');
      expect((parseJson(result.stdout) as { data: unknown }).data).toEqual({ pong: true });
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "invalid JSON frame exits 2",
    async () => {
      const local = await startNativeServer((message, socket, send) => {
        if (message.method === "initialize") {
          sendHandshake(message, send);
          return;
        }
        const payload = Buffer.from("{not-json}", "utf8");
        const header = Buffer.alloc(4);
        header.writeUInt32BE(payload.length, 0);
        socket.write(Buffer.concat([header, payload]));
      });
      try {
        const result = await runCli(["--port", String(local.port), "ping"]);
        checkStatus(result, 2);
        expect(result.stderr).toContain("not valid JSON");
      } finally {
        await local.close();
      }
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "client timeout exits 2",
    async () => {
      const local = await startNativeServer(async (message, _socket, send) => {
        if (message.method === "initialize") {
          sendHandshake(message, send);
          return;
        }
        await Bun.sleep(300);
        send({ jsonrpc: "2.0", id: message.id, result: { content: [] } });
      });
      try {
        const result = await runCli(["--port", String(local.port), "ping", "--timeout", "0.05"]);
        checkStatus(result, 2);
        expect(result.stderr).toContain("timed out");
      } finally {
        await local.close();
      }
    },
    TEST_TIMEOUT_MS,
  );
});