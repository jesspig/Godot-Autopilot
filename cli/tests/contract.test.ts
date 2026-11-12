import { afterAll, beforeAll, beforeEach, describe, expect, test } from "bun:test";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

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

function jsonResponse(status: number, body: unknown): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

function sseResponse(envelope: unknown, multiline = false): Response {
  let frame: string;
  if (multiline) {
    const record = envelope as { id?: unknown; result?: unknown };
    const head = `{"jsonrpc": "2.0", "id": ${JSON.stringify(record.id)},`;
    const tail = `"result": ${JSON.stringify(record.result)}}`;
    frame = `event: message\ndata: ${head}\ndata: ${tail}\n\n`;
  } else {
    frame = `event: message\ndata: ${JSON.stringify(envelope)}\n\n`;
  }
  return new Response(frame, { headers: { "content-type": "text/event-stream" } });
}

function okResponse(reqId: unknown, business: unknown): Response {
  return sseResponse({
    jsonrpc: "2.0",
    id: reqId,
    result: { content: [{ type: "text", text: JSON.stringify(business) }] },
  });
}

function rpcErrorResponse(reqId: unknown, message: string): Response {
  return sseResponse({
    jsonrpc: "2.0",
    id: reqId,
    error: { code: -32602, message },
  });
}

function routeDirect(reqId: unknown, name: unknown, args: Record<string, unknown>): Response {
  if (name === "ping") return okResponse(reqId, { pong: true });
  if (name === "list_categories") return okResponse(reqId, { categories: ["mock-cat"] });
  if (name === "search_tools") {
    state.lastSearchArgs = args;
    return sseResponse(
      {
        jsonrpc: "2.0",
        id: reqId,
        result: {
          content: [
            { type: "text", text: JSON.stringify({ result: { results: SEARCH_RESULTS } }) },
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
    return jsonResponse(503, { error: { code: -32603, message: "editor busy" } });
  }
  if (name === "mock_rpc_error") return rpcErrorResponse(reqId, "mock top-level JSON-RPC error");
  return okResponse(reqId, { error: { code: -32602, message: `unknown tool: ${String(name)}` } });
}

function routeWrapped(reqId: unknown, inner: unknown, innerArgs: unknown): Response {
  if (inner === "system_status") {
    return okResponse(reqId, { ok: true, engine: "mock", status: "ready" });
  }
  if (inner === "mock_echo") return okResponse(reqId, { echo: innerArgs });
  if (inner === "mock_image") {
    return sseResponse({
      jsonrpc: "2.0",
      id: reqId,
      result: { content: [{ type: "image", data: PNG_B64, mimeType: "image/png" }] },
    });
  }
  if (inner === "mock_503") {
    return jsonResponse(503, { error: { code: -32603, message: "editor busy" } });
  }
  if (inner === "mock_rpc_error") return rpcErrorResponse(reqId, "mock top-level JSON-RPC error");
  return okResponse(reqId, { error: { code: -32602, message: `unknown tool: ${String(inner)}` } });
}

async function handle(req: Request): Promise<Response> {
  if (req.method === "GET") {
    return jsonResponse(405, { error: { code: -32600, message: "GET not supported" } });
  }
  const url = new URL(req.url);
  if (url.pathname !== "/mcp") {
    return jsonResponse(404, { error: { code: -32601, message: "not found" } });
  }
  if (req.method !== "POST") {
    return jsonResponse(405, { error: { code: -32600, message: "method not allowed" } });
  }
  let body: unknown;
  try {
    const text = await req.text();
    body = text === "" ? {} : JSON.parse(text);
  } catch {
    return jsonResponse(400, { error: { code: -32700, message: "parse error" } });
  }
  if (body === null || typeof body !== "object" || Array.isArray(body)) {
    return jsonResponse(400, { error: { code: -32600, message: "invalid request" } });
  }
  const record = body as Record<string, unknown>;
  const method = record.method;
  state.methods.push(typeof method === "string" ? method : String(method));
  const reqId = record.id;
  if (method === "initialize") {
    state.initialized = true;
    return sseResponse({
      jsonrpc: "2.0",
      id: reqId,
      result: {
        capabilities: { tools: {} },
        protocolVersion: "2025-11-25",
        serverInfo: { name: "mock-gda", version: "0.3.7" },
      },
    });
  }
  if (method === "notifications/initialized") {
    return new Response("{}", { status: 202, headers: { "content-type": "application/json" } });
  }
  if (method !== "tools/call") {
    return jsonResponse(400, {
      jsonrpc: "2.0",
      id: reqId,
      error: { code: -32600, message: "bad method" },
    });
  }
  if (!state.initialized) {
    return jsonResponse(400, {
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

let server: ReturnType<typeof Bun.serve> | undefined;
let port = 0;

beforeAll(() => {
  server = Bun.serve({ hostname: "127.0.0.1", port: 0, fetch: handle });
  port = server.port as number;
});

afterAll(() => {
  server?.stop(true);
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

function freePort(): number {
  const probe = Bun.serve({ hostname: "127.0.0.1", port: 0, fetch: () => new Response("x") });
  const found = probe.port as number;
  probe.stop(true);
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

describe("CLI MCP contract", () => {
  test(
    "ping: lazy handshake, direct meta tool, compact stdout",
    async () => {
      state.initialized = false;
      state.methods = [];
      const result = await cliRun(["ping"]);
      checkStatus(result, 0);
      expect(parseJson(result.stdout)).toEqual({ pong: true });
      expect(result.stdout.trim()).not.toContain("\n");
      expect(state.lastDirectName).toBe("ping");
      expect(state.methods).toEqual([
        "tools/call",
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
      const body = parseJson(result.stdout) as Record<string, unknown>;
      expect(body.ok).toBe(true);
      expect(body.server).toEqual({ ok: true, engine: "mock", status: "ready" });
      expect(body.mcp_url).toBe(`http://127.0.0.1:${port}/mcp`);
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
      expect(parseJson(result.stdout)).toEqual({ categories: ["mock-cat"] });
      expect(state.lastDirectName).toBe("list_categories");
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "search: multiline SSE projection",
    async () => {
      const result = await cliRun(["search", "q"]);
      checkStatus(result, 0);
      expect(parseJson(result.stdout)).toEqual(SEARCH_RESULTS);
      expect(state.lastDirectName).toBe("search_tools");
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "search filters: category and tags forwarded",
    async () => {
      const result = await cliRun(["search", "mock", "--category", "mock-cat", "--tags", "a,b"]);
      checkStatus(result, 0);
      expect(parseJson(result.stdout)).toEqual(SEARCH_RESULTS);
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
      const body = parseJson(result.stdout) as Record<string, unknown>;
      expect(body.name).toBe("mock_tool");
      expect("inputSchema" in body).toBe(true);
      expect(state.lastDirectName).toBe("get_tool_detail");
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "call: wraps call_tool and ignores traceparent _meta",
    async () => {
      const result = await cliRun(["call", "mock_echo", "--args", '{"k":1}']);
      checkStatus(result, 0);
      expect(parseJson(result.stdout)).toEqual({ echo: { k: 1 } });
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
      expect(parseJson(first.stdout)).toEqual({ echo: payload });
      expect(parseJson(second.stdout)).toEqual({ echo: payload });
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "call: meta tool goes direct",
    async () => {
      const result = await cliRun(["call", "ping"]);
      checkStatus(result, 0);
      expect(parseJson(result.stdout)).toEqual({ pong: true });
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
      const body = parseJson(result.stdout) as { error?: { code?: number } };
      expect(body.error?.code).toBe(-32602);
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "HTTP 503 exits 2",
    async () => {
      const result = await cliRun(["call", "mock_503"]);
      checkStatus(result, 2);
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "connection refused exits 2",
    async () => {
      const closed = freePort();
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
      const body = parseJson(result.stdout) as { images?: Array<{ saved?: boolean }> };
      expect(Array.isArray(body.images)).toBe(true);
      expect(body.images?.[0]?.saved).toBe(true);
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
      const closed = freePort();
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
      expect(parseJson(result.stdout)).toEqual({ pong: true });
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "non-JSON 200 response exits 2",
    async () => {
      const local = Bun.serve({
        hostname: "127.0.0.1",
        port: 0,
        fetch: () =>
          new Response("<html>oops</html>", {
            status: 200,
            headers: { "content-type": "application/json" },
          }),
      });
      try {
        const result = await runCli(["--port", String(local.port), "ping"]);
        checkStatus(result, 2);
        expect(result.stderr).toContain("could not parse the plain-JSON response.");
      } finally {
        local.stop(true);
      }
    },
    TEST_TIMEOUT_MS,
  );

  test(
    "client timeout exits 2",
    async () => {
      const local = Bun.serve({
        hostname: "127.0.0.1",
        port: 0,
        fetch: async () => {
          await Bun.sleep(300);
          return new Response("{}", {
            status: 200,
            headers: { "content-type": "application/json" },
          });
        },
      });
      try {
        const result = await runCli(["--port", String(local.port), "ping", "--timeout", "0.05"]);
        checkStatus(result, 2);
        expect(result.stderr).toContain("request timed out.");
      } finally {
        local.stop(true);
      }
    },
    TEST_TIMEOUT_MS,
  );
});
