# 项目总览（Overview）

> 审计日期：2026-08-16（2026-08-12 初稿；08-16 随 mcp-cpp-sdk 0.3.1 升级同步），基于当前工作树文件与代码逐项核对（不依赖 git 历史）。
> 事实来源：根 `README.md` / `README_zh.md` / `AGENTS.md`、`CMakeLists.txt`、`cmake/FetchDependencies.cmake`、`src/main.cpp`、`src/core/server_context.cpp`、`src/tools/tool_defs.def`、`src/tools/dispatch.cpp`、`src/prompts/prompt_handlers.cpp`、`src/resources/resource_handlers.cpp`、`Example/project.godot`、`Example/docs/`。

## 项目定位

Godot-Autopilot 是一个 **MCP（Model Context Protocol）服务器**，以 **GDExtension 插件**形式**进程内加载到 Godot 编辑器**，在原生引擎 API 层面把 AI Agent 与 Godot 连接（场景树、物理服务器、渲染服务器、音频、导航、输入模拟、脚本执行等），而非模拟用户 UI 操作。

- **进程内 GDExtension**：插件随编辑器启动而启动、随编辑器关闭而停止，无独立桥接进程（`src/main.cpp` 的 `GDExtensionEntryPoint` 注册 `GodotAutopilotPlugin`，`_enter_tree()` 中创建并启动 `ServerContext`）。
- **产物名**：`godot-autopilot`（`.dll` / `.so` / `.dylib` + `.gdextension`），部署目录 `Example/addons/godot-autopilot/`。
- **附属工程**：`Example/` 是文档/示例工程（详见 [example.md](./example.md)），`build.py` 构建后部署插件到其中。

## 核心能力

### MCP 工具：339 个 = 7 元工具 + 332 领域工具

数值以 `src/tools/tool_defs.def`（332 条 `TOOL_ENTRY`）与 `src/tools/dispatch.cpp`（`meta_tool_names` 7 个）为准；数量随插件版本变化，运行时可经 `search_tools` 确认。工具名遵循 `<动词>_<类别>_<维度>_<对象>_<修饰>`（动词置首，snake_case）。

**7 个元工具**（直接 `server.RegisterTool()` 注册，见 `register_all.cpp`）：

| 元工具 | 职责 |
|---|---|
| `ping` | 健康检查 |
| `search_tools` | BM25 关键词检索工具 |
| `list_categories` | 列出全部类别 |
| `get_tool_detail` | 获取单个工具完整 schema |
| `call_tool` | 按名称代理调用领域工具（检查 `error` 字段并置 `is_error`） |
| `batch_execute` | 顺序批量执行多工具 |
| `code_execute` | 执行任意 GDScript（临时 Node + `_run()`，可选 `function_name` 多函数模式） |

**332 个领域工具**（经 `call_tool` 代理，`g_handlers` 映射分发），按 23 个类别组织（InputMap 并入 Input），各类别数量以 `tool_defs.def` 统计为准：

| 类别 | 数量 | 类别 | 数量 |
|---|---:|---|---:|
| Render | 49 | Scene | 12 |
| Physics | 47 | Scripts | 10 |
| Display | 24 | Text | 10 |
| Editor | 22 | TileMap | 7 |
| Resources | 21 | Debugger | 7 |
| Audio | 20 | Game | 7 |
| Input | 19 | Properties | 5 |
| OS | 16 | Docs | 4 |
| Debug | 16 | Group | 3 |
| Navigation | 15 | SpriteFrames | 3 |
| Config | 13 | System | 1 |
| | | Capture | 1 |

另有 `system_status`（端口/版本/运行时长）注册于 `g_handlers`，经 `call_tool` 调用，不直接注册为 MCP 工具；`ToolCatalog` 合计 343 条目（332 领域 + 7 元工具 + `system_status` + 3 个 meta 快照）。

### 提示词模板：7 个

`src/prompts/prompt_handlers.cpp` 注册：`create-3d-scene`、`setup-character`、`debug-physics`、`setup-input-map`、`setup-gui`、`tool-usage`、`keycode-reference`。另有调试器专用 prompts（`src/prompts/debugger_prompts.cpp`）。

### MCP 资源：15 个

`src/resources/resource_handlers.cpp` 注册 8 个：`godot://engine/version`、`godot://scene/tree`、`godot://scene/{path}`、`godot://filesystem/tree`、`godot://filesystem/{path}`、`godot://editor/selection`、`godot://editor/settings/{key}`、`godot://log/recent`。

`src/resources/debugger_resources.cpp` 另注册 7 个调试器资源：`godot://editor/output-log`、`godot://debugger/errors`、`godot://debugger/output`、`godot://debugger/stack-dump`、`godot://debugger/scene-tree`、`godot://debugger/monitors`、`godot://debugger/session`。

### 运行时桥接（game_* 工具）

编辑器进程的 MCP 服务器与游戏运行时进程经 EngineDebugger 消息通道（`gda:request` / `gda:response` / `gda:ready`）双向通信，`src/runtime/`（`game_bridge.cpp` / `game_bridge_input.cpp` / `game_bridge_eval.cpp` + `gda_protocol.hpp`）实现输入模拟、GDScript 求值、截图捕获、日志/错误回传、场景树查询等 op。工具侧封装见 `game_*` / `get_game_log_entries` 等。详见 [modules/entry_runtime.md](./modules/entry_runtime.md)。

### 编辑器 UI

- 自定义底部日志面板 `McpLogDock`（"MCP Log"，按 LogLevel/LogCategory 过滤、文本搜索、折叠重复）。
- 右侧配置面板 `McpConfigDock`（"MCP Config"：端口运行时重启 + 持久化、一键生成 8 个客户端 MCP 配置）。
- `ExportGuard`：导出期间拒绝领域工具调用（返回 `{"error":"editor is exporting; ..."}`）。

## 技术栈

| 层次 | 技术 | 来源 |
|---|---|---|
| 引擎 | Godot 4.x GDExtension（目标 4.7，见 `Example/project.godot` features） | — |
| 绑定层 | godot-cpp，GIT_TAG `10.0.0-rc1` | `cmake/FetchDependencies.cmake` |
| MCP 协议 | mcp-cpp-sdk（modelcontextprotocol-cpp-sdk），GIT_TAG `0.3.1`，仓库 `jesspig/modelcontextprotocol-cpp-sdk` | 同上 |
| HTTP / 异步 | mcp-cpp-sdk 自研网络栈（内部 HTTP 线程） | mcp-cpp-sdk 内置（0.3.x 起移除 libhv） |
| JSON | mcp::JsonValue（SDK 内置，自研解析器） | mcp-cpp-sdk 内置（0.3.x 起移除 simdjson） |
| 构建 | CMake 3.28+（`cmake_minimum_required(3.28...4.2)`）、C++17、Ninja 预设（`debug`/`release`） | 根 `CMakeLists.txt`、`CMakePresets.json` |
| 编译 | 优先 Clang/clang-cl 自动探测，MSVC/GCC 回退；sccache/ccache、LTO（ThinLTO/LTCG/IPO）、Unity 构建、作业池 | `cmake/BuildOptimization.cmake` 等 |
| 测试 | googletest（FetchContent 拉取，L1）；自研 `gda_test_runner` + `tests/config/*.json`（L2） | `tests/` |

构建优化可通过环境变量覆盖：`GDA_COMPILE_JOBS`、`GDA_LINK_JOBS`、`GDA_CCACHE`、`GDA_SCCACHE`、`GDA_LTO`、`GDA_UNITY_ENABLED`、`GDA_UNITY_BATCH`、`GDA_MAX_COMPILE_MEM_MB`、`GDA_MAX_LINK_MEM_MB`、`GDA_ARCH`、`GDA_IS_CI` 等。详见 [build.md](./build.md)。

## 目录结构

```text
godot-self-driving/
├── CMakeLists.txt              # add_library(godot-autopilot SHARED ...)，新 .cpp 必须登记
├── CMakePresets.json           # debug / release（Ninja）
├── build.py                    # 构建 + 部署到 Example/addons/godot-autopilot/
├── cmake/                      # 构建模块：FetchDependencies / BuildOptimization / Lto / Cache / CompilerOptions / Platform
├── src/
│   ├── main.cpp                # GDExtension 入口 + GodotAutopilotPlugin(EditorPlugin) 生命周期
│   ├── core/                   # 基础设施：CommandQueue(header-only)、config、LogSystem、ModeDetector、
│   │                           #   ResourceRegistry、SceneDirtyTracker、ExportGuard、ServerContext
│   ├── tools/                  # 领域工具：36 个 *_ops.cpp、register_all、dispatch、tool_catalog、
│   │                           #   tool_defs.def（332 条工具定义）、schema_* 生成器
│   ├── resources/              # MCP Resources：resource_handlers + debugger_resources
│   ├── prompts/                # 提示词模板：7 个主题 + debugger_prompts
│   ├── runtime/                # 游戏运行时桥接：game_bridge(±input/eval) + gda_protocol.hpp
│   ├── ui/                     # 编辑器 UI：mcp_config_dock、mcp_log_dock
│   └── util/                   # 通用：variant_json、bm25_index、error_util、readback_util、scene_path
├── tests/                      # L1 gda_unit_tests（61 个 gtest）+ L2 gda_test_runner + config/*.json（5 份）
├── docs/                       # 规划文档（docs/plan/）与本知识库（docs/wiki/）
└── Example/                    # 文档/示例工程（详见 example.md）
```

各子目录职责详见 [modules/](./modules/) 页面。

## 整体架构

```mermaid
flowchart LR
    Host[MCP 主机: Claude Desktop / Cursor / opencode 等] -->|POST http://127.0.0.1:9527/mcp| HTTP[SDK HTTP 线程<br/>StreamableHttpServerTransport]
    HTTP -->|queue.submit() 返回 future| Q[CommandQueue 互斥队列]
    Q -->|_process 每帧 drain()| Main[Godot 主线程]
    Main --> API[Godot API / 场景 / 引擎]
    Main --> Dock[McpLogDock 日志面板]
    HTTP -->|engine-debugger 通道| Runtime[游戏运行时进程<br/>game_bridge 桥接]
    HTTP -.future.get() 同步等待.-> HTTP
```

- **线程模型**：所有 Godot API 调用必须经 `queue.submit()` 由主线程执行——HTTP 线程直接调用会崩溃。`dispatch::call_handler` 按 `is_main_thread()` 决定直接执行或入队等待（`dispatch.cpp`）；排空点唯一：`GodotAutopilotPlugin::_process()` 调 `s_queue.drain()`（`main.cpp`）。
- **生命周期**：`GDExtensionEntryPoint` → SCENE 级别（非编辑器进程注册桥接监听）/ EDITOR 级别（注册 4 个类 + `add_by_type`）→ 插件 `_enter_tree()` 建 UI、启 `ServerContext` → `_exit_tree()` 逆序清理。`gda_cmdline_mode()`（`GDA_FORCE_HEADLESS=1` 时禁用）下跳过 UI 与服务器。
- **日志**：`LogCategory { System, Transport, Tools, Resources, Prompts }` 五类、`LogLevel { Debug, Info, Warning, Error }` 四级，内存环形缓冲上限 10000 条。

## 命名约定

| 项 | 约定 |
|---|---|
| 命名空间 | `godot_autopilot`（部分子模块用子命名空间，如 `dispatch`、`resource_registry`、`scene_dirty_tracker`、`runtime::game_bridge`） |
| 工具命名 | `<动词>_<类别>_<维度>_<对象>_<修饰>`（动词置首，snake_case，如 `intersect_physics_2d_ray`、`create_scene_node`、`set_input_map_action_deadzone`） |
| 代码前缀 | 常量 `GDA_`（如 `GDA_DEFAULT_PORT`）；运行时环境变量 `GODOT_AUTOPILOT_PORT`、`GDA_FORCE_HEADLESS` |
| 产物 | `godot-autopilot`（库名/插件目录/`.gdextension` 名） |
| 端口 | 9527（`GDA_DEFAULT_PORT`，`config.hpp`），`GODOT_AUTOPILOT_PORT` 环境变量覆盖，端点 `/mcp`（`server_context.cpp`） |

## 关键数字速查

| 项 | 值 |
|---|---|
| MCP 端口 / 端点 | 9527 / `/mcp`（环境变量 `GODOT_AUTOPILOT_PORT` 覆盖） |
| 工具总数 | 339 = 7 元 + 332 领域（领域 23 类别）；ToolCatalog 343 条目 |
| 提示词 / 资源 | 7 模板 / 15 资源 |
| 日志类别 | 5（System / Transport / Tools / Resources / Prompts） |
| 引擎目标 | Godot 4.7（`Example/project.godot` features） |

## 文档一致性核查（速览）

- 端口 9527、`/mcp`、`GODOT_AUTOPILOT_PORT`：README（英/中）、AGENTS.md、代码三方一致 ✓
- 工具总数 339：README.md（英文）"~339" 与 `tool_defs.def` + meta 数量一致 ✓；README_zh.md 已同步更新（原"~200 个工具、13 个类别"过时，已修正）
- README.md 类别数量表与 def 一致（23 类，以 `tool_defs.def` 实测为准）✓
- 无 CI：`.github/` 不存在，AGENTS.md 声称属实 ✓
- 目标引擎 4.7：与 `Example/project.godot` 一致 ✓；README 前提"Godot 4.3+"为宽松下界
- 详细对照见 [example.md](./example.md) 与 [modules/core.md](./modules/core.md) 的"不一致点"章节

## 相关页面

- 示例工程：[example.md](./example.md)
- 构建与部署：[build.md](./build.md)
- 测试体系：[tests.md](./tests.md)
- 核心模块：[modules/core.md](./modules/core.md)
- 入口与运行时桥接：[modules/entry_runtime.md](./modules/entry_runtime.md)
