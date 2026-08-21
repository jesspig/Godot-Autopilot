---
type: 模块文档
title: 核心模块
description: 命令队列、配置常量、日志、模式检测、资源缓存、脏状态跟踪、服务器生命周期与插件配置
tags:
  - 模块
  - 核心层
  - 线程模型
timestamp: "2026-08-22T01:38:08+08:00"
resource: src/core/
---

# 核心模块（src/core/）

> 审计日期：2026-08-22（2026-08-12 初稿；08-16 随 mcp-cpp-sdk 0.3.1 升级同步；08-17 随配置面板端口持久化同步并补 YAML frontmatter；08-22 随版本号收敛为根 `VERSION` 单一来源同步 MCP 标识引用），基于当前工作树代码逐行核对（不依赖 git 历史）。
> 覆盖范围：`src/core/` 下 9 组文件。注意：`CommandQueue` 为 header-only（仅 `command_queue.hpp`，无对应 `.cpp`），实际为 16 个文件。

## 模块简介

`src/core/` 是插件的基础设施层：负责把 HTTP 线程（mcp-cpp-sdk 自研网络栈）的请求安全地桥接到 Godot 主线程、提供进程内日志、配置常量、运行时模式检测、资源缓存、场景脏状态跟踪，以及 MCP 服务器的生命周期管理。所有模块位于命名空间 `godot_autopilot`（`ResourceRegistry`/`SceneDirtyTracker` 使用 `godot_autopilot::resource_registry` / `godot_autopilot::scene_dirty_tracker` 子命名空间）。

## 职责表

| 模块 | 文件 | 职责 | 消费方 |
|---|---|---|---|
| `CommandQueue` | `command_queue.hpp`（header-only） | 跨线程任务队列：`submit()` 入队并返回 `std::future`，主线程 `drain()` 批量执行 | 所有工具/resource/prompt handler、`main.cpp` |
| 配置常量 | `config.hpp` | 端口、超时、缓冲区上限等编译期常量（GDA_ 前缀） | `server_context.cpp`、`runtime/game_bridge.cpp` 等 |
| `ExportGuard` | `export_guard.cpp/hpp` | 导出期间置位全局原子标志，供工具分发判定"导出中" | `dispatch::export_blocked_result`、`_enter_tree` 注册 |
| `LogSystem` | `log_system.cpp/hpp` | 进程内环形日志（内存 + 回调，无文件输出），单例 | 全部模块、`McpLogDock`、log 类资源 |
| `ModeDetector` | `mode_detector.cpp/hpp` | 运行时模式检测（编辑器/游戏/未知） | `_enter_tree` 启动日志 |
| `ResourceRegistry` | `resource_registry.cpp/hpp` | 内存资源缓存（oid 键 + `name:` 前缀键），全局 mutex 保护 | 资源类工具 |
| `SceneDirtyTracker` | `scene_dirty_tracker.cpp/hpp` | 记录"当前编辑场景是否被修改"及根节点实例 ID | `GodotAutopilotPlugin::_get_unsaved_status` |
| `ServerContext` | `server_context.cpp/hpp` | MCP 服务器组装、端口解析、启动/停止/重启、工具/资源/prompt 注册 | `main.cpp` 入口 |
| `PluginConfig` | `plugin_config.cpp/hpp` | 插件自身配置持久化（`user://godot_autopilot/config.json`，当前仅端口） | `ServerContext` 端口解析、`McpConfigDock` Apply |

## 关键接口清单

### CommandQueue（header-only）

- `template <typename Fn> auto submit(Fn&&) -> std::future<std::invoke_result_t<Fn>>` — 入队任务，返回 future；任务异常通过 `promise.set_exception` 传播
- `void drain()` — 主线程调用；锁定交换批量任务后逐个执行；**首次调用时记录当前线程为"主线程"**
- `bool is_main_thread() const` — 与记录的线程 ID 比较
- 内部结构：`std::queue<std::unique_ptr<TaskBase>>` + `std::mutex` + `std::atomic<std::thread::id>`
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
- `query_from(size_t start_index, size_t* next_index)` + `size_t next_index()` — 增量查询（用于 `godot://log/recent` 类资源）
- `static LogSystem& instance()` — 局部静态单例
- `set_on_new_entry(OnNewEntryCallback)` — 新条目回调（锁外调用），`McpLogDock` 消费
- **写入目标：仅内存 + 回调；无文件、无 Godot 控制台直接输出**

### ModeDetector

- `static RuntimeMode detect()` — `Engine::get_singleton()` 为 null → `Unknown`；否则 `is_editor_hint()` 为真 → `Editor`，反之为 `Game`
- `static bool is_editor()` — `detect() == Editor`

### ResourceRegistry（命名空间函数，非类）

- `register_resource(res, name)` — 以 `instance_id` 为键，另加 `"name:" + name` 键
- `lookup_memory(name)` — 先查 `name:` 键；若参数为纯数字再回退查 oid 键；未命中返回空 `Ref`
- `erase_oid(object_id)`、`is_registered(name)`
- 线程安全：`std::unordered_map<std::string, godot::Ref<godot::Resource>>` + 全局 `std::mutex`

### SceneDirtyTracker（命名空间函数，非类）

- `mark_scene_modified()` — 读取 `EditorInterface::get_edited_scene_root()`，无根节点则不置脏；记录根实例 ID
- `clear_scene_modified()`、`bool is_current_scene_dirty()` — 脏判定要求"有根节点且实例 ID 与记录一致"
- 消费方：`_get_unsaved_status`（决定编辑器"未保存"标记）

### ServerContext

- 构造：持有 `CommandQueue&`，创建 `ToolCatalog` 与 `Bm25Index`，`resolve_port()` 解析端口并写 Transport 日志
- `bool start()` — 依次：`StreamableHttpServerTransport`（port、`endpoint = "/mcp"`、`stateless = true`、`enable_legacy_sse = false`）→ `mcp::McpServer::Create` → `register_tools()` → `transport_->Start()`；成功后回写 `port_ = http_opts.port`；SDK 自身日志默认关闭（`MCP_LOG_LEVEL` 未设置时为 Off）
- `void stop()` — `server_->Close()` + `transport_->Close()`；析构函数对 running 状态兜底调用
- `bool restart(uint16_t port)` — `stop()` → 更新 `port_` → `start()`；供配置面板运行时改端口（配置面板 Apply 后立即生效，无需重启编辑器）
- `int get_port()` / `bool is_running()` / `const std::string& last_error()`
- MCP 服务器标识：`mcp::Implementation{"godot-autopilot", GDA_VERSION}`（宏经 `configure_file` 由根 `VERSION` 文件生成，见 `build.md` "版本号单一来源"）
- 生命周期回调（全部写 Transport 类别日志）：`on_method_called`（Debug）、`on_client_connected` / `on_initialized` / `on_transport_close`（Info）、`on_protocol_error` / `on_transport_error`（Error）
- `register_tools()` 注册四类：工具、资源、prompt、调试器专用资源/prompt

### PluginConfig（命名空间静态方法，非类实例）

- `int load_port()` — 读 `user://godot_autopilot/config.json` 的 `port` 键；文件不存在/解析失败/非整数时返回 `-1`（表示未配置）
- `bool save_port(int port)` — 写回 `{"port": N}`（先 `DirAccess::make_dir_recursive_absolute` 建目录）；失败记 System 类别错误日志并返回 false
- 消费方：`ServerContext::resolve_port()`（启动时读取）、`McpConfigDock::_on_apply_port()`（Apply 成功后写入）

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
- 排空点唯一：`GodotAutopilotPlugin::_process(double)` 调用 `s_queue.drain()`（`main.cpp`），随后轮询 `McpLogDock`
- `drain()` 首次执行时把当前线程记为"主线程"，此后 `is_main_thread()` 据此判定

## 生命周期（插件 ↔ ServerContext）

1. `GDExtensionEntryPoint`：`MODULE_INITIALIZATION_LEVEL_SCENE` 注册类，`MODULE_INITIALIZATION_LEVEL_EDITOR` 时 `EditorPlugins::add_by_type<GodotAutopilotPlugin>()`
2. `_enter_tree`：设置 editor queue → 状态栏/Log Dock/输出捕获/调试器插件 → `new ServerContext(queue)` 并 `start()` → `add_export_plugin(ExportGuard)`；`GDA_FORCE_HEADLESS` 或 `gda_cmdline_mode()` 时跳过 UI/服务器
3. `_process`：每帧 `drain()` + Dock 轮询
4. `_exit_tree`：`ServerContext::stop()` + delete → 注销各组件

## 环境变量与端口

- `GODOT_AUTOPILOT_PORT`：`resolve_port()` 用 `std::getenv` 读取、`std::atoi` 转换（**无格式校验**）
- 端口解析优先级：**环境变量 > `PluginConfig::load_port()`（user:// 持久化值，需 > 0）> `GDA_DEFAULT_PORT`（9527）**——环境变量优先保证测试/CI 场景不受面板配置影响
- 运行时改端口：`ServerContext::restart(uint16_t)`（配置面板 Apply 触发，成功后经 `PluginConfig::save_port` 持久化）
- `GDA_FORCE_HEADLESS`：强制 headless 相关路径（`main.cpp` 读取）

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

## 与现有文档的不一致点

| 文档 | 声称 | 代码事实 | 判定 |
|---|---|---|---|
| AGENTS.md（构建段） | 暗示每个模块有成对的 `.cpp/.hpp` 参与 `add_library()` | `command_queue.hpp` 无对应 `.cpp`，header-only，不进 CMake 源列表 | 文档未明说，审计时需注意 |
| README.md（前提） | "Godot 4.3+" | Example 项目为 4.7（`Example/project.godot`），AGENTS.md 亦写 4.7 | 文档间冲突 |
| README.md（安装） | "Open your Godot project — the server starts automatically" | cmdline/`GDA_FORCE_HEADLESS` 模式下 UI 与服务器被禁用（`main.cpp`） | 存在例外，描述不完整 |
| Example/docs/architecture.md | "HTTP 线程提交必须经 `CommandQueue::submit()`，由主线程 `_process()` 排空" | 与代码完全一致（`main.cpp:217`） | 一致 ✓ |
| AGENTS.md（架构/端口段） | 端口 9527、`/mcp`、`GODOT_AUTOPILOT_PORT` 覆盖、日志类别五枚举 | 全部与代码一致 | 一致 ✓ |
| README.md（设计表） | Port 9527、线程模型"Command queue + frame sync" | 一致 ✓ | 一致 ✓ |
| AGENTS.md | 日志类别 "仅此几个：System、Transport、Tools、Resources、Prompts" | `LogCategory` 枚举完全相同 | 一致 ✓ |

## 相关页面

- 入口：由 [../index.md](../index.md) 链接
- 模块总览：[../overview.md](../overview.md)
- 测试覆盖：[../tests.md](../tests.md)
