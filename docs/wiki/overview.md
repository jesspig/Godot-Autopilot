---
type: 项目总览
title: 项目总览
description: 项目定位、整体架构、技术栈与目录结构的权威总览
tags:
  - 总览
  - 架构
  - 技术栈
timestamp: "2026-11-17T16:54:14+08:00"
resource:
  - README.md
  - README.zh-CN.md
  - INSTALL.md
  - src/
---

# 项目总览（Overview）

> 审计日期：2026-11-17（Game Jam v2 与工具面 252 收口；数值口径见 [tests.md](./tests.md)）

## 项目定位

Godot-Autopilot 是一个 **MCP（Model Context Protocol）服务器**，以 **GDExtension 插件**形式**进程内加载到 Godot 编辑器**，在原生引擎 API 层面把 AI Agent 与 Godot 连接（场景树、物理服务器、渲染服务器、音频、导航、输入模拟、脚本执行等），而非模拟用户 UI 操作。

- **进程内 GDExtension**：插件随编辑器启动而启动、随编辑器关闭而停止，无独立桥接进程（`src/main.cpp` 的 `GDExtensionEntryPoint` 注册 `GodotAutopilotPlugin`，`_enter_tree()` 中创建并启动 `ServerContext`）。
- **产物名**：`godot-autopilot`（`.dll` / `.so` / `.dylib` + `.gdextension`），部署目录为目标 Godot 工程的 `addons/godot-autopilot/`。
- **示例工程（可选）**：仓库不内置示例；`bun run gda-dev-tools deploy --project <dir>` 可部署到任意 Godot 工程，未传 `--project` 时递归发现 `examples/` 下的比赛最终项目并跳过 intake 与 e2e 沙盒。`jam` 抽题生成 `examples/<theme_id>/project/` intake，`jam create` 创建 `<slug>-<hash8>` 比赛项目，`jam e2e` 管理固定沙盒 `examples/e2e`（每轮重置）。程序化测试不依赖抽题，只有生成器单测在临时目录覆盖。详见 [gamejam.md](gamejam.md)。

## 核心能力

### 调用通道（2026-11-13 T-03）

插件在 TCP 端口之外额外监听一条本机 IPC 通道（Windows loopback TCP、POSIX AF_UNIX；4 字节大端长度前缀 + JSON，单帧 4 MiB；首帧 `initialize` 校验实例令牌），与 TCP `/mcp` 并存、绑定失败仅告警并降级；实例登记以 `endpoints.ipc = {kind, path|host+port, token, protocol, framing, auth, version}` 表达端点（缺字段按无 IPC 处理）。CLI `gda call --socket <path>` 或 `endpoints.ipc` 可直连该通道，失败默认回退端口通道。实现见 `src/core/transport/ipc_transport.cpp`、`ipc_framing.cpp`、`ipc_endpoint.cpp`。

### MCP 工具面：catalog 252 = 5 元工具 + 246 领域工具 + `system_status`

数值以 `src/tools/base/tool_spec.hpp`（`ToolSpec` 数据层）、`src/tools/base/tool_registry.hpp`（`ToolRegistry` 单一来源）与 `src/tools/domains/*/*_tools.hpp`（246 个域工具的 `make_tools()`，每工具一份 `ToolSpec`）为准；catalog/index 252 条目由 `register_all.cpp` 从 registry 派生；`get_active_registry()` 暴露活跃 registry，数量随插件版本变化，运行时可经 `search_tools` 确认。工具名遵循 `<动词>_<类别>_<维度>_<对象>_<修饰>`（动词置首，snake_case）；合并后的 action 工具用 `action` 参数区分动作，单工具最多 8 个 action。

**5 个元工具**（`SpecTool` + `tool_flags::kMeta`，顶层 MCP 工具，见 `register_all.cpp`）：

| 元工具 | 职责 |
|---|---|
| `search_tools` | BM25 关键词检索真实工具；别名不入索引。`list_categories` 已并入其别名 |
| `get_tool_detail` | 获取单个 canonical 工具的完整 schema；传别名返回 canonical 详情并附弃用字段 |
| `call_tool` | 按名称代理调用领域工具或 `system_status`：先经 `dispatch` 解析别名并适配参数/投影结果，检查 `error` 字段并置 `is_error`；别名响应顶层附 `deprecated_alias`/`canonical_name`/`alias_removal_version`/`deprecated_since`，`confirm=true` 不会被别名丢失 |
| `batch_execute` | 顺序批量执行多工具（内部经 `dispatch::call_handler`，逐条返回） |
| `code_execute` | 执行任意 GDScript（临时 Node + `_run()`，可选 `function_name` 多函数模式） |

**246 个领域工具**（经 `call_tool` 代理，`g_handlers` 映射分发；35 个域文件，28 个 category），按 category 分布如下（两列合计 252，含 5 元与 `system_status`；`Image`/`Mesh` 两个新域的工具归 Resources 类，`C#` 域归 Scripts 类，`GridMap`/`Tileset` 归 TileMap 类，`SceneTree` 归 Scene 类，故 category 数少于域文件数）：

| 类别 | 数量 | 类别 | 数量 |
|---|---:|---|---:|
| Physics | 45 | Render | 29 |
| Resources | 18 | Editor | 16 |
| Navigation | 14 | Scripts | 13 |
| Game | 12 | OS | 11 |
| Animation | 10 | Input | 10 |
| Scene | 8 | Display | 8 |
| TileMap | 8 | Config | 8 |
| Debug | 7 | Properties | 6 |
| Audio | 5 | System | 5 |
| Debugger | 4 | Text | 3 |
| Theme | 3 | Capture | 2 |
| Meta | 2 | Analysis | 1 |
| Docs | 1 | Group | 1 |
| SpriteFrames | 1 | Testing | 1 |

（`System` 5 = 3 个元工具（`call_tool`/`batch_execute`/`code_execute`）+ `system_status` + `get_game_log_entries`（Log 域，归 System 类），`Meta` 2 = `search_tools`/`get_tool_detail`；域文件口径的 35 个域计数见 [tool_surface_report.md](./tool_surface_report.md)「按域计数」。）

**生产别名 325** 不占 catalog、不进 `search_tools`，只经 `call_tool` / `batch_execute` 解析到 canonical，统一 `deprecated_since="0.4.0"`、`removal_version="0.6.0"`；schema 统计 235 非空 + 17 空（空 = 无参数且无 action schema）；能力矩阵 561 域行 / 567 catalog 行，mapped 315、kept 252、missing 0。`system_status`（端口/版本/运行时长，`SpecTool`）经 `call_tool` 调用，不直接注册为 MCP 工具。`ping` 是 `system_status` 的别名。只读工具回显 `"none"`（`side_effect_name(SideEffect::None)`）；`code_execute`/`batch_execute` 声明为 `code_execute`（`SideEffect::CodeExecute`）。

### 提示词模板：12 个（7 通用 + 5 调试）

`src/prompts/prompt_handlers.cpp` 注册 7 个通用 prompt：`create-3d-scene`、`setup-character`、`debug-physics`、`setup-input-map`、`setup-gui`、`tool-usage`、`keycode-reference`。`src/prompts/debugger_prompts.cpp` 另注册 5 个调试专用 prompt：`debug-analyze-error`、`debug-analyze-breakpoint`、`debug-review-output`、`debug-review-performance`、`debug-session-status`。

### MCP 资源：15 个引擎/调试侧 + skills 资源组

`src/resources/resource_handlers.cpp` 注册 8 个：`godot://engine/version`、`godot://scene/tree`、`godot://scene/{path}`、`godot://filesystem/tree`、`godot://filesystem/{path}`、`godot://editor/selection`、`godot://editor/settings/{key}`、`godot://log/recent`。

`src/resources/debugger_resources.cpp` 另注册 7 个调试器资源：`godot://editor/output-log`、`godot://debugger/errors`、`godot://debugger/output`、`godot://debugger/stack-dump`、`godot://debugger/scene-tree`、`godot://debugger/monitors`、`godot://debugger/session`。

09-16 批起新增 skills 资源组（`register_skill_resources`，`src/resources/skill_resources.hpp` 提供 URI 解析）：`godot://skills`（目录，JSON 目录清单）、`godot://skills/<name>`（入口卡 SKILL.md）、`godot://skills/<name>/<file>`（参考文件）——本轮起为 1 张入口卡 + 9 篇 references，共 12 个资源（1 目录 + 1 技能 + 10 文件），技能经 MCP 协议层可发现可读。

### 运行时桥接（game_* 工具）

编辑器进程的 MCP 服务器与游戏运行时进程经 EngineDebugger 消息通道（`gda:request` / `gda:response` / `gda:ready`）双向通信，`src/runtime/`（`game_bridge.cpp` / `game_bridge_input.cpp` / `game_bridge_eval.cpp` / `game_bridge_verify.cpp` + `gda_protocol.hpp`）实现输入模拟、GDScript 求值、截图捕获、日志/错误回传、场景树查询与 SceneTree 操作（09-29 新增 `scene_tree` op）等 op；游戏工具默认 `wait_ready=true`——游戏已启动但未就绪时请求排队、收到 `gda:ready` 后自动重放，无游戏进程时立即返回 `game not running: ...`。工具侧封装见 `game_*` / `get_game_log_entries` 等。详见 [modules/entry_runtime.md](./modules/entry_runtime.md)。

### 编辑器 UI

- 自定义底部日志面板 `McpLogDock`（"GDA Log"）：按 LogLevel/LogCategory 过滤、文本搜索（09-21 起同时匹配 message 与 detail）、折叠重复、Detail 显示诊断 detail、Open Logs 打开工程级日志目录（`<集中根>/projects/<项目名>-<路径哈希前 8 位>/logs`）、Open Traces 打开同工程 `traces` 目录与 Trace 视图（日志行带可点击 `[trace]` 标记，按 trace_id 渲染结构化事件列表，默认关闭）。"Show timestamps" 开关控制每条日志的 `[HH:MM:SS]` 本地时间前缀，经 `<集中根>/config.json`（默认 `~/.godot-autopilot/config.json`）的 `show_time` 键持久化；新路径缺该键时回退旧 `user://godot_autopilot/config.json`。折叠重复日志时，除条数外始终显示最新一条的时间，不受总开关控制。
- 右侧配置面板 `McpConfigDock`（"MCP Config"）已于 0.3.2 移除：端口由插件持有并持久化，MCP 配置从 `gda status --json` 的 `mcp_url` 生成，授权门改为高风险工具的 `confirm`，技能通过 `gda install skills` 安装。当前仅保留 `McpLogDock`。
- `ExportGuard`：导出期间拒绝领域工具调用（返回 `{"error":"editor is exporting; retry after export completes"}`）；导出期拒绝现覆盖**工具调用 / MCP 资源 / MCP 提示词**三类入口。导出标志为**实例级**——每个 `ExportGuard` 实例各持一份 `ExportState`，`is_exporting()` 遍历实例注册表，任一实例处于导出期即返回 true。

### Computer Use grounding（09-15）

- **编辑器 UI 语义自动化**：`get_editor_ui` 的 `action=elements` 枚举编辑器控件（语义 path / 类型 / 文本 / tooltip / 客户区矩形 / window_id，支持 `query`/`type_filter`/`interactive_only`/`max_elements`），`action=hit` 返回某客户区点下的控件链（最上层优先）；动作侧 `editor_ui_action` 的 `action=click`（按 path 点击，支持 `button`/`double_click`/`warp`/`observe`）、`action=type`（聚焦并整体注入文本，可选 `submit`/`observe`）、`action=shortcut`（解析 `ctrl+s` 形式快捷键并注入），原始坐标回退为 `input_mouse` 的 `click`/`scroll`/`drag` 与 `input_text` 的 `type`。
- **坐标映射**：`get_editor_ui` 的 `action=geometry` 给出视口图像→窗口客户区的 `window = offset + image * scale` 映射（2D 另含 canvas origin/zoom），`get_scene_tree` 的 `paths` 把编辑场景节点映射到客户区坐标（2D/3D 自动或强制，Control 附 rect，顶层 `mapping` 用于截图像素换算），`get_display_info` 的 `action=window` 返回窗口客户区矩形与所在屏幕几何/缩放（桌面坐标与客户区坐标互转）。
- **截图增强**：`capture_editor_viewport` 支持 `space`（`viewport`/`window`）、`region`（裁剪）、`max_dimension`（等比缩小、不放大）、`annotate`（编号红框 + elements）与 `diff_against_last`（与上次编辑器截图比较 changed_ratio/changed_bbox；尺寸不同时按公共左上区域比较并返回 `size_mismatch`/`current_size`；不可比较返回 `comparable=false` 与 `reason` ∈ {`no_baseline`, `unsupported_format`, `baseline_too_large`}）。
- **游戏侧**：`game_input` 的 `queue`/`sequence` 支持 wheel（`direction`/`amount` 或 button_index 4-7）与单步 `mouse_motion`；`capture_editor_viewport` 的 `target=game` 在游戏进程内执行 `region`/`max_dimension`，`annotate` 为游戏 Control 画编号框；`click_game_ui_element` 按节点 path 在游戏进程内完成枚举+坐标注入（一次协议往返，避免主线程死锁），受 `game_runtime` 高风险确认约束（`confirm=true`）。

## 技术栈

| 层次 | 技术 | 来源 |
|---|---|---|
| 引擎 | Godot 4.x GDExtension（目标 4.7，见 `.gdextension` 的 `compatibility_minimum`） | `devtools/lib/gdextension.ts` |
| 绑定层 | godot-cpp，tag `10.0.0-stable`，submodule commit `507ed9d` | `third_party/godot-cpp`、`cmake/Dependencies.cmake` |
| MCP 协议 | mcp-cpp-sdk（modelcontextprotocol-cpp-sdk），tag `0.3.11`，submodule commit `93e18ef`，仓库 `jesspig/modelcontextprotocol-cpp-sdk` | 同上 |
| HTTP / 异步 | mcp-cpp-sdk 自研网络栈（内部 HTTP 线程） | mcp-cpp-sdk 内置（0.3.x 起移除 libhv） |
| JSON | mcp::JsonValue（SDK 内置，自研解析器） | mcp-cpp-sdk 内置（0.3.x 起移除 simdjson） |
| 构建 | CMake 3.28+（`cmake_minimum_required(3.28...4.2)`）、C++17、生成器双路径预设（有 ninja 用 `debug`/`release`，无则 `debug-portable`/`release-portable`） | 根 `CMakeLists.txt`、`CMakePresets.json`、`devtools/lib/config.ts` |
| 编译 | 优先 Clang/clang-cl 自动探测，MSVC/GCC 回退；sccache/ccache、LTO（ThinLTO/LTCG/IPO）、Unity 构建、作业池（仅 Ninja 注册）、按 ninja 可用性自动选预设 | `cmake/BuildOptimization.cmake`、`devtools/lib/cmake.ts` 等 |
| 测试 | googletest（`third_party/googletest` 子模块，tag `v1.15.2` / commit `b514bdc`；单元 + 集成两层 gtest，程序化四层口径见 [tests.md](./tests.md)） | `tests/` |

构建优化参数分两类：`GDA_COMPILE_JOBS`、`GDA_LINK_JOBS` 支持 CACHE（`-D`）与进程环境变量双通道；其余（`GDA_UNITY_BUILD`、`GDA_UNITY_BATCH_SIZE`、`GDA_MAX_COMPILE_MEM_MB`、`GDA_MAX_LINK_MEM_MB`、`GDA_UNITY_MEM_MB`）仅支持 `-D` CACHE 参数；`GDA_ARCH`、`GDA_IS_CI` 为探测结果变量，`GDA_CCACHE`/`GDA_SCCACHE` 为 `find_program` 探测结果（均非用户配置入口）。详见 [build.md](./build.md)。

## 目录结构

```text
godot-self-driving/
├── CMakeLists.txt              # add_library(godot-autopilot SHARED ...)，新 .cpp 必须登记
├── CMakePresets.json           # 6 个 configure 预设：base-debug/base-release（hidden 基座）+ debug/release（Ninja）+ debug-portable/release-portable（平台默认生成器）
├── INSTALL.md                  # 一键脚本、Release 手动安装、源码构建与 Agent 安装流程
├── README.md / README.zh-CN.md / README.zh-TW.md / README.ja.md / README.ko.md
├── devtools/                   # 开发链 Bun/TS 实现：main.ts + tui.ts + lib/{config,cmake,gdextension,zip,jam_projects}.ts + commands/{build,deploy,clean,package,package-check,test,jam}.ts + data/gamejam/ + templates/gamejam/
├── scripts/                    # 安装与发布辅助脚本：install.ps1 / install.sh
├── cmake/                      # 构建模块：Dependencies / BuildOptimization / Lto / Cache / CompilerOptions / Platform / skill_gen
├── tools/                      # 构建辅助脚本：embed_skills.ts（经 cmake/skill_gen.cmake 调用，构建期嵌入 skill 内容）
├── third_party/                # git submodule 依赖：godot-cpp / mcp-cpp-sdk / googletest（版本由 gitlink commit 锁定，见 .gitmodules）
├── src/
│   ├── main.cpp                # GDExtension 入口 + GodotAutopilotPlugin(EditorPlugin) 生命周期
│   ├── core/                   # 基础设施：CommandQueue(header-only)、config、LogSystem、ModeDetector、
│   │                           #   ResourceRegistry、SceneDirtyTracker、ExportGuard、ServerContext、PluginConfig、EditorReadiness、ErrorWatermark、EditorCoords、LogPersist、TraceRecorder、SanitizePolicy、Monitor、MonitorEnv、PerfSampler
│   ├── tools/                  # 领域工具：61 个 .cpp（domains/*/*_ops + base/register_all、dispatch、tool_args、tool_pipeline 等）、
│   │                           #   35 个域 _tools.hpp（246 条 ToolSpec，325 个别名）+ tool_spec.hpp / tool_registry.hpp / dynamic_spec_store.hpp /
│   │                           #   ready_policy.hpp（游戏通道就绪决策，header-only）/ autopilot_tools.{hpp,cpp}（用户脚本动态工具 API）
│   ├── resources/              # MCP Resources：resource_handlers + debugger_resources + skill_resources（skills 资源组，12 个资源）
│   ├── prompts/                # 提示词模板：12 个（prompt_handlers 7 通用 + debugger_prompts 5 调试）
│   ├── runtime/                # 游戏运行时桥接：game_bridge(±input/eval/verify) + gda_protocol.hpp
│   ├── ui/                     # 编辑器 UI：mcp_log_dock（0.3.2 删除 mcp_config_dock）
│   └── util/                   # 通用编译单元：variant_json、bm25_index、error_util、readback_util、scene_verify、
│   │                           #   skill_gen、skill_content_generated（构建期嵌入薄胶水）；共 27 个文件（7 cpp + 20 hpp）
│   │                           #   header-only（10 个）：json_godot、rid_registry、scene_path、project_path、type_hint、gdscript_wrap、resource_fs、mcp_image_content、inline_resource_json、owned_exception
├── skills/                     # 插件技能卡源（1 张入口卡 + 9 篇 references；共 10 个 .md）
├── cli/                        # CLI 载体（Bun + TS 源码直跑，零运行时依赖）：bin/gda.ts 入口 + src/ 54 个 .ts（含 src/mcp/ 协议客户端、src/ipc/ 本机通道、src/commands/ 22 个）+ tests/ 37 个 .test.ts
├── tests/                      # 单元/边界/集成 gtest 目标 + support/ 夹具 + guard/ 源码守卫（含 no_python_guard）+ probe/ 传输探针（数值见 tests.md 数值核算总表）
├── docs/                       # 本知识库（docs/wiki/，含 modules/、plans/、changelog/）
├── package.json                # CLI 包定义（name godot-autopilot-cli，bin gda → cli/bin/gda.ts，scripts.gda-dev-tools → devtools/main.ts，零运行时依赖）
├── bun.lock                    # CLI devDependency 锁（@types/bun + typescript）
└── examples/                   # 本地示例/e2e 工程（不入库；按 <主题>/<项目> 嵌套；deploy/clean 递归发现并跳过 intake；jam 生成 intake 与最终项目）
```

各子目录职责详见 [modules/](./modules/) 页面。

## 整体架构

```mermaid
flowchart LR
    Host[MCP 主机: Claude Desktop / Cursor / opencode 等] -->|POST http://127.0.0.1:&lt;port&gt;/mcp| HTTP[SDK HTTP 线程<br/>StreamableHttpServerTransport]
    HTTP -->|queue.submit() 返回 future| Q[CommandQueue 互斥队列]
    Q -->|_process 每帧 drain()| Main[Godot 主线程]
    Main --> API[Godot API / 场景 / 引擎]
    Main --> Dock[McpLogDock 日志面板]
    HTTP -->|engine-debugger 通道| Runtime[游戏运行时进程<br/>game_bridge 桥接]
    HTTP -.future.get() 同步等待.-> HTTP
```

- **线程模型**：所有 Godot API 调用必须经 `CommandQueue` 在主线程执行（`submit()` 或带预算的等待入口 `dispatch::run_on_main_thread_with_budget` / `dispatch::run_bounded_on_main_thread`）——HTTP 线程直接调用会崩溃。`CommandQueue::execute_sync()` 是**遗留入口**：`src/` 内已无任何调用点，实现是无界的 `submit(...).get()`，**不得新增调用**。`dispatch::call_handler` 按 `is_main_thread()` 决定直接执行或入队等待（`dispatch.cpp`）；09-29 起非主线程调用经 `run_on_main_thread_with_budget` 设主线程等待预算（默认 27000ms、健康探针 5000ms、长阻塞 29000ms（常量 `GDA_DISPATCH_WAIT_LONG_BLOCKING_MS = GDA_DISPATCH_WAIT_MAX_MS`，env `GODOT_AUTOPILOT_LONG_BLOCKING_TIMEOUT_MS` 可下调）），超时取消未执行任务并返回 `main_thread_timeout`；`call_tool` 元工具的编排回调本身在 MCP 线程执行（等待游戏响应/截图定型不再阻塞主线程消息泵），其内部经 dispatch 把领域工具 handler 路由回主线程。排空点唯一：`GodotAutopilotPlugin::_process()` 先调 `perf_sampler::tick(delta)`（09-21 起周期采样与请求超时看门狗），再调 `LogPersist::flush_on_main_thread()`（本地持久化写盘），最后 `s_queue.drain()`（`main.cpp`）。
- **生命周期**：`GDExtensionEntryPoint` → SCENE 级别（非编辑器进程注册桥接监听）/ EDITOR 级别（注册 6 个类：4 个插件/UI 类 + 2 个调试器捕获类，随后 `add_by_type`）→ 插件 `_enter_tree()` 建 UI、启 `ServerContext` → `_exit_tree()` 逆序清理。`gda_cmdline_mode()`（`GDA_FORCE_HEADLESS=1` 时禁用）下跳过 UI 与服务器。
- **观测/日志**：`LogCategory { System, Transport, Tools, Resources, Prompts }` 五类、`LogLevel { Debug, Info, Warning, Error }` 四级，内存环形缓冲上限 10000 条（另有 `log_detailed` 诊断 detail 与 `trace_id`/`span_id` 关联字段）；09-21 起所有埋点统一经 `src/core/monitor.*` 门面写入（可被 MCP SDK worker 线程调用），工具/协议请求/生命周期/数据流/性能/持久化健康等事件由 `TraceRecorder` 记录（容量 20000，`kind` 区分类型、`request_id` 串联端到端），`perf_sampler` 主线程周期采样、`monitor_env` 写 `kind=snapshot`；经 `LogPersist` 在主线程 flush 落盘到工程级目录（`<集中根>/projects/<项目名>-<路径哈希前 8 位>/logs/` 人类可读、`<集中根>/projects/<项目名>-<路径哈希前 8 位>/traces/` 结构化 jsonl，schema v2），默认脱敏。

## 命名约定

| 项 | 约定 |
|---|---|
| 命名空间 | `godot_autopilot`（部分子模块用子命名空间，如 `dispatch`、`resource_registry`、`scene_dirty_tracker`、`runtime::game_bridge`） |
| 工具命名 | `<动词>_<类别>_<维度>_<对象>_<修饰>`（动词置首，snake_case，如 `create_scene_node`、`property_set`、`resource_query`）；合并工具用 `action` 参数区分动作 |
| 代码前缀 | 常量 `GDA_`（如 `GDA_DEFAULT_PORT`）；运行时环境变量 `GODOT_AUTOPILOT_PORT`/`GODOT_AUTOPILOT_HOST`、`GDA_FORCE_HEADLESS` |
| 产物 | `godot-autopilot`（库名/插件目录/`.gdextension` 名） |
| 端口 | 首次启动在 20000–29999 随机分配并按工程持久化（`GODOT_AUTOPILOT_PORT` 环境变量覆盖），端点 `/mcp`；默认仅绑定 `127.0.0.1`，`GODOT_AUTOPILOT_HOST` 非环回值由 `ServerContext::start()` 拒绝 |

## 关键数字速查

数值（工具总数、schema 基线、单元/集成用例数、ctest 注册点、CLI 单测数、守卫等）以 [tests.md](tests.md)「数值核算总表」为唯一权威，本页不复制。端口与端点、派发预算、`code_execute` 超时等硬事实见上文各对应小节。

## 相关页面

- 构建与部署：[build.md](./build.md)
- 测试体系：[tests.md](./tests.md)
- 核心模块：[modules/core.md](./modules/core.md)
- 入口与运行时桥接：[modules/entry_runtime.md](./modules/entry_runtime.md)
