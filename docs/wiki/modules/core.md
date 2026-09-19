---
type: 模块文档
title: 核心模块
description: 命令队列、配置常量、日志、模式检测、资源缓存、脏状态跟踪、错误水印、导入就绪门控、服务器生命周期与插件配置
tags:
  - 模块
  - 核心层
  - 线程模型
timestamp: "2026-09-19T17:39:40+08:00"
resource: src/core/
---

# 核心模块（src/core/）

> 审计日期：2026-09-19（2026-08-29 随 0.2.2 版本与全量审计同步；09-02 随安全与并行硬化同步；09-13 上午随资源 path 加载注册进 ResourceRegistry 同步；09-13 下午随收口批次同步 PluginConfig allow 键、授权门与 call_tool MCP 线程例外；09-13 17:50 随 B 组知识库审计同步——补正主线程排空点行号、移除已删除的 architecture.md 对照行；09-13 晚随 0.2.4 版知识库全量审计同步——补正 query_recent/query_from 消费方注释、SCENE/EDITOR 两级初始化描述、GDA_FORCE_HEADLESS 语义与 editor_readiness 消费方；09-14 随修复批次同步——PluginConfig 消费方增列 Allow game_runtime 复选框；09-16 随坐标换算批次同步——补 `editor_coords` 职责行与新增纯函数（此前职责表漏列该模块），基于当前工作树代码逐行核对（不依赖 git 历史）；09-18 随知识库一致性审计同步——`query_recent(50)` 消费方行号 470→527（以 `resource_handlers.cpp` 实测为准）、审计日期头补齐 09-16 改动的日期同步；09-19 随注释清理批同步——坐标换算引擎映射与缓存提示取舍入文档（源码整行注释删除）。
> 覆盖范围：`src/core/` 下 9 个 cpp + 12 个头 + `version.hpp.in` 模板（`CommandQueue` 与 `error_watermark` 为 header-only，实际 12 业务组 + 版本）。注意：`CommandQueue` 为 header-only（仅 `command_queue.hpp`，无对应 `.cpp`），`error_watermark.hpp` 同为 header-only，`version.hpp.in` 经 `configure_file` 生成 `version.hpp`。

## 模块简介

`src/core/` 是插件的基础设施层：负责把 HTTP 线程（mcp-cpp-sdk 自研网络栈）的请求安全地桥接到 Godot 主线程、提供进程内日志、配置常量、运行时模式检测、资源缓存、场景脏状态跟踪，以及 MCP 服务器的生命周期管理。所有模块位于命名空间 `godot_autopilot`（`ResourceRegistry`/`SceneDirtyTracker` 使用 `godot_autopilot::resource_registry` / `godot_autopilot::scene_dirty_tracker` 子命名空间）。

## 职责表

| 模块 | 文件 | 职责 | 消费方 |
|---|---|---|---|
| `CommandQueue` | `command_queue.hpp`（header-only） | 跨线程任务队列：`submit()` 入队并返回 `std::future`，主线程 `drain()` 批量执行 | 所有工具/resource/prompt handler、`main.cpp` |
| 配置常量 | `config.hpp` | 端口、超时、缓冲区上限等编译期常量（GDA_ 前缀） | `server_context.cpp`、`runtime/game_bridge.cpp` 等 |
| `ExportGuard` | `export_guard.cpp/hpp` | 导出期间置位全局原子标志，供工具分发判定"导出中" | `dispatch::export_blocked_result`、`_enter_tree` 注册 |
| `LogSystem` | `log_system.cpp/hpp` | 进程内环形日志（仅内存，无文件输出），单例 | 全部模块、`McpLogDock`、log 类资源 |
| `ModeDetector` | `mode_detector.cpp/hpp` | 运行时模式检测（编辑器/游戏/未知） | `_enter_tree` 启动日志 |
| 坐标换算（`coords` 命名空间，纯函数） | `editor_coords.cpp/hpp` | 仿射/Rect/图像尺寸纯逻辑 + 画布空间→窗口客户区换算（09-16 新增 `viewport_point_to_window` / `viewport_rect_center_to_window`，`screen_transform×canvas` 复合，恒等原样返回；画布空间即 `Control::get_global_rect()`/`get_global_transform()` 所在空间，注入的 `InputEventMouseButton.position` 为窗口客户区坐标（引擎侧经 `get_final_transform()` 反变换）——`src/core/editor_coords.hpp:12-15`） | 游戏侧 `run_ui_click` 注入坐标换算（见[入口与运行时](entry_runtime.md)） |
| `ResourceRegistry` | `resource_registry.cpp/hpp` | 内存资源缓存（oid 键 + `name:` 前缀键），全局 mutex 保护 | 资源类工具 |
| `SceneDirtyTracker` | `scene_dirty_tracker.cpp/hpp` | 记录"当前编辑场景是否被修改"及根节点实例 ID | `GodotAutopilotPlugin::_get_unsaved_status` |
| `error_watermark` | `error_watermark.hpp`（header-only） | 错误水印计数器：累积待消费错误数，供 MCP 响应附 `new_errors_since_last_call` doorbell | `register_all.cpp`（响应后处理）、`runtime_ops.cpp`（游戏 runtime_error 计数） |
| `editor_readiness` | `editor_readiness.cpp/hpp` | 编辑器导入/扫描进行中检测与 retryable 软错误构造，reimport 类工具的门控 | `resource_ops`（reimport/save 类 handler） |
| `ServerContext` | `server_context.cpp/hpp` | MCP 服务器组装、端口解析、启动/停止/重启、工具/资源/prompt 注册 | `main.cpp` 入口 |
| `PluginConfig` | `plugin_config.cpp/hpp` | 插件自身配置持久化（`user://godot_autopilot/config.json`，当前含 port、show_time（Show timestamps 开关持久化，默认 true）与 allow（能力授权列表，默认空=拒绝）） | `ServerContext` 端口解析（`load_port`）、`McpConfigDock` Apply（`save_port`）与 Allow code_execute 复选框（`load_allow`/`save_allow`）、`McpLogDock` 时间前缀（`load_show_time`/`save_show_time`）、`authorization::capability_enabled`（经注册的 AllowProvider 每次调用读取） |
| 版本宏 | `version.hpp.in`（configure_file 模板，生成 `<build>/generated/version.hpp`） | 定义 `GDA_VERSION` 字符串宏，取自根 `VERSION` 文件单一来源 | `server_context.cpp`（MCP `server_info`）、`register_all.cpp`（`system_status.version`） |

## 关键接口清单

### CommandQueue（header-only）

- `template <typename Fn> auto submit(Fn&&) -> std::future<std::invoke_result_t<Fn>>` — 入队任务，返回 future；任务异常通过 `promise.set_exception` 传播
- `bool drain()` — 主线程调用；关闭或线程不匹配时返回 `false`，否则锁定交换批量任务后逐个执行；首次调用固定当前线程为主线程
- `void open()` / `void close()` / `bool is_closed()` — 管理队列生命周期；默认容量为 1024，关闭时拒绝并完成未执行任务的 future
- `execute_sync(Fn&&)` — 主线程直接执行，其他线程提交后等待 future
- `bool is_main_thread() const` — 与记录的线程 ID 比较
- 内部结构：`std::queue<std::unique_ptr<TaskBase>>` + `std::mutex` + 关闭状态 + 容量限制 + 主线程 ID
- 实例：`GodotAutopilotPlugin::s_queue`（静态成员），经静态 `queue()` 访问器暴露；`runtime_ops::set_editor_queue` 也持有同一指针

### config.hpp（全部常量，见文末常量表）

### ExportGuard（`godot::EditorExportPlugin` 子类）

- `static bool is_exporting()` — 查询导出标志
- `_export_begin(...)` 置 `g_exporting = true`；`_export_end()` 置 `false`（`std::atomic<bool>`，relaxed 语义）
- 在 `_enter_tree` 中 `add_export_plugin(export_guard_)` 注册；导出期间领域工具调用返回 `{"error":"editor is exporting; retry after export completes"}`

### LogSystem（单例）

- 枚举：`LogLevel { Debug, Info, Warning, Error }`；`LogCategory { System, Transport, Tools, Resources, Prompts }`
- `void log(LogLevel, LogCategory, const std::string&)` — 环形缓冲，上限 `MAX_ENTRIES = 10000`，超限 `pop_front`；分配递增 `serial`
- `query(const Query&)` — 支持 `min_level` / `filter_text`（大小写不敏感） / `category` 过滤
- `query_recent(size_t limit)` — 取最近 N 条（`godot://log/recent` 资源经 `query_recent(50)` 消费，`resource_handlers.cpp:527`）
- `query_from(size_t start_index, size_t* next_index)` + `size_t next_index()` — 增量查询（`McpLogDock::poll_new_entries` 与 `get_plugin_log` 的 `since_index` 消费，`mcp_log_dock.cpp:217`、`debugger_ops.cpp:683`）
- `static LogSystem& instance()` — 局部静态单例
- **写入目标：仅内存；无文件、无回调、无 Godot 控制台直接输出**（`McpLogDock` 经 `poll_new_entries`/`query_from` 轮询消费）
- MCP 侧消费方：`get_plugin_log`（debugger_ops，09-13 下午新增）经 `query`/`query_from` 读取同一缓冲（级别/分类/子串/增量过滤，值拷贝快照），与 `McpLogDock` 并行消费互不影响

### ModeDetector

- `static RuntimeMode detect()` — `Engine::get_singleton()` 为 null → `Unknown`；否则 `is_editor_hint()` 为真 → `Editor`，反之为 `Game`
- `static bool is_editor()` — `detect() == Editor`

### ResourceRegistry（命名空间函数，非类）

- `register_resource(res, name)` — 以 `instance_id` 为键，另加 `"name:" + name` 键（`name` 为空则只登记 oid 键）
- `lookup_memory(name)` — 先查 `name:` 键；若参数为纯数字再回退查 oid 键；未命中返回空 `Ref`
- `erase_oid(object_id)`
- 线程安全：`std::unordered_map<std::string, godot::Ref<godot::Resource>>` + 全局 `std::mutex`
- **path 加载自动注册（09-13 起）**：`resource_ops::resolve_resource` 经 `path` 加载磁盘文件的实例在返回前调 `register_resource(res, path)`——该实例被注册表持有，跨调用存活，后续可用 `object_id`/`name:`（含原 `res://` 路径字符串）再次定位；这也是"加载后修改、再保存/查询"能在多次工具调用间保持同一实例的原因。

### SceneDirtyTracker（命名空间函数，非类）

- `mark_scene_modified()` — 读取 `EditorInterface::get_edited_scene_root()`，无根节点则不置脏；记录根实例 ID
- `clear_scene_modified()`、`bool is_current_scene_dirty()` — 脏判定要求"有根节点且实例 ID 与记录一致"
- 消费方：`_get_unsaved_status`（决定编辑器"未保存"标记）

### error_watermark（header-only，命名空间 `godot_autopilot::error_watermark`，08-24 新增）

错误水印 doorbell：AI 客户端无需轮询即可感知"上次调用之后发生了多少个新错误"。

- `record_error(int64_t count = 1)` — 累积待消费错误计数（mutex 保护）
- `int64_t consume_new_errors()` — 取走并清零累积值（一次性消费语义）
- `count_response_errors(const mcp::JsonValue&)` — 统计一个响应对象中的错误数：顶层 `error` 字符串计 1；`results` 数组内逐项 `error` 字符串各计 1（覆盖 batch_execute 的子结果）
- 接线点：编辑器侧在 `register_all.cpp` RegisterTool 回调内对每次工具响应调用 `count_response_errors` 并累入水印，随后把 `consume_new_errors()` 结果作为顶层字段 `new_errors_since_last_call` 附到响应上（含 0 值）；游戏侧 `runtime_ops.cpp` 收到 runtime_error 增量时 `record_error()`
- 同一 `call_tool` 回调（09-13 起）还承担截图响应后处理：经 `util::try_attach_image_content`（`util/mcp_image_content.hpp`）把 `capture_editor_viewport`/`capture_game_viewport`/`capture_display_screen` 的 PNG base64 转为 MCP image content 块，文本 JSON 的 `data` 替换为 `"<attached-as-image-content>"` 并加 `image_attached:true`；`batch_execute`/`code_execute` 内的调用不附加 image 块

### editor_readiness（命名空间函数，非类，08-24 新增）

导入/扫描就绪门控，避免 reimport 类调用撞上进行中的文件系统扫描：

- `bool is_import_in_progress()` — 经 `EditorInterface::get_resource_filesystem()` 查 `is_scanning()` 与 `is_importing()`（后者以 `has_method` 探测，兼容引擎版本差异）；接口不可用时返回 false
- `mcp::JsonValue busy_error()` — 构造软错误 `{"error": "editor is currently importing/scanning resources; retry shortly", "retryable": true, "retry_after_ms": 500}`
- 消费方：`resource_ops`（reimport 双入口与 uid 重建）和 `text_ops`（write_file 命中已导入文件）的 reimport 路径——导入中且有实际工作量时返回 busy_error，scan 场景幂等跳过

### ServerContext

- 构造：持有 `CommandQueue&`，创建 `ToolCatalog` 与 `Bm25Index`，`resolve_port()`/`resolve_host()` 解析端口与主机并写 Transport 日志；默认环回绑定 `127.0.0.1`，`ServerContext::start()` 拒绝非环回地址
- `bool start()` — 依次：`StreamableHttpServerTransport`（`host = resolve_host()` 默认 `127.0.0.1`、`port`、`endpoint = "/mcp"`、`stateless = true`、`enable_legacy_sse = false`）→ `mcp::McpServer::Create` → `register_tools()` → `transport_->Start()`；成功后回写 `port_ = http_opts.port`；SDK 自身日志默认关闭（`MCP_LOG_LEVEL` 未设置时为 Off）
- `void stop()` — 幂等关闭 `server_`/`transport_`，清空 dispatch handler 与 active registry；析构函数对非空资源兜底调用
- `bool restart(uint16_t port)` — `stop()` → 更新 `port_` → `start()`；供配置面板运行时改端口（配置面板 Apply 后立即生效，无需重启编辑器）
- `int get_port()` / `bool is_running()` / `const std::string& last_error()`
- MCP 服务器标识：`mcp::Implementation{"godot-autopilot", GDA_VERSION}`（宏经 `configure_file` 由根 `VERSION` 文件生成，见 `build.md` "版本号单一来源"）
- 缓存提示（`server_context.cpp:91-99`）：MCP 2026-07-28 规范要求六类 cacheable 结果携带缓存提示，协商到 2026 era 的客户端会按必填字段校验；`ttlMs=0` 表示"立即过期"，仅满足字段声明，客户端仍每次重新拉取，行为与未声明时一致
- 生命周期回调（全部写 Transport 类别日志）：`on_method_called`（Debug）、`on_client_connected` / `on_initialized` / `on_transport_close`（Info）、`on_protocol_error` / `on_transport_error`（Error）
- 诊断日志增强：初始化 / 注册工具 / 启动传输 / 停止 / 重启等关键流程均补充诊断日志；`start()` 启动失败不再硬编码 "unknown exception"，改为输出捕获到的真实异常类型，便于排查
- `register_tools()` 注册四类：工具、资源、prompt、调试器专用资源/prompt

### PluginConfig（命名空间静态方法，非类实例）

- `int load_port()` — 读 `user://godot_autopilot/config.json` 的 `port` 键；文件不存在/解析失败/非整数时返回 `-1`（表示未配置）
- `bool save_port(int port)` — 写回 `{"port": N}`（先 `DirAccess::make_dir_recursive_absolute` 建目录，复用 `save_config_value` 合并写回保留其他键）；失败记 System 类别错误日志并返回 false
- `bool load_show_time()` — 读 `show_time` 键（`user://godot_autopilot/config.json` 的 `show_time`）；文件不存在/解析失败/非布尔时返回 `true`（默认开启，与配置面板 Show timestamps 开关一致）
- `bool save_show_time(bool show)` — 写回 `{"show_time": bool}`（同经 `save_config_value` 合并写回）；失败记 System 类别错误日志并返回 false
- `std::string load_allow()` — 读 `allow` 键（逗号分隔能力列表）；文件不存在/解析失败/非字符串时返回空串
- `bool save_allow(const std::string &allow)` — 写回 `{"allow": string}`（同经 `save_config_value` 合并写回）
- **AllowProvider 桥接（09-13 下午起）**：`plugin_config.cpp` 的静态对象在构造时调用 `authorization::set_allow_provider(&PluginConfig::load_allow)`，使 `capability_enabled` 在无 `GODOT_AUTOPILOT_ALLOW` 环境变量时改读配置——每次调用实时读取，故配置改动下一次工具调用即生效
- 消费方：`ServerContext::resolve_port()`（启动时 `load_port`）、`McpConfigDock::_on_apply_port()`（Apply 成功后 `save_port`）与 "Allow code_execute" / "Allow game_runtime" 复选框（`load_allow`/`save_allow`）、`McpLogDock` 时间前缀开关（`load_show_time`/`save_show_time`，配置面板持久化）、`authorization`（能力门的 AllowProvider）

## 线程模型

```mermaid
flowchart LR
    Client[MCP Client] -->|POST /mcp 端口 9527| HTTP[SDK HTTP 线程]
    HTTP -->|queue.submit() 返回 future| Q[CommandQueue 互斥队列]
    Q -->|_process 每帧 drain| Main[Godot 主线程]
    Main --> API[Godot API / 场景 / 引擎]
    HTTP -.等待 future.get().|H 调用方阻塞等待结果
```

- HTTP 线程绝不直接触碰 Godot API：领域工具经 `dispatch::call_handler` 判断——非主线程时 `submit(...).get()` 同步等待（`dispatch.cpp`）；资源/prompt 注册同样走 `submit`
- 唯一编排层例外（09-13 下午起）：`call_tool` 元工具回调在 MCP 线程执行（等待运行时响应/截图定型不再经 `execute_sync` 占用主线程），编排逻辑自身不触碰 Godot API、领域工具 handler 仍由 dispatch 路由回主线程；其余 6 个元工具仍走 `execute_sync`
- 排空点唯一：`GodotAutopilotPlugin::_process(double)` 调用 `s_queue.drain()`（`main.cpp`），随后轮询 `McpLogDock`
- `drain()` 首次执行时把当前线程记为"主线程"，此后 `is_main_thread()` 据此判定

## 生命周期（插件 ↔ ServerContext）

1. `GDExtensionEntryPoint`（`main.cpp:258-325`）：`MODULE_INITIALIZATION_LEVEL_SCENE` 在非编辑器进程调用 `game_bridge::register_listener()`（桥接类注册与消息捕获）；`MODULE_INITIALIZATION_LEVEL_EDITOR` 注册 debugger 类与 4 个类后 `EditorPlugins::add_by_type<GodotAutopilotPlugin>()`
2. `_enter_tree`：设置 editor queue → Log Dock/输出捕获/调试器插件 → `new ServerContext(queue)` 并 `start()` → Config Dock → `add_export_plugin(ExportGuard)`；`gda_cmdline_mode()` 为真时跳过 UI/服务器（`GDA_FORCE_HEADLESS=1` 反向禁用 cmdline 模式，语义与变量名相反）
3. `_process`：每帧 `drain()` + Dock 轮询
4. `_exit_tree`：`ServerContext::stop()` + delete → 注销各组件

安全与停止/重载的可执行约束见 [T0 安全边界与并发契约](../security_contract.md)。当前队列关闭会拒绝新任务并为未执行任务完成异常 future；运行时 pending 请求在队列清空时统一取消。

## 环境变量与端口

- `GODOT_AUTOPILOT_PORT`：`resolve_port()` 用 `std::getenv` 读取、`std::atoi` 转换（**无格式校验**）
- 端口解析优先级：**环境变量 > `PluginConfig::load_port()`（user:// 持久化值，需 > 0）> `GDA_DEFAULT_PORT`（9527）**——环境变量优先保证测试/CI 场景不受面板配置影响
- `GODOT_AUTOPILOT_HOST`：`resolve_host()` 用 `std::getenv` 读取，非空即生效；默认环回绑定 `127.0.0.1`，`ServerContext::start()` 拒绝非环回地址
- 运行时改端口：`ServerContext::restart(uint16_t)`（配置面板 Apply 触发，成功后经 `PluginConfig::save_port` 持久化）
- `GDA_FORCE_HEADLESS`：值为 `1` 时**禁用** cmdline 模式（强制建 UI/启服务器，语义与变量名相反；`main.cpp:46-54` 读取）

监听、可信客户端模型、路径和响应大小边界见 [T0 安全边界与并发契约](../security_contract.md)。

## config.hpp 常量全表

| 常量 | 值 | 类型 | 主要用途 |
|---|---|---|---|
| `GDA_DEFAULT_PORT` | `9527` | `int` | MCP HTTP 默认端口 |
| `GDA_HEALTHY_ACTIVITY_THRESHOLD_MS` | `3000` | `int64_t` | `get_game_status` 健康判定阈值（`game_bridge.cpp`） |
| `GDA_DEFAULT_TIMEOUT_MS` | `5000` | `int64_t` | 默认操作超时 |
| `GDA_MAX_TIMEOUT_MS` | `30000` | `int64_t` | 超时上限（与 `code_execute` 工具 30s 上限呼应） |
| `GDA_ERROR_BUFFER_MAX` | `200` | `size_t` | 错误缓冲上限（`game_bridge.cpp`） |
| `GDA_OUTPUT_BUFFER_MAX` | `500` | `size_t` | 输出缓冲上限（`game_bridge.cpp`） |
| `GDA_EVAL_TRUNCATE_BYTES` | `8192` | `size_t` | eval 输出截断字节数（`game_bridge.cpp`） |
| `GDA_CAPTURE_MAX_DIMENSION` | `4096` | `int64_t` | 截图单边像素上限 |
| `GDA_CAPTURE_MAX_PNG_BYTES` | `8 MiB` | `size_t` | 截图 PNG 大小上限 |
| `GDA_VARIANT_MAX_STRING_BYTES` | `64 KiB` | `size_t` | Variant 字符串单项上限 |
| `GDA_VARIANT_MAX_ARRAY_ELEMENTS` | `10000` | `size_t` | Variant 数组/PackedArray 元素上限 |
| `GDA_MAX_JSON_RESPONSE_BYTES` | `4 MiB` | `size_t` | JSON 响应大小上限 |
| `GDA_SCAN_MAX_FILES` | `10000` | `size_t` | 扫描文件数上限 |
| `GDA_SCAN_MAX_FILE_BYTES` | `2 MiB` | `size_t` | 扫描单文件大小上限 |
| `GDA_SCAN_MAX_TOTAL_BYTES` | `32 MiB` | `size_t` | 扫描累计字节上限 |
| `GDA_SCAN_MAX_DEPTH` | `64` | `size_t` | 扫描目录深度上限 |

## 与现有文档的不一致点

| 文档 | 声称 | 代码事实 | 判定 |
|---|---|---|---|
| AGENTS.md（构建段） | 暗示每个模块有成对的 `.cpp/.hpp` 参与 `add_library()` | `command_queue.hpp` 无对应 `.cpp`，header-only，不进 CMake 源列表 | 文档未明说，审计时需注意 |
| README.md / README.en.md（前提） | "Godot 4.7+"（`README.md:5,53` / `README.en.md:5,53`） | Example 项目为 4.7（`Example/project.godot`），AGENTS.md 亦写 4.7 | 一致 ✓ |
| README.md / README.en.md（安装） | "服务端自动启动……无需启用开关"（`README.md:65` / `README.en.md:65`） | cmdline/`GDA_FORCE_HEADLESS` 模式下 UI 与服务器被禁用（`main.cpp`） | 存在例外，描述不完整 |
| AGENTS.md（架构/端口段） | 端口 9527、`/mcp`、`GODOT_AUTOPILOT_PORT` 覆盖、日志类别五枚举 | 全部与代码一致 | 一致 ✓ |
| README.md / README.en.md（安全段） | 回环监听 + 端口 9527 + `/mcp`（`README.md:65,74` / `README.en.md:65,74`） | 与 `server_context.cpp` 一致 | 一致 ✓ |
| AGENTS.md | 日志类别 "仅此几个：System、Transport、Tools、Resources、Prompts" | `LogCategory` 枚举完全相同 | 一致 ✓ |

## 相关页面

- 入口：由 [../index.md](../index.md) 链接
- 模块总览：[../overview.md](../overview.md)
- 测试覆盖：[../tests.md](../tests.md)
