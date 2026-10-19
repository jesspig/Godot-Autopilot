#!/usr/bin/env python3
"""主线程契约守卫：静态扫描 src/ 下 *.cpp/*.hpp 中的 Godot API 直接调用。

判定依据是「线程上下文分类」：每个源文件被显式归入主线程（main）、受管
（managed）或外线程（foreign）三类。外线程文件里的 Godot API 调用默认
违规，只有落在该文件显式登记的主线程作用域里才放行；已确认但尚未修复的
越界点登记在 KNOWN_FOREIGN_API_SITES 里，不计入违规但会在 stderr 提示。

词法层面先剥离字符串字面量（含转义）、字符字面量与 R"TAG(...)TAG" raw
string，再剥离注释，最后才做标识符匹配，避免把字符串与注释里的 API 名
误判为真实调用。

无违规打印 main-thread-guard OK 并退出 0，有违规退出 1，文件读/解码失败
退出 2。可选参数是待扫描目录，缺省为仓库的 src/。只用标准库。
"""

import fnmatch
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
SCAN_DIR = ROOT / "src"
SNIPPET_MAX = 120
RAW_START = re.compile(r"(?:u8|u|U|L)?R\"([^()\s\\]{0,16})\(")

CTX_MAIN = "main"
CTX_MANAGED = "managed"
CTX_FOREIGN = "foreign"

THREAD_CONTEXT_RULES = (
    ("src/core/command_queue.hpp", CTX_FOREIGN,
     "队列任务体可在 transport 线程构造，只有 drain 后才在主线程执行。"),
    ("src/core/config.hpp", CTX_FOREIGN,
     "被 transport 线程的工具层直接包含。"),
    ("src/core/error_watermark.hpp", CTX_FOREIGN,
     "错误水位由 transport 线程累计。"),
    ("src/core/log_persist.cpp", CTX_FOREIGN,
     "transport 线程经 SpecTool::execute 入队日志与轨迹并镜像写盘。"),
    ("src/core/log_persist.hpp", CTX_FOREIGN,
     "transport 线程经 SpecTool::execute 入队日志与轨迹并镜像写盘。"),
    ("src/core/log_system.cpp", CTX_FOREIGN,
     "transport 线程直接写日志。"),
    ("src/core/log_system.hpp", CTX_FOREIGN,
     "transport 线程直接写日志。"),
    ("src/core/monitor.cpp", CTX_FOREIGN,
     "transport 线程直接上报监控事件。"),
    ("src/core/monitor.hpp", CTX_FOREIGN,
     "transport 线程直接上报监控事件。"),
    ("src/core/sanitize_policy.cpp", CTX_FOREIGN,
     "transport 线程读取脱敏开关。"),
    ("src/core/sanitize_policy.hpp", CTX_FOREIGN,
     "transport 线程读取脱敏开关。"),
    ("src/core/server_context.cpp", CTX_FOREIGN,
     "MCP 服务与端口解析发生在 transport 线程。"),
    ("src/core/server_context.hpp", CTX_FOREIGN,
     "MCP 服务与端口解析发生在 transport 线程。"),
    ("src/core/trace_recorder.cpp", CTX_FOREIGN,
     "transport 线程直接记录轨迹事件。"),
    ("src/core/trace_recorder.hpp", CTX_FOREIGN,
     "transport 线程直接记录轨迹事件。"),
    ("src/tools/authorization.hpp", CTX_FOREIGN,
     "能力判定由 transport 线程的元工具路径触发。"),
    ("src/tools/dispatch.cpp", CTX_FOREIGN,
     "工具分发在 transport 线程发起，只有提交到队列后才在主线程执行。"),
    ("src/tools/dispatch.hpp", CTX_FOREIGN,
     "工具分发在 transport 线程发起，只有提交到队列后才在主线程执行。"),
    ("src/tools/dynamic_spec_store.hpp", CTX_FOREIGN,
     "动态工具规格由 transport 线程读写。"),
    ("src/tools/ready_policy.hpp", CTX_FOREIGN,
     "就绪判定在 transport 线程执行。"),
    ("src/tools/register_all.cpp", CTX_FOREIGN,
     "MCP 工具注册回调在 transport 线程执行。"),
    ("src/tools/register_all.hpp", CTX_FOREIGN,
     "MCP 工具注册回调在 transport 线程执行。"),
    ("src/tools/tool_base.hpp", CTX_FOREIGN,
     "工具基类被 transport 线程的元工具路径使用。"),
    ("src/tools/tool_invoke.cpp", CTX_FOREIGN,
     "工具组合调用在发起线程执行。"),
    ("src/tools/tool_invoke.hpp", CTX_FOREIGN,
     "工具组合调用在发起线程执行。"),
    ("src/tools/tool_pipeline.cpp", CTX_FOREIGN,
     "后处理管线随 SpecTool::execute 在发起线程执行。"),
    ("src/tools/tool_pipeline.hpp", CTX_FOREIGN,
     "后处理管线随 SpecTool::execute 在发起线程执行。"),
    ("src/tools/tool_spec.hpp", CTX_FOREIGN,
     "SpecTool::execute 在 call_tool 元工具路径上于 transport 线程执行。"),
    ("src/runtime/*", CTX_MANAGED,
     "游戏进程侧：Godot 调用经 SceneTreeTimer 与 awaiter 排到游戏主线程。"),
    ("src/main.cpp", CTX_MAIN,
     "引擎插件生命周期回调（_enter_tree/_process/_exit_tree）由引擎在主线程驱动。"),
    ("src/core/editor_coords.cpp", CTX_MAIN,
     "纯几何换算，不触碰引擎状态。"),
    ("src/core/editor_coords.hpp", CTX_MAIN,
     "纯几何换算，不触碰引擎状态。"),
    ("src/core/editor_readiness.cpp", CTX_MAIN,
     "编辑器就绪探测只在主线程查询 EditorInterface。"),
    ("src/core/editor_readiness.hpp", CTX_MAIN,
     "编辑器就绪探测只在主线程查询 EditorInterface。"),
    ("src/core/export_guard.cpp", CTX_MAIN,
     "导出回调由引擎导出流程在主线程触发。"),
    ("src/core/export_guard.hpp", CTX_MAIN,
     "导出回调由引擎导出流程在主线程触发。"),
    ("src/core/mode_detector.cpp", CTX_MAIN,
     "运行模式探测在主线程读取引擎单例。"),
    ("src/core/mode_detector.hpp", CTX_MAIN,
     "运行模式探测在主线程读取引擎单例。"),
    ("src/core/monitor_env.cpp", CTX_MAIN,
     "环境快照只在 _enter_tree 的主线程调用。"),
    ("src/core/perf_sampler.cpp", CTX_MAIN,
     "性能采样由 _process 在主线程驱动。"),
    ("src/core/perf_sampler.hpp", CTX_MAIN,
     "性能采样由 _process 在主线程驱动。"),
    ("src/core/plugin_config.cpp", CTX_MAIN,
     "配置读写由主线程触发：配置面板、启动快照与领域工具的授权判定。"),
    ("src/core/plugin_config.hpp", CTX_MAIN,
     "配置读写由主线程触发：配置面板、启动快照与领域工具的授权判定。"),
    ("src/core/resource_registry.cpp", CTX_MAIN,
     "资源注册表只被主线程上的 code_execute 与 resource 工具使用。"),
    ("src/core/resource_registry.hpp", CTX_MAIN,
     "资源注册表只被主线程上的 code_execute 与 resource 工具使用。"),
    ("src/core/scene_dirty_tracker.cpp", CTX_MAIN,
     "脏场景标记由主线程的 _get_unsaved_status 与主线程工具读写。"),
    ("src/core/scene_dirty_tracker.hpp", CTX_MAIN,
     "脏场景标记由主线程的 _get_unsaved_status 与主线程工具读写。"),
    ("src/tools/autopilot_tools.cpp", CTX_MAIN,
     "AutopilotTools 引擎单例的方法由引擎在主线程调用。"),
    ("src/tools/autopilot_tools.hpp", CTX_MAIN,
     "AutopilotTools 引擎单例的方法由引擎在主线程调用。"),
    ("src/tools/debugger_access.cpp", CTX_MAIN,
     "调试器会话访问由编辑器调试器插件在主线程回调。"),
    ("src/tools/debugger_access.hpp", CTX_MAIN,
     "调试器会话访问由编辑器调试器插件在主线程回调。"),
    ("src/tools/debugger_ops.cpp", CTX_MAIN,
     "调试器插件由编辑器在主线程回调。"),
    ("src/tools/debugger_ops.hpp", CTX_MAIN,
     "调试器插件由编辑器在主线程回调。"),
    ("src/tools/editor_ui_actions.cpp", CTX_MAIN,
     "编辑器 UI 动作在主线程执行。"),
    ("src/tools/editor_ui_actions.hpp", CTX_MAIN,
     "编辑器 UI 动作在主线程执行。"),
    ("src/tools/editor_ui_ops.cpp", CTX_MAIN,
     "编辑器 UI 工具经 dispatch 提交到主线程队列执行。"),
    ("src/tools/editor_ui_ops.hpp", CTX_MAIN,
     "编辑器 UI 工具经 dispatch 提交到主线程队列执行。"),
    ("src/tools/runtime_game_ops.cpp", CTX_MAIN,
     "游戏运行时工具经 dispatch 提交到主线程队列执行。"),
    ("src/tools/runtime_ops.cpp", CTX_MAIN,
     "游戏通道编排在编辑器主线程执行：调试器回调或 CommandQueue 提交体。"),
    ("src/tools/runtime_ops.hpp", CTX_MAIN,
     "游戏通道编排在编辑器主线程执行：调试器回调或 CommandQueue 提交体。"),
    ("src/tools/schema_builder.cpp", CTX_MAIN,
     "schema 构造是纯数据处理。"),
    ("src/tools/schema_builder.hpp", CTX_MAIN,
     "schema 构造是纯数据处理。"),
    ("src/tools/tool_args.cpp", CTX_MAIN,
     "参数解析是纯数据处理。"),
    ("src/tools/tool_args.hpp", CTX_MAIN,
     "参数解析是纯数据处理。"),
    ("src/tools/tool_catalog.cpp", CTX_MAIN,
     "工具目录是纯数据索引。"),
    ("src/tools/tool_catalog.hpp", CTX_MAIN,
     "工具目录是纯数据索引。"),
    ("src/tools/tool_registry.hpp", CTX_MAIN,
     "工具注册表只保存静态描述数据。"),
    ("src/tools/*_ops.cpp", CTX_MAIN,
     "领域工具 handler 经 dispatch 提交到主线程队列执行。"),
    ("src/tools/*_ops.hpp", CTX_MAIN,
     "领域工具头文件，实现随同名 _ops.cpp 同线程。"),
    ("src/tools/*_tools.hpp", CTX_MAIN,
     "工具描述头文件，只保存静态描述数据。"),
    ("src/prompts/*", CTX_MAIN,
     "prompt 构造体经 CommandQueue 提交到主线程执行。"),
    ("src/resources/*", CTX_MAIN,
     "资源读取体经 CommandQueue::execute_sync 提交到主线程执行。"),
    ("src/ui/*", CTX_MAIN,
     "编辑器 UI 控件只在主线程构造与回调。"),
    ("src/util/*", CTX_MAIN,
     "工具层纯转换与文件辅助，只被主线程上的 handler 调用。"),
)

FOREIGN_MAIN_THREAD_SCOPES = {
    "src/core/log_persist.cpp": (
        "godot_autopilot::LogPersist::init_session",
        "godot_autopilot::LogPersist::flush_on_main_thread",
        "godot_autopilot::LogPersist::rotate_on_init",
        "godot_autopilot::append_persist_file",
        "godot_autopilot::prune_persist_dir",
    ),
}

KNOWN_FOREIGN_API_SITES = (
    ("src/core/log_persist.cpp",
     "godot_autopilot::LogPersist::store_trace_image",
     "已知越界：transport 线程直接落盘 trace 截图，违反主线程契约；"
     "改为入主线程缓冲后应移除本条。"),
)

GODOT_API_PATTERNS = (
    ("godot::FileAccess", r"\bgodot::FileAccess\b"),
    ("godot::DirAccess", r"\bgodot::DirAccess\b"),
    ("godot::EditorInterface", r"\bgodot::EditorInterface\b"),
    ("godot::Engine", r"\bgodot::Engine\b"),
    ("godot::SceneTree", r"\bgodot::SceneTree\b"),
    ("godot::ClassDB", r"\bgodot::ClassDB\b"),
    ("godot::ClassDBSingleton", r"\bgodot::ClassDBSingleton\b"),
    ("godot::ProjectSettings", r"\bgodot::ProjectSettings\b"),
    ("godot::Input", r"\bgodot::Input\b"),
    ("godot::DisplayServer", r"\bgodot::DisplayServer\b"),
    ("godot::OS", r"\bgodot::OS\b"),
    ("godot::Performance", r"\bgodot::Performance\b"),
    ("godot::Resource", r"\bgodot::Resource\b"),
    ("godot::ResourceLoader", r"\bgodot::ResourceLoader\b"),
    ("godot::ResourceSaver", r"\bgodot::ResourceSaver\b"),
    ("godot::Ref<godot::Resource>", r"\bgodot::Ref\s*<\s*godot::Resource\b"),
)

CONTROL_KEYWORDS = frozenset({
    "if", "for", "while", "switch", "catch", "do", "else", "return", "throw",
    "sizeof", "decltype", "new", "delete", "case", "default", "co_await",
    "co_return", "co_yield", "alignof", "static_assert", "noexcept", "throw",
})

DEFINITION_SUFFIXES = ("{", "const", "noexcept", "override", "final", "->", ":")


class Scope:
    __slots__ = ("kind", "name", "qualified", "parent", "start", "end")

    def __init__(self, kind, name, qualified, parent, start, end):
        self.kind = kind
        self.name = name
        self.qualified = qualified
        self.parent = parent
        self.start = start
        self.end = end


def _blank(chunk):
    return "".join("\n" if ch == "\n" else " " for ch in chunk)


def _mask_literals(text):
    out = []
    n = len(text)
    i = 0
    while i < n:
        ch = text[i]
        if ch in "RULu":
            m = RAW_START.match(text, i)
            if m:
                closer = ")" + m.group(1) + '"'
                end = text.find(closer, m.end())
                end = n if end == -1 else end + len(closer)
                out.append(_blank(text[i:end]))
                i = end
                continue
        if ch == '"':
            j = i + 1
            while j < n:
                cur = text[j]
                if cur == "\\":
                    j += 2
                    continue
                if cur == '"':
                    j += 1
                    break
                if cur == "\n":
                    j += 1
                    break
                j += 1
            out.append(_blank(text[i:j]))
            i = j
            continue
        if ch == "'":
            j = i + 1
            while j < n:
                cur = text[j]
                if cur == "\\":
                    j += 2
                    continue
                if cur == "'":
                    j += 1
                    break
                if cur == "\n":
                    j += 1
                    break
                j += 1
            out.append(_blank(text[i:j]))
            i = j
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def _strip_comments(text):
    out = []
    n = len(text)
    i = 0
    while i < n:
        if text[i] == "/" and i + 1 < n and text[i + 1] == "/":
            end = text.find("\n", i)
            end = n if end == -1 else end
            out.append(_blank(text[i:end]))
            i = end
            continue
        if text[i] == "/" and i + 1 < n and text[i + 1] == "*":
            end = text.find("*/", i + 2)
            end = n if end == -1 else end + 2
            out.append(_blank(text[i:end]))
            i = end
            continue
        out.append(text[i])
        i += 1
    return "".join(out)


def _match_paren(text, open_index):
    depth = 0
    n = len(text)
    i = open_index
    while i < n:
        ch = text[i]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def _definition_name(pending, pending_start, full_text):
    text = pending.strip()
    if not text:
        return None, None
    lead = len(pending) - len(pending.lstrip())
    m = re.search(r"\bnamespace\s+([A-Za-z_][A-Za-z0-9_]*)", text)
    if m:
        return "namespace", m.group(1)
    m = re.search(r"\b(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)", text)
    if m:
        return "class", m.group(1)
    m = re.search(r"\benum\s+(?:class\s+)?([A-Za-z_][A-Za-z0-9_]*)", text)
    if m:
        return "class", m.group(1)
    for m in re.finditer(r"([A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_~][A-Za-z0-9_]*)*)\s*\(", text):
        name = m.group(1)
        simple = name.split("::")[-1]
        if simple in CONTROL_KEYWORDS:
            continue
        open_index = pending_start + lead + m.end() - 1
        close_index = _match_paren(full_text, open_index)
        if close_index < 0:
            continue
        tail = full_text[close_index + 1:].lstrip()
        if not tail.startswith(DEFINITION_SUFFIXES):
            continue
        before = text[:m.start()].rstrip()
        if before.endswith(".") or before.endswith("->"):
            continue
        if "=" in before:
            continue
        return "function", name
    return None, None


def _namespace_prefix(stack):
    for scope in reversed(stack):
        if scope.kind == "namespace":
            return scope.qualified
    return ""


def _build_scopes(text):
    scopes = []
    stack = []
    pending = []
    pending_start = 0
    n = len(text)
    i = 0
    while i < n:
        ch = text[i]
        if ch == "{":
            kind, name = _definition_name("".join(pending), pending_start, text)
            parent = stack[-1] if stack else None
            if kind == "namespace":
                prefix = _namespace_prefix(stack)
                qualified = prefix + "::" + name if prefix else name
            elif kind == "class":
                prefix = _namespace_prefix(stack)
                qualified = prefix + "::" + name if prefix else name
            elif kind == "function":
                prefix = _namespace_prefix(stack)
                def_name = name
                if "::" not in def_name and parent is not None and parent.kind == "class":
                    def_name = parent.name + "::" + def_name
                qualified = prefix + "::" + def_name if prefix else def_name
            else:
                kind = "block"
                name = None
                qualified = parent.qualified if parent is not None else ""
            scope = Scope(kind, name, qualified, parent, i, n)
            scopes.append(scope)
            stack.append(scope)
            pending = []
            pending_start = i + 1
            i += 1
            continue
        if ch == "}":
            if stack:
                stack[-1].end = i
                stack.pop()
            pending = []
            pending_start = i + 1
            i += 1
            continue
        if ch == ";":
            pending = []
            pending_start = i + 1
            i += 1
            continue
        if not pending:
            pending_start = i
        pending.append(ch)
        i += 1
    return scopes


def _chain_at(scopes, offset):
    best = None
    for scope in scopes:
        if scope.start <= offset < scope.end:
            if best is None or scope.start > best.start:
                best = scope
    chain = []
    cur = best
    while cur is not None:
        chain.append(cur)
        cur = cur.parent
    chain.reverse()
    return chain


def _function_scopes(chain):
    return [scope.qualified for scope in chain if scope.kind == "function"]


def _context_of(rel_path):
    for pattern, context, _reason in THREAD_CONTEXT_RULES:
        if fnmatch.fnmatchcase(rel_path, pattern):
            return context
    return CTX_FOREIGN


def _collect_files(scan_dir):
    if not scan_dir.is_dir():
        return []
    found = list(scan_dir.rglob("*.cpp")) + list(scan_dir.rglob("*.hpp"))
    return sorted(found, key=lambda p: p.as_posix())


def _display_path(path, scan_dir):
    try:
        return path.relative_to(ROOT).as_posix()
    except ValueError:
        return path.relative_to(scan_dir).as_posix()


def _line_starts(text):
    starts = [0]
    for idx, ch in enumerate(text):
        if ch == "\n":
            starts.append(idx + 1)
    return starts


def _line_of(starts, offset):
    low = 0
    high = len(starts) - 1
    while low < high:
        mid = (low + high + 1) // 2
        if starts[mid] <= offset:
            low = mid
        else:
            high = mid - 1
    return low + 1


def scan_file(rel_path, text):
    masked = _strip_comments(_mask_literals(text))
    scopes = _build_scopes(masked)
    starts = _line_starts(text)
    context = _context_of(rel_path)
    if context != CTX_FOREIGN:
        return [], []
    allowed = FOREIGN_MAIN_THREAD_SCOPES.get(rel_path, ())
    known = [(path, scope) for path, scope, _reason in KNOWN_FOREIGN_API_SITES
             if path == rel_path]
    violations = []
    notes = []
    seen = set()
    source_lines = text.split("\n")
    for symbol, pattern in GODOT_API_PATTERNS:
        for m in re.finditer(pattern, masked):
            chain = _chain_at(scopes, m.start())
            functions = _function_scopes(chain)
            if any(name in allowed for name in functions):
                continue
            line = _line_of(starts, m.start())
            scope = functions[-1] if functions else ""
            record = (rel_path, line, scope)
            if record in seen:
                continue
            seen.add(record)
            snippet = source_lines[line - 1].rstrip("\r").strip()[:SNIPPET_MAX]
            if any(scope == known_scope for _path, known_scope in known):
                notes.append((rel_path, line, snippet, symbol, scope))
                continue
            violations.append((rel_path, line, snippet, symbol, scope))
    return violations, notes


def main():
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except Exception:
        pass
    try:
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass
    scan_dir = SCAN_DIR
    if len(sys.argv) > 1:
        scan_dir = Path(sys.argv[1]).resolve()
    decode_errors = []
    violations = []
    notes = []
    for path in _collect_files(scan_dir):
        rel = _display_path(path, scan_dir)
        try:
            data = path.read_bytes()
        except OSError as exc:
            decode_errors.append(rel + ": read error: " + str(exc))
            continue
        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError as exc:
            decode_errors.append(rel + ": decode error: " + str(exc))
            continue
        found, noted = scan_file(rel, text)
        violations.extend(found)
        notes.extend(noted)
    if decode_errors:
        for item in decode_errors:
            print(item, file=sys.stderr)
        return 2
    for rel, line, _snippet, _symbol, scope in notes:
        print("known-foreign-api-site: " + rel + ":" + str(line) + ": " + scope,
              file=sys.stderr)
    if violations:
        for rel, line, snippet, _symbol, _scope in violations:
            print(rel + ":" + str(line) + ": " + snippet)
        return 1
    print("main-thread-guard OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
