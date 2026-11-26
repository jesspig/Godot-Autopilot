#!/usr/bin/env bun
import { spawnSync } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

const EXPECTED_GODOT_VERSION = "4.7.2.stable.mono.official.ed1daf0bf";
const LIB_NAMES: Readonly<Record<string, string>> = {
  win32: "godot-autopilot.dll",
  linux: "libgodot-autopilot.so",
  darwin: "libgodot-autopilot.dylib",
};
const MAX_CAPTURE = 8000;
const START_TIMEOUT_MS = 150_000;

type Status = "pass" | "fail" | "skip" | "pending";

interface Options {
  godot: string;
  cli: string;
  cliRunner?: string;
  project: string;
  report: string;
  mode: "headless" | "gui";
  lib?: string;
  exportPreset?: string;
  timeoutSec: number;
}

interface CmdResult {
  args: string[];
  code: number | null;
  signal: string | null;
  stdout: string;
  stderr: string;
  error: string;
  durationMs: number;
}

interface Outcome {
  observed: string | Record<string, unknown>;
  ok: boolean | null;
  status?: Status;
  error?: string;
  stdout?: string;
  stderr?: string;
}

interface StepRecord {
  id: string;
  name: string;
  command: string;
  expected: string;
  observed: string | Record<string, unknown>;
  ok: boolean | null;
  status: Status;
  error: string;
  duration_ms: number;
  stdout: string;
  stderr: string;
}

interface Report {
  schema_version: number;
  task: string;
  generated_at: string;
  status: "pending" | "pass" | "fail";
  environment: Record<string, unknown>;
  summary: { total: number; passed: number; failed: number; skipped: number; pending: number };
  steps: StepRecord[];
  artifacts: Record<string, string>;
  warnings: string[];
  failure: string | null;
}

function parseArgs(argv: string[]): Options {
  const usage = "usage: smoke.ts --godot <path> --cli <path> [--cli-runner <exe>] --project <dir> [--lib <path>] [--report <file>] [--mode headless|gui] [--export-preset <name>] [--timeout <sec>]";
  const get = (name: string): string | undefined => {
    const prefix = `--${name}=`;
    const eq = argv.findIndex((arg) => arg.startsWith(prefix));
    if (eq >= 0) return argv[eq].slice(prefix.length);
    const index = argv.indexOf(`--${name}`);
    return index >= 0 ? argv[index + 1] : undefined;
  };
  const godot = get("godot");
  const cli = get("cli");
  const project = get("project");
  if (godot === undefined || cli === undefined || project === undefined) throw new Error(usage);
  const mode = get("mode") ?? "headless";
  if (mode !== "headless" && mode !== "gui") throw new Error("--mode must be headless or gui");
  const timeoutText = get("timeout") ?? "90";
  const timeoutSec = Number(timeoutText);
  if (!Number.isFinite(timeoutSec) || timeoutSec <= 0) throw new Error("--timeout must be a positive number");
  return {
    godot: path.resolve(godot),
    cli: path.resolve(cli),
    cliRunner: get("cli-runner") === undefined ? undefined : path.resolve(get("cli-runner") as string),
    project: path.resolve(project),
    report: path.resolve(get("report") ?? path.join("tests", "engine", "out", "engine-report.json")),
    mode,
    lib: get("lib") === undefined ? undefined : path.resolve(get("lib") as string),
    exportPreset: get("export-preset"),
    timeoutSec,
  };
}

function shorten(text: string): string {
  const clean = (text ?? "").replace(/\r\n/g, "\n").trim();
  return clean.length <= MAX_CAPTURE ? clean : `${clean.slice(0, MAX_CAPTURE)}...<truncated>`;
}

function runCmd(cmd: string, args: string[], timeoutMs: number): CmdResult {
  const started = Date.now();
  const result = spawnSync(cmd, args, {
    encoding: "utf8",
    timeout: timeoutMs,
    maxBuffer: 32 * 1024 * 1024,
    windowsHide: true,
  });
  return {
    args,
    code: result.status,
    signal: result.signal ?? null,
    stdout: result.stdout ?? "",
    stderr: result.stderr ?? "",
    error: result.error?.message ?? "",
    durationMs: Date.now() - started,
  };
}

function parseJson(text: string): Record<string, unknown> | null {
  const start = text.indexOf("{");
  const end = text.lastIndexOf("}");
  if (start < 0 || end <= start) return null;
  try {
    const value: unknown = JSON.parse(text.slice(start, end + 1));
    return value !== null && typeof value === "object" && !Array.isArray(value)
      ? value as Record<string, unknown>
      : null;
  } catch {
    return null;
  }
}

function field(value: unknown, ...keys: string[]): unknown {
  let current = value;
  for (const key of keys) {
    if (current === null || typeof current !== "object") return undefined;
    const record = current as Record<string, unknown>;
    if (record[key] === undefined && record.data !== null && typeof record.data === "object") {
      current = (record.data as Record<string, unknown>)[key];
    } else {
      current = record[key];
    }
  }
  return current;
}

function deepField(value: unknown, key: string, maxDepth = 5): unknown {
  if (maxDepth <= 0 || value === null || typeof value !== "object") return undefined;
  const record = value as Record<string, unknown>;
  if (record[key] !== undefined) return record[key];
  for (const child of Object.values(record)) {
    const found = deepField(child, key, maxDepth - 1);
    if (found !== undefined) return found;
  }
  return undefined;
}

function deepScalar(value: unknown, keys: string[], maxDepth = 5): unknown {
  for (const key of keys) {
    const found = deepField(value, key, maxDepth);
    if (found === undefined) continue;
    if (typeof found !== "object") return found;
    for (const candidate of keys) {
      const nested = deepField(found, candidate, maxDepth);
      if (nested !== undefined && typeof nested !== "object") return nested;
    }
  }
  return undefined;
}

function failText(result: CmdResult): string {
  const parts = [
    `exit=${result.code ?? "null"}`,
    result.signal === null ? "" : `signal=${result.signal}`,
    result.error,
    shorten(result.stderr),
  ].filter((part) => part !== "");
  return parts.join(" | ");
}

class Runner {
  readonly steps: StepRecord[] = [];
  readonly warnings: string[] = [];
  readonly artifacts: Record<string, string> = {};
  abort = false;

  constructor(readonly opts: Options, readonly report: Report) {}

  cli(args: string[], timeoutMs?: number): CmdResult {
    const fullArgs = [...args, "--project", this.opts.project];
    if (this.opts.cliRunner !== undefined) {
      return runCmd(this.opts.cliRunner, [this.opts.cli, ...fullArgs], timeoutMs ?? this.opts.timeoutSec * 1000);
    }
    return runCmd(this.opts.cli, fullArgs, timeoutMs ?? this.opts.timeoutSec * 1000);
  }

  async step(id: string, name: string, expected: string, fn: () => Outcome | Promise<Outcome>): Promise<StepRecord> {
    const started = Date.now();
    let outcome: Outcome;
    try {
      outcome = await fn();
    } catch (error) {
      outcome = { observed: {}, ok: false, error: `threw: ${String(error)}` };
    }
    const status: Status = outcome.status ?? (outcome.ok === null ? "pending" : outcome.ok ? "pass" : "fail");
    const record: StepRecord = {
      id,
      name,
      command: "",
      expected,
      observed: outcome.observed,
      ok: outcome.ok,
      status,
      error: outcome.error ?? "",
      duration_ms: Date.now() - started,
      stdout: shorten(outcome.stdout ?? ""),
      stderr: shorten(outcome.stderr ?? ""),
    };
    this.steps.push(record);
    if (status === "fail") this.warnings.push(`${id}: ${record.error || "assertion failed"}`);
    return record;
  }

  setCommand(id: string, command: string): void {
    const step = this.steps.find((entry) => entry.id === id);
    if (step !== undefined) step.command = command;
  }

  private async debugErrors(): Promise<{ result: CmdResult; count: number; watermark: number }> {
    const result = this.cli(["debug", "errors", "--limit", "100"]);
    const json = parseJson(result.stdout);
    const count = Number(field(json, "data", "error_count") ?? 0);
    const watermark = Number(field(json, "data", "watermark") ?? 0);
    return { result, count, watermark };
  }

  private async gameEval(source: string): Promise<CmdResult> {
    return this.cli(["game", "eval", "--source", source, "--confirm"]);
  }

  async execute(): Promise<void> {
    const o = this.opts;

    await this.step("preflight", "前置检查", "godot/cli/project 路径存在且断言模板可解析", () => {
      const missing: string[] = [];
      if (!fs.existsSync(o.godot)) missing.push(`godot: ${o.godot}`);
      if (!fs.existsSync(o.cli)) missing.push(`cli: ${o.cli}`);
      if (!fs.existsSync(path.join(o.project, "project.godot"))) missing.push(`project.godot: ${o.project}`);
      if (!fs.existsSync(path.join(o.project, "assertions.json"))) missing.push("assertions.json");
      let assertionsOk = false;
      try {
        assertionsOk = Array.isArray(field(JSON.parse(fs.readFileSync(path.join(o.project, "assertions.json"), "utf8")), "steps"));
      } catch {
        assertionsOk = false;
      }
      const observed = { godot: o.godot, cli: o.cli, project: o.project, missing, assertions_ok: assertionsOk };
      return { observed, ok: missing.length === 0 && assertionsOk, error: missing.join("; ") };
    });
    if (this.steps.at(-1)?.status === "fail") {
      this.abort = true;
      return;
    }

    const version = await this.step("godot_version", "固定引擎版本", `godot --version 包含 ${EXPECTED_GODOT_VERSION}`, () => {
      const result = runCmd(o.godot, ["--version"], 30_000);
      const text = shorten(result.stdout || result.stderr);
      return {
        observed: { version: text, exit: result.code },
        ok: result.code === 0 && text.includes(EXPECTED_GODOT_VERSION),
        error: result.code === 0 ? `version mismatch: ${text}` : failText(result),
        stdout: result.stdout,
        stderr: result.stderr,
      };
    });
    this.setCommand("godot_version", `${o.godot} --version`);
    if (version.status === "fail") this.abort = true;

    if (!this.abort) {
      await this.step("prepare_addon", "部署 GDExtension 产物", "addon 目录含 .gdextension 与当前平台动态库", () => {
        const libName = LIB_NAMES[process.platform];
        if (libName === undefined) {
          return { observed: { os: process.platform }, ok: false, error: `unsupported platform: ${process.platform}` };
        }
        const dir = path.join(o.project, "addons", "godot-autopilot");
        const descriptor = path.join(dir, "godot-autopilot.gdextension");
        const lib = path.join(dir, libName);
        if (o.lib !== undefined) {
          if (!fs.existsSync(o.lib)) {
            return { observed: { lib: o.lib }, ok: false, error: "--lib 指向的文件不存在" };
          }
          fs.mkdirSync(dir, { recursive: true });
          fs.copyFileSync(o.lib, lib);
        }
        const observed = {
          os: process.platform,
          descriptor,
          descriptor_ok: fs.existsSync(descriptor),
          library: lib,
          library_ok: fs.existsSync(lib),
          library_bytes: fs.existsSync(lib) ? fs.statSync(lib).size : 0,
        };
        const ok = observed.descriptor_ok && observed.library_ok && observed.library_bytes > 0;
        return { observed, ok, error: ok ? undefined : "缺少 .gdextension 或平台库；用 --lib 指定 cmake 构建产物" };
      });
      if (this.steps.at(-1)?.status === "fail") this.abort = true;
    }

    if (this.abort) return;

    const start = await this.step("editor_start", "启动编辑器", "gda start 在 60s 内注册插件实例并返回 id/pid/port", () => {
      const startArgs = ["start", "--godot", o.godot, "--wait", "60"];
      if (o.mode === "headless") startArgs.push("--headless");
      const result = this.cli(startArgs, START_TIMEOUT_MS);
      const json = parseJson(result.stdout);
      const ok = result.code === 0 && json !== null && typeof field(json, "id") === "string" && typeof field(json, "pid") === "number";
      return {
        observed: json ?? { raw: shorten(result.stdout) },
        ok,
        error: ok ? undefined : failText(result),
        stdout: result.stdout,
        stderr: result.stderr,
      };
    });
    this.setCommand("editor_start", `gda start --godot <path> --wait 60 --project <dir> (--headless when mode=headless)`);
    if (start.status === "fail") {
      this.report.failure = "editor_start failed; downstream engine steps were skipped";
      return;
    }

    await this.step("gcp_hello", "GCP hello", "editor status 返回 instance.id 与 capabilities，ok=true", () => {
      const result = this.cli(["editor", "status"]);
      const json = parseJson(result.stdout);
      const ok = result.code === 0 && json?.ok === true && typeof field(json, "instance", "id") === "string";
      return {
        observed: json ?? { raw: shorten(result.stdout) },
        ok,
        error: ok ? undefined : failText(result),
        stdout: result.stdout,
        stderr: result.stderr,
      };
    });

    await this.step("gcp_ping", "GCP ping", "editor ping 返回 pong=true", () => {
      const result = this.cli(["editor", "ping"]);
      const json = parseJson(result.stdout);
      const ok = result.code === 0 && json?.ok === true && field(json, "pong") === true;
      return {
        observed: json ?? { raw: shorten(result.stdout) },
        ok,
        error: ok ? undefined : failText(result),
        stdout: result.stdout,
        stderr: result.stderr,
      };
    });

    await this.step("gcp_status", "GCP system_status", "system_status 返回 ok=true 且 tool_count>0", () => {
      const result = this.cli(["tool", "call", "system_status", "--args", "{}"]);
      const json = parseJson(result.stdout);
      const payload = field(json, "result") ?? json;
      const ok = result.code === 0 && json?.ok === true && Number(field(payload, "tool_count") ?? 0) > 0;
      return {
        observed: payload ?? { raw: shorten(result.stdout) },
        ok,
        error: ok ? undefined : failText(result),
        stdout: result.stdout,
        stderr: result.stderr,
      };
    });

    await this.step("scene_open", "打开主场景", "editor_scene_control open 返回 ok=true", () => {
      const result = this.cli(["scene", "open", "res://main.tscn"]);
      const json = parseJson(result.stdout);
      const ok = result.code === 0 && json?.ok === true;
      return { observed: json ?? { raw: shorten(result.stdout) }, ok, error: ok ? undefined : failText(result), stdout: result.stdout, stderr: result.stderr };
    });

    await this.step("scene_create", "创建场景节点", "create_scene_node 返回 ok=true 且节点名回显", () => {
      const args = JSON.stringify({ type: "Node2D", name: "SmokeRoot" });
      const result = this.cli(["tool", "call", "create_scene_node", "--args", args]);
      const json = parseJson(result.stdout);
      const ok = result.code === 0 && json?.ok === true;
      return { observed: field(json, "result") ?? json ?? { raw: shorten(result.stdout) }, ok, error: ok ? undefined : failText(result), stdout: result.stdout, stderr: result.stderr };
    });

    await this.step("scene_save", "保存场景", "save_editor_scene 落盘 res://smoke_scene.tscn 且 ok=true", () => {
      const args = JSON.stringify({ action: "save_as", path: "res://smoke_scene.tscn" });
      const result = this.cli(["tool", "call", "save_editor_scene", "--args", args]);
      const json = parseJson(result.stdout);
      const ok = result.code === 0 && json?.ok === true;
      const saved = path.join(o.project, "smoke_scene.tscn");
      return { observed: { envelope: field(json, "result") ?? json, disk_exists: fs.existsSync(saved) }, ok, error: ok ? undefined : failText(result), stdout: result.stdout, stderr: result.stderr };
    });

    await this.step("gdscript_exec", "编译执行 GDScript", "execute_script 返回引擎版本且 AutopilotTools 单例已注册", () => {
      const source = "return {\"version\": Engine.get_version_info().get(\"string\", \"\"), \"autopilot_singleton\": Engine.has_singleton(\"AutopilotTools\")}";
      const result = this.cli(["tool", "call", "execute_script", "--args", JSON.stringify({ source_code: source }), "--confirm"]);
      const json = parseJson(result.stdout);
      const payload = field(json, "result") ?? json;
      const version = deepField(payload, "version");
      const singleton = deepField(payload, "autopilot_singleton");
      const ok = result.code === 0 && json?.ok === true && typeof version === "string" && version.includes(EXPECTED_GODOT_VERSION) && singleton === true;
      return { observed: payload ?? { raw: shorten(result.stdout) }, ok, error: ok ? undefined : `version=${String(version)} singleton=${String(singleton)} | ${failText(result)}`, stdout: result.stdout, stderr: result.stderr };
    });

    await this.step("game_bridge", "启动游戏并验证 bridge", "game start 与 game status 均返回 ok=true", () => {
      const play = this.cli(["game", "start", "--confirm"]);
      const status = this.cli(["game", "status"]);
      const playJson = parseJson(play.stdout);
      const statusJson = parseJson(status.stdout);
      const ok = play.code === 0 && playJson?.ok === true && status.code === 0 && statusJson?.ok === true;
      return {
        observed: { play: field(playJson, "result") ?? playJson, status: field(statusJson, "result") ?? statusJson },
        ok,
        error: ok ? undefined : `play: ${failText(play)} | status: ${failText(status)}`,
        stdout: play.stdout,
        stderr: `${play.stderr}\n${status.stderr}`,
      };
    });

    const probeSource = [
      "var n = get_tree().current_scene",
      "if n == null or not n.has_method(\"probe_snapshot\"):",
      "\treturn {\"error\": \"no_probe\"}",
      "return n.probe_snapshot()",
    ].join("\n");
    await this.step("game_probe", "游戏内探针快照", "probe_snapshot 返回 marker 与数值型 frames_seen", async () => {
      await new Promise((resolve) => setTimeout(resolve, 500));
      const result = this.gameEval(probeSource);
      const json = parseJson(result.stdout);
      const payload = field(json, "result") ?? json;
      const marker = deepField(payload, "marker");
      const frames = deepField(payload, "frames_seen");
      const ok = result.code === 0 && json?.ok === true && marker === "gda-engine-smoke-v1" && typeof frames === "number";
      return { observed: payload ?? { raw: shorten(result.stdout) }, ok, error: ok ? undefined : `marker=${String(marker)} frames=${String(frames)} | ${failText(result)}`, stdout: result.stdout, stderr: result.stderr };
    });

    await this.step("input_injection", "注入游戏输入", "game input 入队返回 ok=true", () => {
      const event = JSON.stringify({ keycode: 32 });
      const result = this.cli(["game", "input", "--type", "key", "--mode", "event", "--event", event, "--confirm"]);
      const json = parseJson(result.stdout);
      const ok = result.code === 0 && json?.ok === true;
      return { observed: field(json, "result") ?? json ?? { raw: shorten(result.stdout) }, ok, error: ok ? undefined : failText(result), stdout: result.stdout, stderr: result.stderr };
    });

    await this.step("frame_advance", "验证帧推进", "两次读取 process_frames 严格递增", () => {
      const source = "return Engine.get_process_frames()";
      const firstResult = this.gameEval(source);
      const secondResult = this.gameEval(source);
      const firstJson = parseJson(firstResult.stdout);
      const secondJson = parseJson(secondResult.stdout);
      const first = Number(deepScalar(firstJson, ["return_value", "result"]) ?? 0);
      const second = Number(deepScalar(secondJson, ["return_value", "result"]) ?? 0);
      const ok = firstResult.code === 0 && secondResult.code === 0 && Number.isFinite(first) && Number.isFinite(second) && second > first;
      return { observed: { first, second }, ok, error: ok ? undefined : `first=${first} second=${second} | ${failText(firstResult)} | ${failText(secondResult)}`, stdout: secondResult.stdout, stderr: secondResult.stderr };
    });

    await this.step("screenshot", "捕获截图", "GUI 模式产出 PNG；headless 模式跳过", () => {
      if (o.mode === "headless") {
        return { observed: "skipped: headless 无渲染输出，截图由 GUI 作业承担", ok: null, status: "skip" };
      }
      const dir = path.join(path.dirname(o.report), "artifacts", "screenshots");
      fs.mkdirSync(dir, { recursive: true });
      const result = this.cli(["game", "capture", "--target", "game", "--save", "--dir", dir]);
      const pngs = fs.existsSync(dir) ? fs.readdirSync(dir).filter((name) => name.toLowerCase().endsWith(".png")) : [];
      this.artifacts.screenshots = dir;
      const ok = result.code === 0 && pngs.length > 0;
      return { observed: { png_count: pngs.length, files: pngs }, ok, error: ok ? undefined : failText(result), stdout: result.stdout, stderr: result.stderr };
    });

    await this.step("error_delta", "错误增量与帧推进", "push_error 后错误增量增加且帧继续推进", async () => {
      const before = await this.debugErrors();
      const inject = this.gameEval("push_error(\"gda engine smoke injected\"); return \"injected\"");
      const after = await this.debugErrors();
      const advanced = this.gameEval("return Engine.get_process_frames()");
      const observed = {
        before: { count: before.count, watermark: before.watermark },
        after: { count: after.count, watermark: after.watermark },
        inject_exit: inject.code,
        frame_advance_exit: advanced.code,
      };
      const delta = after.watermark > before.watermark || after.count > before.count;
      const ok = inject.code === 0 && advanced.code === 0 && delta;
      return {
        observed,
        ok,
        error: ok ? undefined : `no error delta: ${JSON.stringify(observed)} | inject=${failText(inject)}`,
        stdout: after.result.stdout,
        stderr: `${before.result.stderr}\n${inject.stderr}\n${after.result.stderr}`,
      };
    });

    await this.step("export_guard", "导出期拒绝入口", "导出期工具/资源/提示词入口均返回 editor is exporting", () => {
      const tool = this.cli(["tool", "call", "ping", "--args", "{}"]);
      const list = this.cli(["resources", "list"]);
      const listJson = parseJson(list.stdout);
      const uriCandidates = deepField(listJson, "resources") ?? [];
      const uri = Array.isArray(uriCandidates) ? String((uriCandidates as Array<Record<string, unknown>>).find((item) => typeof field(item, "uri") === "string")?.uri ?? "") : "";
      const resource = uri === "" ? null : this.cli(["resources", "read", uri]);
      const prompts = this.cli(["prompts", "list"]);
      const promptsJson = parseJson(prompts.stdout);
      const promptCandidates = deepField(promptsJson, "prompts") ?? [];
      const name = Array.isArray(promptCandidates) ? String((promptCandidates as Array<Record<string, unknown>>).find((item) => typeof field(item, "name") === "string")?.name ?? "") : "";
      const prompt = name === "" ? null : this.cli(["prompts", "get", name]);
      const observed = {
        baseline: {
          tool_exit: tool.code,
          resources_list_exit: list.code,
          resource_uri: uri,
          resource_exit: resource?.code ?? null,
          prompts_list_exit: prompts.code,
          prompt_name: name,
          prompt_exit: prompt?.code ?? null,
        },
        export_preset: o.exportPreset ?? null,
        export_blocked_observed: false,
      };
      return {
        observed,
        ok: null,
        status: "pending",
        error: "CLI 无进程内导出入口；导出期拒绝须在真实编辑器导出中验证，见 failure-injection.md。baseline 仅证明三类等价入口可达。",
        stdout: `${tool.stdout}\n${list.stdout}\n${prompts.stdout}`,
        stderr: `${tool.stderr}\n${list.stderr}\n${prompts.stderr}`,
      };
    });

    await this.step("ipc_auth", "IPC 认证/端点校验", "伪造 IPC 端点在发送请求前失败", () => {
      const fake = path.join(os.tmpdir(), `gda-engine-smoke-missing-${process.pid}`);
      const result = this.cli(["editor", "ping", "--socket", fake]);
      const json = parseJson(result.stdout);
      const requestSent = field(json, "request_sent") === true;
      const ok = result.code !== 0 && !requestSent;
      return {
        observed: { exit: result.code, request_sent: requestSent, error: field(json, "error") ?? shorten(result.stderr) },
        ok,
        error: ok ? undefined : failText(result),
        stdout: result.stdout,
        stderr: result.stderr,
      };
    });

    await this.step("ipc_no_resend", "IPC no-resend", "超时请求 error.request_sent=true 且未重发，随后 ping 仍可用", () => {
      const args = JSON.stringify({ source_code: "while true: pass" });
      const result = this.cli(["call", "execute_script", "--args", args, "--timeout", "1", "--confirm"], 15_000);
      const json = parseJson(result.stdout);
      const errorRecord = field(json, "error");
      const message = String(field(errorRecord, "message") ?? "");
      const requestSent = field(errorRecord, "request_sent") === true;
      const recovered = this.cli(["editor", "ping"]);
      const ok = result.code !== 0 && requestSent && recovered.code === 0 && /not retried|not re-sent|exec_timeout/i.test(message);
      return {
        observed: { exit: result.code, request_sent: requestSent, message, recovered_exit: recovered.code },
        ok,
        error: ok ? undefined : `timeout=${failText(result)} recovered=${failText(recovered)}`,
        stdout: result.stdout,
        stderr: result.stderr,
      };
    });

    await this.step("cleanup", "停止编辑器", "gda stop 正常退出", () => {
      const result = this.cli(["stop"]);
      return { observed: { exit: result.code }, ok: result.code === 0, error: result.code === 0 ? undefined : failText(result), stdout: result.stdout, stderr: result.stderr };
    });
  }
}

function buildReport(o: Options): Report {
  return {
    schema_version: 1,
    task: "T-15",
    generated_at: new Date().toISOString(),
    status: "pending",
    environment: {
      godot: o.godot,
      godot_version_expected: EXPECTED_GODOT_VERSION,
      cli: o.cli,
      project: o.project,
      mode: o.mode,
      platform: process.platform,
      arch: process.arch,
      timeout_sec: o.timeoutSec,
    },
    summary: { total: 0, passed: 0, failed: 0, skipped: 0, pending: 0 },
    steps: [],
    artifacts: { report: o.report },
    warnings: [],
    failure: null,
  };
}

function finalize(report: Report, runner: Runner): void {
  report.steps = runner.steps;
  report.warnings = runner.warnings;
  report.artifacts = { ...report.artifacts, ...runner.artifacts };
  const summary = report.summary;
  for (const step of runner.steps) {
    summary.total += 1;
    if (step.status === "pass") summary.passed += 1;
    else if (step.status === "fail") summary.failed += 1;
    else if (step.status === "skip") summary.skipped += 1;
    else summary.pending += 1;
  }
  report.status = summary.failed > 0 ? "fail" : summary.pending > 0 ? "pending" : "pass";
}

async function main(): Promise<number> {
  let opts: Options;
  try {
    opts = parseArgs(process.argv.slice(2));
  } catch (error) {
    process.stderr.write(`${String(error instanceof Error ? error.message : error)}\n`);
    return 3;
  }
  const report = buildReport(opts);
  const runner = new Runner(opts, report);
  try {
    await runner.execute();
  } catch (error) {
    report.failure = String(error);
  } finally {
    finalize(report, runner);
    fs.mkdirSync(path.dirname(opts.report), { recursive: true });
    fs.writeFileSync(opts.report, `${JSON.stringify(report, null, 2)}\n`, "utf8");
    process.stdout.write(`${opts.report}\nstatus=${report.status} total=${report.summary.total} pass=${report.summary.passed} fail=${report.summary.failed} skip=${report.summary.skipped} pending=${report.summary.pending}\n`);
  }
  return report.status === "fail" || report.failure !== null ? 1 : 0;
}

process.exit(await main());
