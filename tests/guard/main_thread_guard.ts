import path from "node:path";

import {
  ROOT,
  decodeUtf8Strict,
  escapeRegExp,
  isDirectory,
  readBytes,
  relativeWithin,
  toPosix,
  walkFiles,
} from "./guard_support";

const SCAN_DIR = path.join(ROOT, "src");
const SNIPPET_MAX = 120;
const RAW_START = /(?:u8|u|U|L)?R"([^()\s\\]{0,16})\(/y;

const CTX_MAIN = "main";
const CTX_MANAGED = "managed";
const CTX_FOREIGN = "foreign";

type Context = typeof CTX_MAIN | typeof CTX_MANAGED | typeof CTX_FOREIGN;

const THREAD_CONTEXT_RULES: ReadonlyArray<readonly [string, Context, string]> = [
  ["src/core/command_queue.hpp", CTX_FOREIGN, "队列任务体可在 transport 线程构造，只有 drain 后才在主线程执行。"],
  ["src/core/config.hpp", CTX_FOREIGN, "被 transport 线程的工具层直接包含。"],
  ["src/core/error_watermark.hpp", CTX_FOREIGN, "错误水位由 transport 线程累计。"],
  ["src/core/observability/log_persist.cpp", CTX_FOREIGN, "transport 线程经 SpecTool::execute 入队日志与轨迹并镜像写盘。"],
  ["src/core/observability/log_persist.hpp", CTX_FOREIGN, "transport 线程经 SpecTool::execute 入队日志与轨迹并镜像写盘。"],
  ["src/core/observability/log_system.cpp", CTX_FOREIGN, "transport 线程直接写日志。"],
  ["src/core/observability/log_system.hpp", CTX_FOREIGN, "transport 线程直接写日志。"],
  ["src/core/observability/monitor.cpp", CTX_FOREIGN, "transport 线程直接上报监控事件。"],
  ["src/core/observability/monitor.hpp", CTX_FOREIGN, "transport 线程直接上报监控事件。"],
  ["src/core/observability/sanitize_policy.cpp", CTX_FOREIGN, "transport 线程读取脱敏开关。"],
  ["src/core/observability/sanitize_policy.hpp", CTX_FOREIGN, "transport 线程读取脱敏开关。"],
  ["src/core/server_context.cpp", CTX_FOREIGN, "MCP 服务与端口解析发生在 transport 线程。"],
  ["src/core/server_context.hpp", CTX_FOREIGN, "MCP 服务与端口解析发生在 transport 线程。"],
  ["src/core/observability/trace_recorder.cpp", CTX_FOREIGN, "transport 线程直接记录轨迹事件。"],
  ["src/core/observability/trace_recorder.hpp", CTX_FOREIGN, "transport 线程直接记录轨迹事件。"],
  ["src/tools/base/authorization.hpp", CTX_FOREIGN, "能力判定由 transport 线程的元工具路径触发。"],
  ["src/tools/base/dispatch.cpp", CTX_FOREIGN, "工具分发在 transport 线程发起，只有提交到队列后才在主线程执行。"],
  ["src/tools/base/dispatch.hpp", CTX_FOREIGN, "工具分发在 transport 线程发起，只有提交到队列后才在主线程执行。"],
  ["src/tools/base/dynamic_spec_store.hpp", CTX_FOREIGN, "动态工具规格由 transport 线程读写。"],
  ["src/tools/base/ready_policy.hpp", CTX_FOREIGN, "就绪判定在 transport 线程执行。"],
  ["src/tools/base/register_all.cpp", CTX_FOREIGN, "MCP 工具注册回调在 transport 线程执行。"],
  ["src/tools/base/register_all.hpp", CTX_FOREIGN, "MCP 工具注册回调在 transport 线程执行。"],
  ["src/tools/base/tool_base.hpp", CTX_FOREIGN, "工具基类被 transport 线程的元工具路径使用。"],
  ["src/tools/base/tool_invoke.cpp", CTX_FOREIGN, "工具组合调用在发起线程执行。"],
  ["src/tools/base/tool_invoke.hpp", CTX_FOREIGN, "工具组合调用在发起线程执行。"],
  ["src/tools/base/tool_pipeline.cpp", CTX_FOREIGN, "后处理管线随 SpecTool::execute 在发起线程执行。"],
  ["src/tools/base/tool_pipeline.hpp", CTX_FOREIGN, "后处理管线随 SpecTool::execute 在发起线程执行。"],
  ["src/tools/base/tool_spec.hpp", CTX_FOREIGN, "SpecTool::execute 在 call_tool 元工具路径上于 transport 线程执行。"],
  ["src/runtime/*", CTX_MANAGED, "游戏进程侧：Godot 调用经 SceneTreeTimer 与 awaiter 排到游戏主线程。"],
  ["src/main.cpp", CTX_MAIN, "引擎插件生命周期回调（_enter_tree/_process/_exit_tree）由引擎在主线程驱动。"],
  ["src/core/editor_coords.cpp", CTX_MAIN, "纯几何换算，不触碰引擎状态。"],
  ["src/core/editor_coords.hpp", CTX_MAIN, "纯几何换算，不触碰引擎状态。"],
  ["src/core/editor_readiness.cpp", CTX_MAIN, "编辑器就绪探测只在主线程查询 EditorInterface。"],
  ["src/core/editor_readiness.hpp", CTX_MAIN, "编辑器就绪探测只在主线程查询 EditorInterface。"],
  ["src/core/export_guard.cpp", CTX_MAIN, "导出回调由引擎导出流程在主线程触发。"],
  ["src/core/export_guard.hpp", CTX_MAIN, "导出回调由引擎导出流程在主线程触发。"],
  ["src/core/mode_detector.cpp", CTX_MAIN, "运行模式探测在主线程读取引擎单例。"],
  ["src/core/mode_detector.hpp", CTX_MAIN, "运行模式探测在主线程读取引擎单例。"],
  ["src/core/observability/monitor_env.cpp", CTX_MAIN, "环境快照只在 _enter_tree 的主线程调用。"],
  ["src/core/observability/perf_sampler.cpp", CTX_MAIN, "性能采样由 _process 在主线程驱动。"],
  ["src/core/observability/perf_sampler.hpp", CTX_MAIN, "性能采样由 _process 在主线程驱动。"],
  ["src/core/plugin_config.cpp", CTX_MAIN, "配置读写由主线程触发：配置面板、启动快照与领域工具的授权判定。"],
  ["src/core/plugin_config.hpp", CTX_MAIN, "配置读写由主线程触发：配置面板、启动快照与领域工具的授权判定。"],
  ["src/core/resource_registry.cpp", CTX_MAIN, "资源注册表只被主线程上的 code_execute 与 resource 工具使用。"],
  ["src/core/resource_registry.hpp", CTX_MAIN, "资源注册表只被主线程上的 code_execute 与 resource 工具使用。"],
  ["src/core/scene_dirty_tracker.cpp", CTX_MAIN, "脏场景标记由主线程的 _get_unsaved_status 与主线程工具读写。"],
  ["src/core/scene_dirty_tracker.hpp", CTX_MAIN, "脏场景标记由主线程的 _get_unsaved_status 与主线程工具读写。"],
  ["src/tools/base/autopilot_tools.cpp", CTX_MAIN, "AutopilotTools 引擎单例的方法由引擎在主线程调用。"],
  ["src/tools/base/autopilot_tools.hpp", CTX_MAIN, "AutopilotTools 引擎单例的方法由引擎在主线程调用。"],
  ["src/tools/base/debugger_access.cpp", CTX_MAIN, "调试器会话访问由编辑器调试器插件在主线程回调。"],
  ["src/tools/base/debugger_access.hpp", CTX_MAIN, "调试器会话访问由编辑器调试器插件在主线程回调。"],
  ["src/tools/domains/debugger/debugger_ops.cpp", CTX_MAIN, "调试器插件由编辑器在主线程回调。"],
  ["src/tools/domains/debugger/debugger_ops.hpp", CTX_MAIN, "调试器插件由编辑器在主线程回调。"],
  ["src/tools/domains/editor/editor_ui_actions_ops.cpp", CTX_MAIN, "编辑器 UI 动作在主线程执行。"],
  ["src/tools/domains/editor/editor_ui_actions_ops.hpp", CTX_MAIN, "编辑器 UI 动作在主线程执行。"],
  ["src/tools/domains/editor/editor_ui_ops.cpp", CTX_MAIN, "编辑器 UI 工具经 dispatch 提交到主线程队列执行。"],
  ["src/tools/domains/editor/editor_ui_ops.hpp", CTX_MAIN, "编辑器 UI 工具经 dispatch 提交到主线程队列执行。"],
  ["src/tools/domains/game/game_ops.cpp", CTX_MAIN, "游戏运行时工具经 dispatch 提交到主线程队列执行。"],
  ["src/tools/base/runtime_ops.cpp", CTX_MAIN, "游戏通道编排在编辑器主线程执行：调试器回调或 CommandQueue 提交体。"],
  ["src/tools/base/runtime_ops.hpp", CTX_MAIN, "游戏通道编排在编辑器主线程执行：调试器回调或 CommandQueue 提交体。"],
  ["src/tools/base/schema_builder.cpp", CTX_MAIN, "schema 构造是纯数据处理。"],
  ["src/tools/base/schema_builder.hpp", CTX_MAIN, "schema 构造是纯数据处理。"],
  ["src/tools/base/tool_args.cpp", CTX_MAIN, "参数解析是纯数据处理。"],
  ["src/tools/base/tool_args.hpp", CTX_MAIN, "参数解析是纯数据处理。"],
  ["src/tools/base/tool_catalog.cpp", CTX_MAIN, "工具目录是纯数据索引。"],
  ["src/tools/base/tool_catalog.hpp", CTX_MAIN, "工具目录是纯数据索引。"],
  ["src/tools/base/tool_registry.hpp", CTX_MAIN, "工具注册表只保存静态描述数据。"],
  ["src/tools/*_ops.cpp", CTX_MAIN, "领域工具 handler 经 dispatch 提交到主线程队列执行。"],
  ["src/tools/*_ops.hpp", CTX_MAIN, "领域工具头文件，实现随同名 _ops.cpp 同线程。"],
  ["src/tools/*_tools.hpp", CTX_MAIN, "工具描述头文件，只保存静态描述数据。"],
  ["src/prompts/*", CTX_MAIN, "prompt 构造体经 CommandQueue 提交到主线程执行。"],
  ["src/resources/*", CTX_MAIN, "资源读取体经 CommandQueue 主线程预算提交执行。"],
  ["src/ui/*", CTX_MAIN, "编辑器 UI 控件只在主线程构造与回调。"],
  ["src/util/*", CTX_MAIN, "工具层纯转换与文件辅助，只被主线程上的 handler 调用。"],
];

const FOREIGN_MAIN_THREAD_SCOPES: ReadonlyMap<string, ReadonlySet<string>> = new Map([
  [
    "src/core/observability/log_persist.cpp",
    new Set([
      "godot_autopilot::LogPersist::init_session",
      "godot_autopilot::LogPersist::flush_on_main_thread",
      "godot_autopilot::LogPersist::flush_trace_images",
      "godot_autopilot::LogPersist::rotate_on_init",
      "godot_autopilot::append_persist_file",
      "godot_autopilot::prune_persist_dir",
    ]),
  ],
]);

const GODOT_API_PATTERNS: ReadonlyArray<readonly [string, string]> = [
  ["godot::FileAccess", "\\bgodot::FileAccess\\b"],
  ["godot::DirAccess", "\\bgodot::DirAccess\\b"],
  ["godot::EditorInterface", "\\bgodot::EditorInterface\\b"],
  ["godot::Engine", "\\bgodot::Engine\\b"],
  ["godot::SceneTree", "\\bgodot::SceneTree\\b"],
  ["godot::ClassDB", "\\bgodot::ClassDB\\b"],
  ["godot::ClassDBSingleton", "\\bgodot::ClassDBSingleton\\b"],
  ["godot::ProjectSettings", "\\bgodot::ProjectSettings\\b"],
  ["godot::Input", "\\bgodot::Input\\b"],
  ["godot::DisplayServer", "\\bgodot::DisplayServer\\b"],
  ["godot::OS", "\\bgodot::OS\\b"],
  ["godot::Performance", "\\bgodot::Performance\\b"],
  ["godot::Resource", "\\bgodot::Resource\\b"],
  ["godot::ResourceLoader", "\\bgodot::ResourceLoader\\b"],
  ["godot::ResourceSaver", "\\bgodot::ResourceSaver\\b"],
  ["godot::Ref<godot::Resource>", "\\bgodot::Ref\\s*<\\s*godot::Resource\\b"],
];

const CONTROL_KEYWORDS = new Set([
  "if",
  "for",
  "while",
  "switch",
  "catch",
  "do",
  "else",
  "return",
  "throw",
  "sizeof",
  "decltype",
  "new",
  "delete",
  "case",
  "default",
  "co_await",
  "co_return",
  "co_yield",
  "alignof",
  "static_assert",
  "noexcept",
]);

const DEFINITION_SUFFIXES = ["{", "const", "noexcept", "override", "final", "->", ":"];
const EMPTY_SCOPES: ReadonlySet<string> = new Set<string>();

interface Scope {
  kind: string;
  name: string | null;
  qualified: string;
  parent: Scope | null;
  start: number;
  end: number;
}

type Violation = [string, number, string, string, string];

function matchRawStart(text: string, index: number): RegExpExecArray | null {
  RAW_START.lastIndex = index;
  return RAW_START.exec(text);
}

function blank(chunk: string): string {
  return chunk.replace(/[^\n]/g, " ");
}

function maskLiterals(text: string): string {
  const out: string[] = [];
  const n = text.length;
  let index = 0;
  while (index < n) {
    const ch = text[index];
    if (ch === "R" || ch === "U" || ch === "L" || ch === "u") {
      const match = matchRawStart(text, index);
      if (match !== null) {
        const closer = ")" + match[1] + '"';
        let end = text.indexOf(closer, match.index + match[0].length);
        end = end === -1 ? n : end + closer.length;
        out.push(blank(text.slice(index, end)));
        index = end;
        continue;
      }
    }
    if (ch === '"') {
      let next = index + 1;
      while (next < n) {
        const current = text[next];
        if (current === "\\") {
          next += 2;
          continue;
        }
        if (current === '"') {
          next += 1;
          break;
        }
        if (current === "\n") {
          next += 1;
          break;
        }
        next += 1;
      }
      out.push(blank(text.slice(index, next)));
      index = next;
      continue;
    }
    if (ch === "'") {
      let next = index + 1;
      while (next < n) {
        const current = text[next];
        if (current === "\\") {
          next += 2;
          continue;
        }
        if (current === "'") {
          next += 1;
          break;
        }
        if (current === "\n") {
          next += 1;
          break;
        }
        next += 1;
      }
      out.push(blank(text.slice(index, next)));
      index = next;
      continue;
    }
    out.push(ch);
    index += 1;
  }
  return out.join("");
}

function stripComments(text: string): string {
  const out: string[] = [];
  const n = text.length;
  let index = 0;
  while (index < n) {
    if (text[index] === "/" && index + 1 < n && text[index + 1] === "/") {
      let end = text.indexOf("\n", index);
      end = end === -1 ? n : end;
      out.push(blank(text.slice(index, end)));
      index = end;
      continue;
    }
    if (text[index] === "/" && index + 1 < n && text[index + 1] === "*") {
      let end = text.indexOf("*/", index + 2);
      end = end === -1 ? n : end + 2;
      out.push(blank(text.slice(index, end)));
      index = end;
      continue;
    }
    out.push(text[index]);
    index += 1;
  }
  return out.join("");
}

function matchParen(text: string, openIndex: number): number {
  let depth = 0;
  let index = openIndex;
  while (index < text.length) {
    const ch = text[index];
    if (ch === "(") depth += 1;
    else if (ch === ")") {
      depth -= 1;
      if (depth === 0) return index;
    }
    index += 1;
  }
  return -1;
}

function definitionName(
  pending: string,
  pendingStart: number,
  fullText: string,
): [string | null, string | null] {
  const text = pending.trim();
  if (text === "") return [null, null];
  const lead = pending.length - pending.trimStart().length;

  const namespaceMatch = /\bnamespace\s+([A-Za-z_][A-Za-z0-9_]*)/.exec(text);
  if (namespaceMatch !== null) return ["namespace", namespaceMatch[1]];

  const classMatch = /\b(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)/.exec(text);
  if (classMatch !== null) return ["class", classMatch[1]];

  const enumMatch = /\benum\s+(?:class\s+)?([A-Za-z_][A-Za-z0-9_]*)/.exec(text);
  if (enumMatch !== null) return ["class", enumMatch[1]];

  const functionPattern = /([A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_~][A-Za-z0-9_]*)*)\s*\(/g;
  for (const match of text.matchAll(functionPattern)) {
    const name = match[1];
    const simple = name.split("::").at(-1) ?? name;
    if (CONTROL_KEYWORDS.has(simple)) continue;
    const localIndex = match.index ?? 0;
    const openIndex = pendingStart + lead + localIndex + match[0].length - 1;
    const closeIndex = matchParen(fullText, openIndex);
    if (closeIndex < 0) continue;
    const tail = fullText.slice(closeIndex + 1).trimStart();
    if (!DEFINITION_SUFFIXES.some((suffix) => tail.startsWith(suffix))) continue;
    const before = text.slice(0, localIndex).trimEnd();
    if (before.endsWith(".") || before.endsWith("->")) continue;
    if (before.includes("=")) continue;
    return ["function", name];
  }
  return [null, null];
}

function namespacePrefix(stack: Scope[]): string {
  for (let index = stack.length - 1; index >= 0; index -= 1) {
    if (stack[index].kind === "namespace") return stack[index].qualified;
  }
  return "";
}

function buildScopes(text: string): Scope[] {
  const scopes: Scope[] = [];
  const stack: Scope[] = [];
  let pending: string[] = [];
  let pendingStart = 0;
  const n = text.length;
  let index = 0;
  while (index < n) {
    const ch = text[index];
    if (ch === "{") {
      const [rawKind, name] = definitionName(pending.join(""), pendingStart, text);
      const parent = stack.length > 0 ? stack[stack.length - 1] : null;
      let kind = "block";
      let qualified = parent !== null ? parent.qualified : "";
      if (rawKind === "namespace" || rawKind === "class") {
        kind = rawKind;
        const prefix = namespacePrefix(stack);
        qualified = prefix !== "" ? `${prefix}::${name ?? ""}` : name ?? "";
      } else if (rawKind === "function") {
        kind = "function";
        const prefix = namespacePrefix(stack);
        let defName = name ?? "";
        if (!defName.includes("::") && parent !== null && parent.kind === "class") {
          defName = `${parent.name ?? ""}::${defName}`;
        }
        qualified = prefix !== "" ? `${prefix}::${defName}` : defName;
      }
      const scope: Scope = {
        kind,
        name: kind === "block" ? null : name,
        qualified,
        parent,
        start: index,
        end: n,
      };
      scopes.push(scope);
      stack.push(scope);
      pending = [];
      pendingStart = index + 1;
      index += 1;
      continue;
    }
    if (ch === "}") {
      if (stack.length > 0) {
        stack[stack.length - 1].end = index;
        stack.pop();
      }
      pending = [];
      pendingStart = index + 1;
      index += 1;
      continue;
    }
    if (ch === ";") {
      pending = [];
      pendingStart = index + 1;
      index += 1;
      continue;
    }
    if (pending.length === 0) pendingStart = index;
    pending.push(ch);
    index += 1;
  }
  return scopes;
}

function chainAt(scopes: Scope[], offset: number): Scope[] {
  let best: Scope | null = null;
  for (const scope of scopes) {
    if (scope.start <= offset && offset < scope.end) {
      if (best === null || scope.start > best.start) best = scope;
    }
  }
  const chain: Scope[] = [];
  let current: Scope | null = best;
  while (current !== null) {
    chain.push(current);
    current = current.parent;
  }
  chain.reverse();
  return chain;
}

function functionScopes(chain: Scope[]): string[] {
  return chain.filter((scope) => scope.kind === "function").map((scope) => scope.qualified);
}

const FNM_CACHE = new Map<string, RegExp>();

function fnmatchTranslate(pattern: string): string {
  let out = "^";
  let index = 0;
  while (index < pattern.length) {
    const ch = pattern[index];
    if (ch === "*") {
      out += ".*";
      index += 1;
      continue;
    }
    if (ch === "?") {
      out += ".";
      index += 1;
      continue;
    }
    if (ch === "[") {
      let next = index + 1;
      let negated = false;
      if (next < pattern.length && pattern[next] === "!") {
        negated = true;
        next += 1;
      }
      if (next < pattern.length && pattern[next] === "]") next += 1;
      while (next < pattern.length && pattern[next] !== "]") next += 1;
      if (next >= pattern.length) {
        out += "\\[";
        index += 1;
        continue;
      }
      let stuff = pattern.slice(index + 1, next);
      if (negated) stuff = "^" + stuff.slice(1);
      else if (stuff.startsWith("^") || stuff.startsWith("[")) stuff = "\\" + stuff;
      out += "[" + stuff.replace(/\\/g, "\\\\") + "]";
      index = next + 1;
      continue;
    }
    out += escapeRegExp(ch);
    index += 1;
  }
  return out + "$";
}

function fnmatchCase(name: string, pattern: string): boolean {
  let regex = FNM_CACHE.get(pattern);
  if (regex === undefined) {
    regex = new RegExp(fnmatchTranslate(pattern));
    FNM_CACHE.set(pattern, regex);
  }
  return regex.test(name);
}

function contextOf(relPath: string): Context {
  for (const [pattern, context] of THREAD_CONTEXT_RULES) {
    if (fnmatchCase(relPath, pattern)) return context;
  }
  return CTX_FOREIGN;
}

function collectFiles(scanDir: string): string[] {
  if (!isDirectory(scanDir)) return [];
  return walkFiles(scanDir).filter((file) => file.endsWith(".cpp") || file.endsWith(".hpp"));
}

function displayPath(file: string, scanDir: string): string {
  const rootRel = relativeWithin(ROOT, file);
  if (rootRel !== null) return toPosix(rootRel);
  return toPosix(relativeWithin(scanDir, file) ?? path.basename(file));
}

function lineStarts(text: string): number[] {
  const starts = [0];
  for (let index = 0; index < text.length; index += 1) {
    if (text[index] === "\n") starts.push(index + 1);
  }
  return starts;
}

function lineOf(starts: number[], offset: number): number {
  let low = 0;
  let high = starts.length - 1;
  while (low < high) {
    const mid = Math.floor((low + high + 1) / 2);
    if (starts[mid] <= offset) low = mid;
    else high = mid - 1;
  }
  return low + 1;
}

function scanFile(relPath: string, text: string): Violation[] {
  const masked = stripComments(maskLiterals(text));
  const scopes = buildScopes(masked);
  const starts = lineStarts(text);
  if (contextOf(relPath) !== CTX_FOREIGN) return [];
  const allowed = FOREIGN_MAIN_THREAD_SCOPES.get(relPath) ?? EMPTY_SCOPES;
  const violations: Violation[] = [];
  const seen = new Set<string>();
  const sourceLines = text.split("\n");
  for (const [symbol, pattern] of GODOT_API_PATTERNS) {
    for (const match of masked.matchAll(new RegExp(pattern, "g"))) {
      const offset = match.index ?? 0;
      const chain = chainAt(scopes, offset);
      const functions = functionScopes(chain);
      if (functions.some((name) => allowed.has(name))) continue;
      const line = lineOf(starts, offset);
      const record = `${relPath}:${line}`;
      if (seen.has(record)) continue;
      seen.add(record);
      const sourceLine = sourceLines[line - 1] ?? "";
      const snippet = Array.from(sourceLine.replace(/\r+$/, "").trim()).slice(0, SNIPPET_MAX).join("");
      violations.push([
        relPath,
        line,
        snippet,
        symbol,
        functions.length > 0 ? functions[functions.length - 1] : "",
      ]);
    }
  }
  return violations;
}

function runSelfTests(): number {
  const fixtureRel = "src/self_test/fixture.cpp";
  const context = contextOf(fixtureRel);
  if (context !== CTX_FOREIGN) {
    console.error(
      `self-test FAIL: foreign/${context}: 样本路径被 THREAD_CONTEXT_RULES 覆盖, 请更换自测样本路径`,
    );
    return 1;
  }
  const badInFunction = "namespace godot_autopilot {\nvoid f() { auto* e = godot::Engine::get_singleton(); }\n}";
  const hits = scanFile(fixtureRel, badInFunction);
  if (!hits.some((hit) => hit[4].split("::").at(-1) === "f")) {
    const scopes = JSON.stringify([...new Set(hits.map((hit) => hit[4]))].sort());
    console.error(`self-test FAIL: 违规且函数链含 f/${hits.length} 条, 函数链 ${scopes}`);
    return 1;
  }
  const badAtGlobalScope = "auto* o = godot::OS::get_singleton();";
  if (scanFile(fixtureRel, badAtGlobalScope).length === 0) {
    console.error("self-test FAIL: >=1 violation/0 hits: 全局作用域 godot::OS 未报违规");
    return 1;
  }
  const mainRel = "src/tools/self_test_fixture_tools.hpp";
  const mainHits = scanFile(mainRel, "auto* e = godot::Engine::get_singleton();");
  if (mainHits.length > 0) {
    console.error(`self-test FAIL: 0 violations/${mainHits.length} hits: main 分类文件不应报违规`);
    return 1;
  }
  const allowedRel = "src/core/observability/log_persist.cpp";
  const allowedText = 'void godot_autopilot::append_persist_file() { godot::FileAccess::exists("x"); }';
  const allowedHits = scanFile(allowedRel, allowedText);
  if (allowedHits.length > 0) {
    console.error(`self-test FAIL: 0 violations/${allowedHits.length} hits: 豁免作用域内调用不应报违规`);
    return 1;
  }
  const literalHits = scanFile(fixtureRel, 'auto* s = "godot::Engine";');
  if (literalHits.length > 0) {
    console.error(`self-test FAIL: 0 violations/${literalHits.length} hits: 字符串字面量未被词法剥离`);
    return 1;
  }
  console.log("main-thread-guard self-test OK");
  return 0;
}

function main(): number {
  const args = process.argv.slice(2);
  if (args.length === 1 && args[0] === "--self-test") return runSelfTests();
  const scanDir = args.length > 0 ? path.resolve(args[0]) : SCAN_DIR;
  const decodeErrors: string[] = [];
  const violations: Violation[] = [];
  for (const file of collectFiles(scanDir)) {
    const rel = displayPath(file, scanDir);
    let data: Buffer;
    try {
      data = readBytes(file);
    } catch (error) {
      decodeErrors.push(`${rel}: read error: ${(error as Error).message}`);
      continue;
    }
    let text: string;
    try {
      text = decodeUtf8Strict(data);
    } catch (error) {
      decodeErrors.push(`${rel}: decode error: ${(error as Error).message}`);
      continue;
    }
    violations.push(...scanFile(rel, text));
  }
  if (decodeErrors.length > 0) {
    for (const item of decodeErrors) console.error(item);
    return 2;
  }
  if (violations.length > 0) {
    for (const [rel, line, snippet] of violations) console.log(`${rel}:${line}: ${snippet}`);
    return 1;
  }
  console.log("main-thread-guard OK");
  return 0;
}

process.exitCode = main();
