import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { spawn, spawnSync } from "node:child_process";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import path from "node:path";

import type { GlobalOptions } from "../src/args";
import { findSpec } from "../src/command-spec";
import { run as runGame } from "../src/commands/game";
import { run as runResource } from "../src/commands/resource";
import { runGenericToolCall, run as runScene } from "../src/commands/scene";
import { run as runScript } from "../src/commands/script";
import { CliError, ExitCode } from "../src/errors";

interface NativeResponse {
  status?: string;
  error?: number;
  error_message?: string;
  confirmation?: unknown;
  result?: unknown;
}

interface FakeNative {
  port: number;
  requests: Array<Record<string, unknown>>;
  close(): Promise<void>;
}

async function serveNative(handler: (request: Record<string, unknown>) => NativeResponse): Promise<FakeNative> {
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
        const answer = handler(request) ?? {};
        const params = Array.isArray(request.params) ? request.params as Array<{ key: string; value: string }> : [];
        const fromRequest = (key: string): string => params.find((entry) => entry.key === key)?.value ?? "";
        const body = Buffer.from(JSON.stringify({
          v: 1,
          kind: "response",
          id: `r-${String(request.id)}`,
          instance_id: String(request.instance_id ?? ""),
          method: String(request.method ?? ""),
          params: [{ key: "result", value: JSON.stringify(answer.result ?? {}) }],
          status: answer.status ?? "ok",
          error: answer.error ?? 0,
          error_message: answer.error_message ?? "",
          confirmation: answer.confirmation ?? undefined,
          task_id: fromRequest("task_id") || undefined,
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
      instance_id: "native-domain",
      port,
      pid: process.pid,
      project_path: path.join(tmp, "proj"),
      endpoints: {
        ipc: { kind: "tcp", host: "127.0.0.1", port, protocol: "mcp-jsonrpc", framing: "length-prefixed-json", auth: "token", version: 1 },
      },
      started_at: Date.now(),
      version: "0.3.5",
      port_source: "persisted",
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

function invocation(payload: unknown): Record<string, unknown> {
  return {
    status: "ok",
    payload,
    artifact_id: "artifact-1",
    digest: "digest-1",
    bytes: 32,
    task_id: "",
  };
}

function gcpParams(request: Record<string, unknown>): Record<string, string> {
  const params = Array.isArray(request.params) ? request.params as Array<{ key: string; value: string }> : [];
  const out: Record<string, string> = {};
  for (const entry of params) out[entry.key] = entry.value;
  return out;
}

let tmp: string;
let prevCliHome: string | undefined;
let prevGdaHome: string | undefined;
let native: FakeNative | undefined;

beforeEach(() => {
  tmp = fs.mkdtempSync(path.join(os.tmpdir(), "gda-domain-test-"));
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

async function withNative(handler: (request: Record<string, unknown>) => NativeResponse) {
  native = await serveNative(handler);
  writePluginIndex(tmp, native.port);
  return { port: native.port, opts: { portText: String(native.port) } as GlobalOptions, requests: native.requests };
}

async function spawnCli(args: string[], home: string): Promise<{ status: number | null; stdout: string }> {
  return new Promise((resolve) => {
    const child = spawn(process.execPath, ["cli/bin/gda.ts", ...args], {
      cwd: path.resolve(import.meta.dir, "..", ".."),
      env: { ...process.env, GODOT_AUTOPILOT_CLI_HOME: home, GODOT_AUTOPILOT_HOME: home },
    });
    let stdout = "";
    child.stdout?.on("data", (chunk) => { stdout += chunk.toString(); });
    child.on("close", (status) => resolve({ status, stdout }));
  });
}

describe("T-09 领域命令映射", () => {
  test("scene.property 映射 property_set 并返回 scene_path/revision/receipt", async () => {
    const ctx = await withNative(() => ({
      result: invocation({ ok: true, scene_path: "res://main.tscn", revision: "rev-9", new_errors_since_last_call: 0 }),
    }));
    const { code, stdout } = await captureRun(() => runScene(
      ["property", "Root/Label", "text", '"hello"'], ctx.opts,
    ));
    expect(code).toBe(ExitCode.Success);
    const body = JSON.parse(stdout);
    expect(body.command).toBe("scene.property");
    expect(body.data.tool).toBe("property_set");
    expect(body.data.scene_path).toBe("res://main.tscn");
    expect(body.data.revision).toBe("rev-9");
    expect(body.data.receipt.request_id).toBeString();
    expect(body.data.receipt.digest).toBe("digest-1");
    const start = ctx.requests.find((request) => request.method === "call.start");
    expect(start).toBeDefined();
    const params = gcpParams(start!);
    expect(params.tool).toBe("property_set");
    expect(params.path).toBe("Root/Label");
    expect(params.property).toBe("text");
    expect(params.value).toBe("hello");
  });

  test("scene.tree 与 scene.verify/diff 映射 get_scene_tree 并强制验证标记", async () => {
    const ctx = await withNative(() => ({ result: invocation({ ok: true }) }));
    await captureRun(() => runScene(["tree"], {
      ...ctx.opts, "--max-depth": 4, "--include-hash": true,
    } as GlobalOptions));
    const treeParams = gcpParams(ctx.requests.find((request) => request.method === "call.start")!);
    expect(treeParams.tool).toBe("get_scene_tree");
    expect(treeParams.max_depth).toBe("4");
    expect(treeParams.include_hash).toBe("true");
    expect(treeParams.verify).toBeUndefined();

    ctx.requests.length = 0;
    await captureRun(() => runScene(["verify"], ctx.opts));
    await captureRun(() => runScene(["diff"], ctx.opts));
    const verify = ctx.requests.filter((request) => request.method === "call.start").map((request) => gcpParams(request));
    expect(verify[0].verify).toBe("true");
    expect(verify[1].verify).toBe("true");
    expect(verify[1].include_hash).toBe("true");
  });

  test("scene.signal --disconnect 切换工具名", async () => {
    const ctx = await withNative(() => ({ result: invocation({ ok: true }) }));
    await captureRun(() => runScene(["signal"], {
      ...ctx.opts,
      "--from": "Root/Button",
      "--signal": "pressed",
      "--to": "Root",
      "--method": "on_pressed",
      "--disconnect": true,
    } as GlobalOptions));
    const params = gcpParams(ctx.requests.find((request) => request.method === "call.start")!);
    expect(params.tool).toBe("signal_disconnect");
    expect(params.source_path).toBe("Root/Button");
    expect(params.signal).toBe("pressed");
  });

  test("script.create/patch/compile/attach/reload/test 映射到真实工具", async () => {
    const cases: Array<{ sub: string; flags?: Record<string, unknown>; positional?: string[]; tool: string; check?: (params: Record<string, string>) => void }> = [
      { sub: "create", positional: ["res://x.gd"], flags: { "--extends": "Node" }, tool: "create_script" },
      { sub: "patch", flags: { "--path": "res://x.gd", "--dry-run": true }, tool: "patch_script", check: (p) => expect(p.dry_run).toBe("true") },
      { sub: "compile", flags: { "--path": "res://x.gd" }, tool: "patch_script", check: (p) => expect(p.dry_run).toBe("true") },
      { sub: "attach", flags: { "--node": "Root", "--script": "res://x.gd" }, tool: "attach_script_to_node" },
      { sub: "reload", flags: { "--path": "res://x.gd", "--keep-state": true }, tool: "reload_script", check: (p) => expect(p.keep_state).toBe("true") },
      { sub: "test", flags: { "--directory": "res://tests" }, tool: "run_gdscript_tests" },
    ];
    for (const entry of cases) {
      const ctx = await withNative(() => ({ result: invocation({ ok: true }) }));
      const { code } = await captureRun(() => runScript(
        [entry.sub, ...(entry.positional ?? [])], { ...ctx.opts, ...(entry.flags ?? {}) } as GlobalOptions,
      ));
      expect(code).toBe(ExitCode.Success);
      const params = gcpParams(ctx.requests.find((request) => request.method === "call.start")!);
      expect(params.tool).toBe(entry.tool);
      entry.check?.(params);
      await native!.close();
      native = undefined;
    }
  });

  test("resource 读写与事务命令映射", async () => {
    const cases: Array<{ sub: string; flags: Record<string, unknown>; tool: string; action?: string }> = [
      { sub: "inspect", flags: { "--path": "res://a.tres" }, tool: "resource_query", action: "type" },
      { sub: "get", flags: { "--path": "res://a.tres", "--property": "x" }, tool: "resource_property", action: "get" },
      { sub: "set", flags: { "--path": "res://a.tres", "--property": "x", "--value": "3" }, tool: "resource_property", action: "set" },
      { sub: "save", flags: { "--path": "a", "--dest": "res://b.tres" }, tool: "resource_save", action: "save" },
      { sub: "import", flags: { "--path": "res://a.png" }, tool: "resource_import", action: "reimport" },
      { sub: "references", flags: { "--path": "res://a.tres" }, tool: "resource_query", action: "references" },
    ];
    for (const entry of cases) {
      const ctx = await withNative(() => ({ result: invocation({ ok: true }) }));
      const { code } = await captureRun(() => runResource(
        [entry.sub], { ...ctx.opts, ...entry.flags } as GlobalOptions,
      ));
      expect(code).toBe(ExitCode.Success);
      const params = gcpParams(ctx.requests.find((request) => request.method === "call.start")!);
      expect(params.tool).toBe(entry.tool);
      if (entry.action !== undefined) expect(params.action).toBe(entry.action);
      await native!.close();
      native = undefined;
    }
  });

  test("game 运行时命令映射与只读命令不需确认", async () => {
    const cases: Array<{ sub: string; flags: Record<string, unknown>; tool: string; action: string; confirm: boolean }> = [
      { sub: "status", flags: {}, tool: "get_game_state", action: "status", confirm: false },
      { sub: "capture", flags: { "--target": "game" }, tool: "capture_editor_viewport", action: "", confirm: false },
      { sub: "input", flags: { "--type": "key", "--mode": "event", "--event": "{}", confirm: true }, tool: "game_input", action: "queue", confirm: true },
      { sub: "eval", flags: { "--source": "return 1", confirm: true }, tool: "game_script", action: "execute", confirm: true },
      { sub: "scenario", flags: { argsJson: '{"events":[]}', confirm: true }, tool: "game_input", action: "sequence", confirm: true },
    ];
    for (const entry of cases) {
      const ctx = await withNative(() => ({ result: invocation({ ok: true }) }));
      const { code } = await captureRun(() => runGame(
        [entry.sub], { ...ctx.opts, ...entry.flags } as GlobalOptions,
      ));
      expect(code).toBe(ExitCode.Success);
      const params = gcpParams(ctx.requests.find((request) => request.method === "call.start")!);
      expect(params.tool).toBe(entry.tool);
      if (entry.action !== "") expect(params.action).toBe(entry.action);
      expect(params.confirm === "true").toBe(entry.confirm);
      await native!.close();
      native = undefined;
    }
  });

  test("所有 T-09 spec 存在且 tools 为非空数组", () => {
    for (const name of [
      "scene.tree", "scene.node", "scene.property", "scene.signal", "scene.verify", "scene.diff", "scene.save",
      "script.create", "script.patch", "script.compile", "script.attach", "script.exec", "script.reload", "script.test",
      "resource.inspect", "resource.get", "resource.set", "resource.save", "resource.import", "resource.rename",
      "resource.move", "resource.references",
      "game.play", "game.stop", "game.status", "game.input", "game.capture", "game.eval", "game.scenario",
    ]) {
      const spec = findSpec(name);
      expect(spec).toBeDefined();
      expect(spec!.tools.length).toBeGreaterThan(0);
      expect(spec!.output.schema).toBe("gda.result.v1");
    }
  });

  test("复杂 JSON 可经 --args-file 文件与 stdin - 提供", async () => {
    const file = path.join(tmp, "args.json");
    fs.writeFileSync(file, JSON.stringify({ properties: { name: "Node2D" }, type: "Node2D" }));
    const ctx = await withNative(() => ({ result: invocation({ ok: true }) }));
    await captureRun(() => runScene(["node"], { ...ctx.opts, "--args-file": file } as GlobalOptions));
    const fileParams = gcpParams(ctx.requests.find((request) => request.method === "call.start")!);
    expect(JSON.parse(fileParams.properties)).toEqual({ name: "Node2D" });
    expect(fileParams.type).toBe("Node2D");

    ctx.requests.length = 0;
    const original = fs.readFileSync;
    (fs as unknown as Record<string, unknown>).readFileSync = ((fd: unknown, encoding: unknown) => {
      if (fd === 0) return JSON.stringify({ properties: { name: "FromStdin" } });
      return (original as (...args: unknown[]) => unknown)(fd, encoding);
    }) as typeof fs.readFileSync;
    try {
      await captureRun(() => runScene(["node"], { ...ctx.opts, "--args-file": "-" } as GlobalOptions));
    } finally {
      (fs as unknown as Record<string, unknown>).readFileSync = original;
    }
    const stdinParams = gcpParams(ctx.requests.find((request) => request.method === "call.start")!);
    expect(JSON.parse(stdinParams.properties)).toEqual({ name: "FromStdin" });
  });

  test("gda tool call 仍可经原生 call.start 访问 canonical 工具", async () => {
    const ctx = await withNative(() => ({ result: invocation({ ok: true, tool: "get_scene_tree" }) }));
    const { code, stdout } = await captureRun(() => runGenericToolCall("get_scene_tree", {
      ...ctx.opts,
      argsJson: '{"max_depth":2}',
    }));
    expect(code).toBe(ExitCode.Success);
    const params = gcpParams(ctx.requests.find((request) => request.method === "call.start")!);
    expect(params.tool).toBe("get_scene_tree");
    expect(params.max_depth).toBe("2");
    expect(JSON.parse(stdout).command).toBe("tool.get_scene_tree");
  });

  test("CLI 端到端: gda scene tree 经领域路由到原生 call.start", async () => {
    const ctx = await withNative(() => ({ result: invocation({ ok: true }) }));
    const result = await spawnCli(
      ["scene", "tree", "--port", String(ctx.port)], tmp,
    );
    expect(result.status).toBe(ExitCode.Success);
    expect(JSON.parse(result.stdout).data.tool).toBe("get_scene_tree");
    const params = gcpParams(ctx.requests.find((request) => request.method === "call.start")!);
    expect(params.tool).toBe("get_scene_tree");
  });

  test("CLI 端到端: gda tool call 可达未提升工具", async () => {
    const ctx = await withNative(() => ({ result: invocation({ ok: true }) }));
    const result = await spawnCli(
      ["tool", "call", "physics_query", "--args", '{"action":"ray"}', "--port", String(ctx.port)], tmp,
    );
    expect(result.status).toBe(ExitCode.Success);
    const params = gcpParams(ctx.requests.find((request) => request.method === "call.start")!);
    expect(params.tool).toBe("physics_query");
    expect(params.action).toBe("ray");
  });

  test("CLI 端到端: 高危命令缺 --confirm 时不建立原生连接", async () => {
    const ctx = await withNative(() => ({ result: invocation({ ok: true }) }));
    const result = spawnSync(process.execPath, [
      "cli/bin/gda.ts", "game", "eval", "--source", "return 1", "--port", String(ctx.port),
    ], {
      cwd: path.resolve(import.meta.dir, "..", ".."),
      env: { ...process.env, GODOT_AUTOPILOT_CLI_HOME: tmp, GODOT_AUTOPILOT_HOME: tmp },
      encoding: "utf8",
    });
    expect(result.status).toBe(ExitCode.ConfirmationRequired);
    expect(result.stderr).toContain("--confirm");
    expect(ctx.requests).toHaveLength(0);
  });
});

describe("T-09 统一高危确认", () => {
  const highRisk: Array<{ sub: string; flags: Record<string, unknown>; run: (a: string[], o: GlobalOptions) => Promise<number> }> = [
    { sub: "exec", flags: { "--source": '"return 1"' }, run: runScript },
    { sub: "rename", flags: { "--path": "res://a.tres", "--to": "res://b.tres" }, run: runResource },
    { sub: "move", flags: { "--path": "res://a.tres", "--to": "res://sub" }, run: runResource },
    { sub: "eval", flags: { "--source": "return 1" }, run: runGame },
    { sub: "play", flags: {}, run: runGame },
    { sub: "stop", flags: {}, run: runGame },
    { sub: "input", flags: { "--type": "key" }, run: runGame },
    { sub: "scenario", flags: {}, run: runGame },
  ];

  test("高危命令缺 --confirm 不发送任何原生调用", async () => {
    for (const entry of highRisk) {
      const ctx = await withNative(() => ({ result: invocation({ ok: true }) }));
      const result = await captureRun(() => entry.run(
        [entry.sub], { ...ctx.opts, ...entry.flags } as GlobalOptions,
      ));
      expect(result.error).toBeInstanceOf(CliError);
      expect(result.error!.code).toBe(ExitCode.ConfirmationRequired);
      expect(result.error!.message).toContain("--confirm");
      expect(ctx.requests).toHaveLength(0);
      await native!.close();
      native = undefined;
    }
  });

  test("高危命令带 --confirm 时执行并携带 confirm=true", async () => {
    const ctx = await withNative(() => ({ result: invocation({ ok: true }) }));
    const { code } = await captureRun(() => runResource(["rename"], {
      ...ctx.opts, "--path": "res://a.tres", "--to": "res://b.tres", confirm: true,
    } as GlobalOptions));
    expect(code).toBe(ExitCode.Success);
    const params = gcpParams(ctx.requests.find((request) => request.method === "call.start")!);
    expect(params.tool).toBe("resource_file");
    expect(params.action).toBe("rename");
    expect(params.confirm).toBe("true");
  });

  test("服务器再次要求确认且客户端未确认时返回工具失败", async () => {
    const ctx = await withNative(() => ({
      status: "needs_confirmation",
      error: 13,
      error_message: "confirmation required",
      confirmation: { ttl_ms: 1000, issued_at_ms: 0, param_digest: "d", nonce: "n" },
    }));
    const result = await captureRun(() => runScene(
      ["property", "Root", "x", "1"], ctx.opts,
    ));
    expect(result.error).toBeInstanceOf(CliError);
    expect(result.error!.code).toBe(ExitCode.ConfirmationRequired);
    expect(ctx.requests.filter((request) => request.method === "call.confirm")).toHaveLength(0);
  });
});
