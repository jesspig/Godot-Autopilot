---
type: 模块文档（设计+实现）
title: ToolSpec 工具数据层与执行管线
description: 以 ToolSpec/ParamSpec 数据化声明工具（schema 从参数表派生）、SpecTool 统一执行授权与后处理、Args 取参器与用户脚本动态工具，取代 GDA_TOOL_CLASS 宏与真类化旧架构
tags:
  - 设计
  - 工具架构
  - ToolSpec
  - 数据化
timestamp: "2026-09-21T01:24:00+08:00"
resource: src/tools/
---

# ToolSpec 工具数据层与执行管线（设计定稿 + 全量迁移）

> **当前 API 面（2026-09-21 复核）**
> `tool_spec.hpp`：`ToolSpec` 数据记录（name/description/category/tags/side_effect/flags/params/handler/raw_schema）+ `ParamSpec`（= `schema::ParamDef`）+ `tool_flags` 位标志 + `SpecTool : ToolBase, ISideEffect` + `make_spec_tool`；schema 由 `params` 经 `schema::build_schema` 派生，`raw_schema` 非空时优先。
> `tool_args.hpp/cpp`：`Args` 取参器 + `ToolArgError`（opt_/require_/get_ 系列 + `reject_unknown`，空值视为缺失）。
> `tool_pipeline.hpp/cpp`：`pipeline::run_post`（`SpecTool::execute` 授权门 → handler 之后的后处理：按 `kObserve` 合并编辑器截图、按 `kSceneTarget` 幂等补全场景路径）。
> `register_all.hpp/cpp`：`build_registry` / `refresh_derived` / `refresh_dynamic_tools` / `get_active_registry`。
> `dynamic_spec_store.hpp` + `autopilot_tools.{hpp,cpp}`：用户脚本动态工具（GDScript `AutopilotTools` 单例，含目录扫描 `rescan`）。
> `tool_invoke.hpp/cpp`：工具间内部组合调用 `tools::invoke_tool` + `invoke_depth()`（深度上限 8）；09-20 起兼作 trace 上下文载体（`SpanGuard`/`capture_trace_context`/`ScopedTraceContext`/`queued_wait_ms`），每次 `SpecTool::execute` 记录一条 `TraceEvent`（缓冲 `TraceRecorder`，落盘 `LogPersist`，见 [核心模块](modules/core.md)）。09-21 起埋点统一经 `src/core/monitor.{hpp,cpp}` 门面（`monitor::tool_call` 等）单次写入，`TraceContext` 增 `request_id`，新增 `RequestSpanGuard` 承载协议请求 span（`RequestContext::GetRequest().id` 建立 `monitor::RequestScope`）。
> 相关页面：[工具注册表](modules/tools_registry.md) · [工程约定](conventions.md) · [测试体系](tests.md) · [架构总览](overview.md)

## 目标与约束

- **一个工具 = 一份数据 + 一个 handler 函数**：元信息、参数表、副作用与路由标志收敛为 `ToolSpec` 值；行为集中在 `<域>_ops.cpp` 的 `handle_xxx`，不再为每个工具生成独立 C++ 子类。
- **schema 单一源**：工具输入 schema 由参数表（`std::vector<ParamSpec>`）在 `SpecTool` 构造时经 `schema::build_schema` 生成；只有嵌套结构（search_tools/batch_execute/code_execute 等 3 个元工具）才在 `raw_schema` 手写 JSON。旧的 `tool_input_schema(name, basic)` / `build_schema_for` / `schema_fills.hpp` / 8 个 `schema_*_ops.cpp` 已整体删除。
- **行为对等（红线）**：不改变对外输出键、`{"error": msg}` 错误语义、参数默认值，以及 `execute()` 仅在 Godot 主线程被调用的保证。
- **自动注册推导而非运行时发现**：C++ 无反射，仍由各域 `make_tools()` 显式清单返回 `vector<unique_ptr<ToolBase>>`，`register_all.cpp` 汇总；把"新增工具改多处"收敛为"参数表 + `make_spec_tool(ToolSpec{...})` 一行 +（仅新 `.cpp` 时才动）CMake"。

## ToolSpec 数据层

```cpp
struct ToolSpec {
  std::string name;
  std::string description;
  std::string category;
  std::vector<std::string> tags;
  SideEffect side_effect = SideEffect::None;
  uint32_t flags = tool_flags::kNone;
  std::vector<ParamSpec> params;               // ParamSpec = schema::ParamDef
  std::function<mcp::JsonValue(const mcp::JsonValue &)> handler;
  mcp::JsonValue raw_schema;                   // 非空对象时优先于 params 生成 schema
};

inline std::unique_ptr<ToolBase> make_spec_tool(ToolSpec spec);
```

- `ParamSpec`/`schema::ParamDef` 四字段：`{name, type, description, required}`；`type` 为 JSON schema 类型字符串（`string`/`integer`/`number`/`boolean`/`object`/`array`）。
- `tool_flags` 位标志（`tool_flags::kNone` + 7 个位）：`kMeta`（元工具归类）、`kDynamic`（用户脚本注册）、`kMutating`（改动状态，遍历排除依据）、`kObserve`（支持 `observe` 参数合并截图）、`kCaptureImage`（图片附件判定）、`kSceneTarget`（成功响应幂等补全场景路径）、`kUndoable`（纯标记）。收编终态：`kCaptureImage` 9 工具 / `kSceneTarget` 25 工具 / `kUndoable` 10 工具（源码静态统计，详见下节 traits 小节）。
- 域侧写法：`const std::vector<ParamSpec> kXxxParams = {...};` 参数表与工具名一一对应，`make_tools()` 中 `v.push_back(make_spec_tool(ToolSpec{name, description, category, {tags}, side_effect, flags, kXxxParams, handler}))`（`side_effect` 取 `SideEffect` 枚举 8 值，见 [安全边界与并发契约](security_contract.md)）。

## SpecTool 与执行管线

```cpp
class SpecTool : public ToolBase, public ISideEffect {
  // meta()/input_schema()/side_effects()/tool_flags()/execute()/spec()
};
```

`SpecTool` 构造时固化三件事：`schema_`（`raw_schema` 优先，否则 `build_schema(params)`）、`meta_`（name/description/category/tags）、`spec_`。`execute()` 的执行顺序固定：

1. 授权门 `authorization::deny_if_unauthorized(name, side_effect)`——命中 `process`/`code_execute`/`game_runtime` 等 capability 未授权时直接返回 `{"error": ..., "authorization_required": ...}`；
2. `handler(args)`；
3. `pipeline::run_post(spec, args, result)`——`flags` 含 `kObserve` 且 `args.observe == true` 时，调用 `capture_ops::handle_capture_viewport({"target":"editor"})` 并把 `data/format/width/height/path` 合并进结果对象（失败写 `observe_error`）；`flags` 含 `kSceneTarget` 且响应成功（无 `error`）时，若 `result.scene_path` 缺失则幂等补全编辑器场景信息（`util::add_scene_info_fields` + `edited_scene_info()`，不做脏标记）；`kUndoable` 为纯标记，post 无动作（各 handler 自管 undo）；其余情况原样返回。

`kObserve` 后处理自 2026-09-19 起统一由管线提供，Input 域 4 个合成输入工具（`click_input_mouse`/`scroll_input_mouse`/`drag_input_mouse`/`type_input_text`）与 Editor 域 2 个元素操作工具（`click_editor_element`/`type_editor_element_text`）已收编，`input_click_ops.cpp` 与 `editor_ui_actions.cpp` 中此前的重复实现已删除。

## 执行埋点与可重放 trace（09-20 新增）

`SpecTool::execute` 在授权门之前解析/继承 trace 上下文（`trace_id = current_trace_id()`，为空则新建；`span_id = new_span_id()`；`parent_span = current_span_id()`），随后记录一条 `TraceEvent` 并追加 `LogSystem::log_detailed` 诊断行。09-21 起统一经 `monitor::tool_call` 单次写入（TraceEvent + 人类日志，人类日志携带 `trace_id`/`span_id`），拒绝/正常路径均补齐 `name`/`request_id`/`thread_id`/`monotonic_ns`：

- **事件字段**：旧字段 `seq/trace_id/span_id/parent_span/session_id/tool/category/flags/side_effect/depth/thread/queue_wait_ms/duration_ms/wall_start_ms/wall_end_ms/auth/ok/error_code/args_digest/args_truncated/result_size`（图片另附 `image_ref/image_bytes/image_hash/image_width/image_height`）；09-21 追加 `kind/name/request_id/correlation_id/phase/state/monotonic_ns/thread_id/attrs/error_type/stack/bytes`（`to_json_line` 在原键序之后追加，即 jsonl schema v2）；`args_digest` 按 `sanitize_policy::enabled()` 选择脱敏（4000 字符上限，剥离敏感键值）或原始（64000 字符上限）口径
- **授权拒绝路径**：`deny_if_unauthorized` 命中时单独记录一条 `auth="denied"`、`error_code="denied"`、`duration_ms=0` 的事件（`category`/`flags`/`side_effect` 照填），与 handler 错误区分
- **图片事件**：结果对象（或其 `result` 子对象）的 `data` 仅在 `format == "png"` 时收集；`diff_image_data` 只要为字符串即收集（不校验 `format`，空串亦会进入）。命中项计算 FNV-1a 哈希与字节数，`image_width`/`image_height` 取自同 scope 的整型 `width`/`height`（缺省 0），并在 `traces/images/` 落盘（仅脱敏关闭时，`LogPersist::store_trace_image`）；jsonl 行只写 `image_ref`（脱敏时为空，其余图片字段 `image_hash`/`image_bytes`/`image_width`/`image_height` 照写）
- **日志等级**：失败或 `duration_ms > 2000ms`（`kSlowToolMs`）记 Warning，其余 Debug；detail 行附 `trace=/span=/parent=/depth=/queue=/dur=/auth=/err=/args=`，慢工具与 `retryable` 结果分别加 `slow_tool=true`/`retryable=true`
- **`detail` 可见性**：`get_plugin_log` 响应条目只含 `serial`/`timestamp`/`level`/`category`/`message`，不含 `detail` 字段；detail 仅在 McpLogDock（"Detail" 开关，默认关闭）与落盘日志（`format_human_line(..., include_detail=true)`）中可见
- **落盘链路**：事件进入 `TraceRecorder` 内存缓冲（容量 20000），主线程 `LogPersist::flush_on_main_thread()` 增量转写为 `traces/trace-<stamp>.jsonl`（首行含 `session_start`，服务器就绪后追加 `server_ready`，结构键见 [核心模块](modules/core.md)）；09-21 起 jsonl 内事件按 `kind` 区分（`lifecycle`/`protocol_request`/`protocol_response`/`tool_call`/`persist_health`/`perf`/`snapshot`/`concurrency` 等，schema v2 新键齐备），L2 `30_observability` 对字段与 kind 做断言

## traits 收编终态（kCaptureImage/kSceneTarget/kUndoable）

- **`kCaptureImage`（9 工具，图片附件单一来源）**：`register_all.cpp` 的 `call_tool` 回调按 flag 判定——registry 已知工具含该 flag 时经 `util::try_attach_image_data` 附加截图 image content；仅未知工具名才走旧白名单 `is_image_capture_tool` 兜底（`util::try_attach_image_content`，`src/util/mcp_image_content.hpp`）。
- **`kSceneTarget`（25 工具，成功响应幂等补全）**：见上节 `run_post` 第 3 步；仅补缺失的 `scene_path`/`scene_unsaved`，不做脏标记。
- **`kUndoable`（10 工具，纯标记）**：各 handler 自管 undo，`run_post` 无动作。
- 计数为源码静态统计（`src/tools/*_tools.hpp` 的 flag 出现次数减 `tool_spec.hpp` 定义行），运行时以 `get_tool_detail` 回显为准。

- **线程契约**：handler 经 `dispatch::call_handler` →（非主线程时）`CommandQueue::execute_sync` 路由到 Godot 主线程执行；唯一例外是 `call_tool` 元工具的编排回调在 MCP 线程执行（等待游戏响应/截图定型，不触碰 Godot API），领域工具 handler 仍由 dispatch 路由回主线程。类内部不得另起线程触碰 Godot API。
- 错误直接返回 `{"error": msg}` JSON 对象（构造用 `util::error_json`/`util::ok_result` 助手）。
- 导出禁用不做角色接口：`server.RegisterTool` 回调统一前置 `ExportGuard::is_exporting()` 检查；异步 `__gda_pending` 约定由 `call_tool` 的等待逻辑统一处理（`runtime_ops::wait_pending_response`）。

## Args 取参器（tool_args.hpp）

`Args(raw, params)` 基于工具参数表提供类型化读取：

- `has`/`is_null`；`opt_string`/`opt_int`/`opt_number`/`opt_bool`/`opt_object`/`opt_array`（返回 `std::optional` 或指针）；
- `require_*` 缺失抛 `ToolArgError("missing required parameter: <name>")`；类型不符抛 `"invalid parameter: <name> must be a <type>"`；
- `get_*` 带默认值；`reject_unknown()` 按参数表拒绝未知键（`"unknown parameter: <name>"`）。

语义要点：`null` 一律视为缺失；`integer` 接受整数值的 JSON double（拒绝小数、拒绝越界）；`boolean` 严格（`1`/`0` 报错）；`reject_unknown` 对非对象输入直接放行。L1 `tool_args_test.cpp` 覆盖 11 项。

## ToolRegistry 与注册派生

`ToolRegistry` 仍是工具单一来源：`add()` 按 `flags & kMeta` 自动归入 meta_（其余进域工具表），`find`/`find_meta`/`find_any`/`all`/`all_meta`/`all_any`/`size`/`meta_size`/`categories` 全量可查；`make_tool_info` 把 `kDynamic`/`kMutating` 映射为 `ToolInfo.dynamic/mutating` 供 catalog 与遍历使用。注册管线细节见 [工具注册表](modules/tools_registry.md)。

## 动态工具（用户脚本 API）

`AutopilotTools`（`GDCLASS` 绑定，GDScript 侧 `Engine.get_singleton("AutopilotTools")`）暴露：

| 方法 | 语义 |
|---|---|
| `register_tool(definition, callable)` | 注册用户工具，返回 handle（失败返回 -1 并 `push_error`） |
| `unregister_tool(handle)` | 按 handle 移除，成功触发重建 |
| `has_tool(name)` / `list_tools()` / `get_tool(name)` | 查询已注册工具（get_tool 含 name/description/category/tags/side_effect/params/handle） |
| `call_tool(name, args)` | 直连 dispatch 调任意工具（自动回主线程） |
| `is_enabled()` / `set_enabled(bool)` | 读写 `user_tools` capability（写 `user://godot_autopilot/config.json` 的 `allow` 字段） |

- **definition 字段**：`name`（必填、非空）、`description`、`category`（默认 `User`）、`tags`、`side_effect`（空/`writes_file`/`writes_config`/`shows_alert`/`modifies_window`/`process`/`code_execute`/`game_runtime`）、`params`（`[{name, type, description, required}]`）。
- **存储与重建**：spec 存入 `dynamic_specs::store()`（`dynamic_spec_store.hpp`，互斥保护），`register_tool`/`unregister_tool` 成功后调 `refresh_dynamic_tools()` 重建 registry（动态 spec 带 `kDynamic` flag）；重名（已注册或与内置工具碰撞）拒绝注册。
- **执行门**：注册与调用均要求 `user_tools` capability（env `GODOT_AUTOPILOT_ALLOW` 优先于配置 `allow`；dock 复选框 "Allow user tools"）。callable 目标对象被释放后调用报"目标已失效，请重新注册"。
- **生命周期**：`AutopilotTools` 析构清空 spec store；编辑器中单例随插件加载注册。

### 目录扫描 rescan

`AutopilotTools::rescan(directory)`（默认 `res://addons/godot-autopilot-tools`）批量注册目录内用户工具，流程固定：`user_tools` 门禁 → 非主线程时经队列 marshal 回主线程 → 仅接受 `res://`/`user://` 前缀 → 递归收集 `.gd` 文件（上限 256 个、深度 8、跳过隐藏条目）→ 逐文件 `ResourceLoader` 加载 + `new` 实例化 + 以单例为参数调用 `register_autopilot_tools(api)`，单文件异常记入 `errors` 不中断扫描 → 返回 `{scanned, registered, failed, errors}`（`failed` 为整型标量）。

- **脚本约定**：实例须提供实例方法 `register_autopilot_tools(api)`，方法内调 `api.register_tool(definition, Callable(...))`；缺该方法的文件记 error 跳过；同名（含与内置工具碰撞）注册被拒绝，故二次 rescan 同一目录无新增 `registered`（幂等）。
- **实例处置**：调用完成后无父节点的 `Node` 型实例会被释放（`memdelete`），非 `Node` 非 `RefCounted` 同样释放——仅 `RefCounted` 派生宿主安全（示例均用 `RefCounted`）。
- **示例**：`samples/user-tools/echo_tool.gd`（GDScript）与 `echo_tool.cs`（C# 文档级示例：rescan 只扫描 `.gd`，C# 需编译后手动注册；`dotnet 10.0.302` 本机可用，但仓库无 .NET 编译验证环节，未经本仓库 CI 验证）。
- **覆盖**：L2 `tests/config/27_user_tools_rescan.json` 端到端（写 probe → rescan 注册 → MCP 调用 → 二次 rescan 幂等 → 清理；需 `code_execute` + `user_tools` 授权，env 优先陷阱同 26 号）。

### C#（.NET 版编辑器）鸭子类型示例

C# 无法继承 GDExtension 类（Godot 限制），不能像 GDScript 那样直接按方法签名调用 `AutopilotTools`，需走**鸭子类型**：`Engine.GetSingleton("AutopilotTools")` 返回 `GodotObject`，全部交互经 `Call(...)` 字符串方法名完成。示例（回显工具：注册 → 调用 → 注销）：

```csharp
using Godot;
using Godot.Collections;

// Invoke 宿主：任意持久对象（这里用 RefCounted 派生）；需保持引用存活，
// 宿主被释放后调用报"目标已失效，请重新注册"
public partial class EchoProbe : RefCounted
{
    public Dictionary Invoke(Dictionary args)
    {
        string value = args.TryGetValue("value", out Variant v) ? v.AsString() : "";
        return new Dictionary { { "echo", value } };
    }
}

public partial class UserToolBootstrap : Node
{
    public override void _Ready()
    {
        // C# 不能继承 GDExtension 类，单例以 GodotObject 鸭子类型接收
        GodotObject api = Engine.GetSingleton("AutopilotTools");
        if (api == null)
        {
            GD.PushError("AutopilotTools singleton not registered");
            return;
        }

        var probe = new EchoProbe();

        var definition = new Dictionary
        {
            { "name", "user_echo_probe" },
            { "description", "Echo probe registered from C#" },
            { "category", "User" },
            { "tags", new Array { "user", "csharp" } },
            { "side_effect", "" },
            { "params", new Array
                {
                    new Dictionary
                    {
                        { "name", "value" },
                        { "type", "string" },
                        { "description", "Value to echo back" },
                        { "required", true },
                    },
                }
            },
        };

        Variant handle = api.Call("register_tool", definition,
            Callable.From<Dictionary, Dictionary>(probe.Invoke));

        // 经 dispatch 调用任意工具（自动回主线程），含刚注册的用户工具
        Variant result = api.Call("call_tool", "user_echo_probe",
            new Dictionary { { "value", "hi" } });

        bool removed = api.Call("unregister_tool", handle).AsBool();
        GD.Print($"handle={handle} result={result} removed={removed}");
    }
}
```

- 对应关系：GDScript 的 `get_singleton` / `Callable(probe, "invoke")` 分别对应 C# 的 `GetSingleton` / `Callable.From<Dictionary, Dictionary>(probe.Invoke)`；`register_tool` / `call_tool` / `unregister_tool` 的方法名与参数顺序不变。
- 宿主与编译前提：需在 **.NET 版 Godot 编辑器**中打开项目，且 C# 脚本先经编辑器编译（构建）后再执行注册；`Invoke` 宿主需保持引用存活（示例用 `RefCounted` 派生承载）。
- 授权门同上：注册与调用要求 `user_tools` capability（env `GODOT_AUTOPILOT_ALLOW` 优先于配置 `allow`）。
- **该 C# 示例为文档级示例，未经本仓库 CI/自动化验证**；GDScript 路径由 `tests/config/26_user_tools_code_mode.json` 端到端覆盖（注册 → MCP 调用 → 发现性 → code-mode 直连 → 注销）。

## 内部组合调用（tool_invoke）

`src/tools/tool_invoke.{hpp,cpp}` 提供服务端正式的工具间组合通道，供 handler 内部调用其他工具而不经过 MCP 往返：`tools::invoke_tool(name, args)` 按序处理导出态拦截（`ExportGuard::is_exporting()`，直接返回固定 error）→ 深度守卫（线程局部计数超 `kMaxInvokeDepth = 8` 返回 `tool invoke depth exceeded (max 8)`）→ `dispatch::call_handler` → 异步 pending 嵌套拦截（含 `__gda_pending` 时拒绝在主线程内嵌套调用异步工具）。诊断函数 `invoke_depth()` 返回当前线程组合调用深度。L1 `tests/unit/tool_invoke_test.cpp` 覆盖 4 项（未知透传/成功透传/pending 拦截/深度守卫）；导出分支无公开 setter，L1 未覆盖，注释已声明（由代码审查覆盖）。

trace 相关扩展（09-20）：`invoke_tool` 在正常路径用 `SpanGuard(trace, span)` 压栈使嵌套调用继承父 span；三条拒绝路径（`export_blocked`/`depth_exceeded`/`async_nested`）也各记一条 `TraceEvent`+`log_detailed`。线程局部 span 栈经 `current_trace_id()`/`current_span_id()` 读取，跨线程时由 `capture_trace_context()`（HTTP 线程捕获取值）与 `ScopedTraceContext`（主线程执行前恢复，析构还原）传递——全仓共 22 处 `ScopedTraceContext` 恢复点（`grep -n ScopedTraceContext src/` 静态统计；`tool_invoke.hpp/cpp` 的 7 处为类型声明与构造/析构实现，不计入）：`resource_handlers.cpp` 8、`debugger_resources.cpp` 7、`autopilot_tools.cpp` 3（动态工具分发/call_tool/rescan）、`code_exec_ops.cpp` 2（batch_execute 的 undo 快照/回滚）、`register_all.cpp` 1（元工具 RegisterTool 回调）、`dispatch.cpp` 1（`call_handler`）；`set_queued_wait_ms()`/`take_queued_wait_ms()` 在 `dispatch::call_handler` 的 `execute_sync` 包装层测量排队等待并一次性取走；`invoke_thread_label()` 依 `runtime_ops::has_editor_queue()` 与 `CommandQueue::is_main_thread()` 标注 `main`/`non-main`。09-21 起 `record_invoke_trace` 走 `monitor::tool_call`，新增 `RequestSpanGuard`（压入协议请求 span，与元工具 `RequestScope` 配合使域工具 span 嵌套在请求 span 下），`TraceContext` 增 `request_id`（`capture_trace_context`/`ScopedTraceContext` 一并保存恢复）。

## batch_execute 最小变量串联

`batch_execute` 在 `src/tools/code_exec_ops.cpp` 内支持最小变量串联（无 schema 改动）：整字符串精确替换——`$prev`（上一操作的成功/失败载荷整体）、`$steps[N].result` / `$steps[N].error`（指定序号操作的成功/失败载荷，支持嵌套结构内替换；非整字符串的混写如 `"prefix $prev suffix"` 保持原样）。不可解析时该步骤记 error，前缀 `unresolvable reference`（首操作用 `$prev`、前向引用、状态错配如对失败操作取 `.result`、序号越界均属此类），走既有 `stop_on_error` 语义；无条件分支、无并行。L1 `tests/unit/batch_refs_test.cpp` 覆盖 6 项。

## 迁移规则与守卫

数据化迁移已全量完成（30 域 + `system_status` + 7 元工具全部为 `SpecTool`），以下旧机制**禁止回退**：

- 旧宏 `GDA_TOOL_CLASS` / `GDA_TOOL_CLASS_SIDE`（`tool_decl.hpp` 已删）；
- `fn_tool.hpp` / `FnTool` / `make_fn_tool`、`meta_tools.hpp` / `MetaTool` / `IMetaTool`（元工具现按 `kMeta` flag 归类）；
- `tool_input_schema` / `build_schema_for` / `schema_fills.hpp` / `schema_*_ops.cpp`（schema 现从参数表派生）；
- `tool_defs.def`（历史已删）。

守卫位于 `tests/guard/`：`migration_guard.cmake` 读 `migrated_domains.txt`（30 个域 + `strict` 行）做零依赖源码断言——每个已迁移域 `src/tools/<域>_tools.hpp` 不含旧宏；strict 模式追加全域文件无旧宏、无 `schema_*_ops.cpp` 残留、全仓 `src/tools/*.hpp|*.cpp` 无 `tool_input_schema` / `tools/tool_decl.hpp` 引用。运行方式：`ctest --preset debug -R migration_guard`，或 `cmake -DSOURCE_DIR=<仓库根> -P tests/guard/migration_guard.cmake`（实测输出 `OK (domains: 30, strict: on)`）。

## 迁移历史

1. **2026-08-20 加层不改**：引入 `ToolBase` 接口 + `FnTool` 适配器 + `ToolRegistry` 单一来源，`tool_defs.def` 保留。
2. **2026-08-21 宏真类化**：`GDA_TOOL_CLASS`/`GDA_TOOL_CLASS_SIDE` 生成独立子类，删除 `tool_defs.def`；元工具改 `IMetaTool` 接口 + 组合。
3. **2026-08-22 收尾清理**：删除 `ArgReader`/`make_ok`/`make_error`/`IExportGuard`/`IAsync` 等未兑现抽象，`FnTool` 构造收缩。
4. **2026-09-19 数据化重构（本轮）**：`ToolSpec`/`ParamSpec`/`SpecTool`/`Args`/`tool_pipeline` 落地，30 域 391 工具 + `system_status` + 7 元工具全量迁移为数据记录；删除宏与全部旧 schema 文件；新增 `tests/guard/` 严格迁移守卫与用户脚本动态工具 API（`AutopilotTools` + L2 `26_user_tools_code_mode`）。

## 相关页面

- [工具注册表](modules/tools_registry.md)
- [工程约定](conventions.md)
- [测试体系](tests.md)
- [架构总览](overview.md)
- [安全边界与并发契约](security_contract.md)
