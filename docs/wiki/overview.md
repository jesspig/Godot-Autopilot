---
type: 项目总览
title: 项目总览
description: 项目定位、整体架构、技术栈与目录结构的权威总览
tags:
  - 总览
  - 架构
  - 技术栈
timestamp: "2026-09-20T23:05:34+08:00"
resource:
  - README.md
  - README.en.md
  - src/
---

# 项目总览（Overview）

> 审计日期：2026-09-19（2026-08-29 随 0.2.2 版本与全量审计同步；09-02 随安全与并行硬化同步；09-08 随 skill_gen 与测试数值同步，同日随 skill 内容外置化修正 src/util 目录树注释；09-10 随 skill 体系 19→7 册重构同步目录树注释与 McpConfigDock 描述；09-13 上午随反馈修复批次同步工具数 365/372/373、Resources 26、L1 114、L2 8 份与技能 8 册；09-13 下午随 T22 收口批次同步工具数 366/373/374、L1 126、L2 9 份、授权门与 get_plugin_log；09-13 晚随 A 组知识库审计修复批次同步 docs 与 cmake 目录树补正、skill_templates 平铺命名澄清；随后跨组复核纠正类别分布口径——以工具 category 字段统计为准（physics_tools.hpp 48 个工具中 get_debug_object_info 归类 Debug，故 Physics 47/Debug 16，27 类合计 366）并同步 README 口径 ~366），基于当前工作树文件与代码逐项核对（不依赖 git 历史）；09-13 晚随 0.2.4 版知识库全量审计同步——补正目录树（core 注释补 EditorReadiness/ErrorWatermark，新增 tools/ 嵌入脚本条目）、EDITOR 级别注册类数 4→6、InputMap 并入 Input 措辞、catalog/index 374 派生来源表述；工具数 366/373/374、27 类、L1 126、L2 9 份、ctest 135 复核无误；09-14 随修复批次同步——McpConfigDock 增列 Allow game_runtime 复选框表述；09-14 修复批次后 L1 140、L2 9、ctest 149；09-15 随 Computer Use grounding 增强批次同步工具数 379/386/387、非 SIDE 322 / SIDE 57、新增 13 个编辑器 UI/输入/显示/场景/游戏工具与截图增强参数、L1 172、L2 11、ctest 183；09-15 随 README/README_zh 工具数同步批次——两版 README 已同步 ~379 域工具口径，本页类别表 Input 15→23 纠正（27 类合计 379）；09-16 随失败修复批次（feature/failure-remediation）文档收尾同步——工具数 384/391/392、非 SIDE 324 / SIDE 60（`fill_tilemap_rect` 以 SIDE 宏声明但 side_effect=None）、新增 5 工具（Editor 2 / TileMap 1 / Game 2）、类别 Editor 29→31 / TileMap 7→8 / Game 10→12（27 类合计 384）、MCP 资源新增 skills 组、L1 229、L2 17 份、ctest 246，两版 README 同步 ~384；09-16 文档复核——删除临时批次汇总页（内容已并入功能页与 changelog），消除本页失效链接；09-16 随客户端配置生成器扩容批次同步——McpConfigDock 一键生成 8→20 个客户端（新增 ZCode/pi/Command Code/Kilo/Roo/Grok Build/Kimi Code/Zed/CodeBuddy/Crush/Copilot VS Code/Reasonix，调研淘汰 Cline 与 Antigravity CLI）、目录树 L1 238；09-16 随视觉辅助场景批次同步——域工具 384→385（Capture 1→2，新增 `review_scene_visually`）、可达 391→392、catalog/index 392→393、L1 gtest 238→246（新增 `vision_assist` 8 用例）、L2 17→18 份（新增 `17_vision_assist.json`）、ctest 246→264）。09-16 随坐标换算/键名统一/脚本新鲜度/UID 守卫/CJK 往返/open 幂等/属性两轮批次同步——L1 246→267（新增 game_ui_coords 6 / keycode_alias 6 / script_freshness 7 / uid_guard 2）、unit 文件 21→25、L2 18→25 份（新增 18_script_freshness / 19_cjk_roundtrip / 22_uid_guard / 23_click_ui_coords / 24_keycode_alias / 25_open_scene_idempotent / 28_sprite_frames_animation）、ctest 264→292（267 L1 + 25 L2）；09-17 随 T09+T12 文档同步批次更新技术栈依赖口径为 godot-cpp rc2 / mcp-cpp-sdk 0.3.4。
> 09-18 补记：`README_zh.md` 已删除（中文 `README.md` + 英文 `README.en.md` 双版，工具口径 badge `tools-385+` / 385+ 个工具 / Over 385 tools），本页事实来源与工具总数段已改引新版 README。
> 09-20 代码-文档一致性审计：`register_all.cpp` 的 `call_tool` 描述字符串实测仍为旧口径"386 非元（385 域 + `system_status`）"，与 391 域实测不符（09-18"已修正"注记失实，系审计记录错误，源码并未改动），本轮已随源码修正为"392 非元（391 域 + `system_status`）"（`register_all.cpp:407`；仅描述文本，无行为影响，L1 无精确断言）。
> 09-19 ToolSpec 数据化重构：391 域工具 + `system_status` + 7 元工具全量改为 `ToolSpec` 数据记录（`make_spec_tool`），删除 `GDA_TOOL_CLASS`/`fn_tool.hpp`/`meta_tools.hpp`/`IMetaTool`/8 个 `schema_*_ops.cpp`/`schema_fills.hpp`/`tool_input_schema`；新增 `tool_args`（取参器）、`tool_pipeline`（`kObserve` 截图后处理）、`dynamic_spec_store`/`autopilot_tools`（用户脚本动态工具 + `user_tools` 授权门）、`tests/guard/` 迁移守卫；MCP 恒 7 元、catalog 399 不变；L1 243→255、L2 26→27 份（新增 `26_user_tools_code_mode`）、`ctest -E "^gda_runner_"` 256 项；遍历改为运行时枚举 392 条后按 `side_effect`/`mutating`/`dynamic` 排除（330 个进入两步骤全过）。
> 事实来源：根 `README.md` / `README.en.md` / `AGENTS.md`、`CMakeLists.txt`、`cmake/FetchDependencies.cmake`、`src/main.cpp`、`src/core/server_context.cpp`、`src/tools/tool_spec.hpp`、`src/tools/tool_registry.hpp`、`src/tools/*_tools.hpp`、`src/tools/dispatch.cpp`、`src/prompts/prompt_handlers.cpp`、`src/resources/resource_handlers.cpp`、`Example/project.godot`、`Example/docs/`。
> 09-20 随 Release 触发修复同步一致性核查行——tag 触发改为双格式（`v*.*.*` / `[0-9]*.*.*`）+ `workflow_dispatch` 手动指定 tag；同日代码-文档一致性审计——`call_tool` 描述口径随源码修正为 392 非元（391 域 + `system_status`），tests 目录数 255/27→269/28。
> 09-20 可重放监控与双目录持久化批次：新增 `src/core/` 的 LogPersist（日志/trace 双目录落盘与修剪）、TraceRecorder（结构化事件环形缓冲）、sanitize_policy（脱敏开关），`LogSystem` 增 `log_detailed`/`detail` 且 `filter_text` 兼匹配 detail；`SpecTool::execute`/`dispatch`/`tool_invoke` 埋点记录 trace（跨线程 span 上下文 + 排队等待）；McpLogDock 增 Detail 切换与 Open Logs、McpConfigDock 增 Desensitize data 复选框；`write_file` APPEND 首建回退修复。计数：L1 269→285（unit 31 文件）、L2 28→29（`29_trace_persistence`）、ctest 299→316；工具总数 399 不变。详见 [测试体系](tests.md) 与 [核心模块](modules/core.md)。
> 09-20 晚依赖升级：godot-cpp 10.0.0-rc2→10.0.0-stable（`cmake/FetchDependencies.cmake:20`；`_deps` checkout `10.0.0-stable`/507ed9d）；mcp-cpp-sdk 维持 0.3.4（依赖升级范围经复核收缩，`cmake/FetchDependencies.cmake:29`；configure 实测 `[mcp] SDK version: 0.3.4`）；`AGENTS.md` 与 wiki 三页（build/conventions/overview）同步。
> 09-20 行号漂移修正（代码-文档一致性审计）：`register_all.cpp` 的 `call_tool` 描述字符串在 `:407`（`:406` 为 `ToolSpec` 起始）；目录树计数同步——`src/tools/` 48 个 .cpp、`src/util/` 8 个（补 `scene_verify.cpp`）、`src/runtime/` 4 个（补 `game_bridge_verify.cpp`），与 [build.md](./build.md) 分目录计数一致。

## 项目定位

Godot-Autopilot 是一个 **MCP（Model Context Protocol）服务器**，以 **GDExtension 插件**形式**进程内加载到 Godot 编辑器**，在原生引擎 API 层面把 AI Agent 与 Godot 连接（场景树、物理服务器、渲染服务器、音频、导航、输入模拟、脚本执行等），而非模拟用户 UI 操作。

- **进程内 GDExtension**：插件随编辑器启动而启动、随编辑器关闭而停止，无独立桥接进程（`src/main.cpp` 的 `GDExtensionEntryPoint` 注册 `GodotAutopilotPlugin`，`_enter_tree()` 中创建并启动 `ServerContext`）。
- **产物名**：`godot-autopilot`（`.dll` / `.so` / `.dylib` + `.gdextension`），部署目录 `Example/addons/godot-autopilot/`。
- **附属工程**：`Example/` 是文档/示例工程（详见 [example.md](./example.md)），`build.py` 构建后部署插件到其中。

## 核心能力

### MCP 工具：可达 398 个 = 7 元工具 + 391 领域工具

数值以 `src/tools/tool_spec.hpp`（`ToolSpec` 数据层）、`src/tools/tool_registry.hpp`（`ToolRegistry` 单一来源）与 `src/tools/*_tools.hpp`（391 域工具的 `make_tools()`，每工具一份 `ToolSpec`）为准，catalog/index 399 条目由 `register_all.cpp` 从 registry 派生；`get_active_registry()` 暴露活跃 registry，数量随插件版本变化，运行时可经 `search_tools` 确认。工具名遵循 `<动词>_<类别>_<维度>_<对象>_<修饰>`（动词置首，snake_case）。

**7 个元工具**（`SpecTool` + `tool_flags::kMeta`，顶层 MCP 工具，见 `register_all.cpp`）：

| 元工具 | 职责 |
|---|---|
| `ping` | 健康检查 |
| `search_tools` | BM25 关键词检索工具 |
| `list_categories` | 列出全部类别 |
| `get_tool_detail` | 获取单个工具完整 schema |
| `call_tool` | 按名称代理调用领域工具（检查 `error` 字段并置 `is_error`） |
| `batch_execute` | 顺序批量执行多工具 |
| `code_execute` | 执行任意 GDScript（临时 Node + `_run()`，可选 `function_name` 多函数模式） |

**391 个领域工具**（经 `call_tool` 代理，`g_handlers` 映射分发），按 27 个类别组织（InputMap 并入 Input；TileMap 含 tileset），各类别数量以工具 category 字段/catalog 统计为准（如 `get_debug_object_info` 定义于 `physics_tools.hpp` 但归类 Debug）：

| 类别 | 数量 | 类别 | 数量 |
|---|---:|---|---:|
| Render | 49 | Text | 10 |
| Physics | 47 | Animation | 10 |
| Resources | 26 | Scripts | 11 |
| Display | 25 | Game | 15 |
| Editor | 32 | Theme | 8 |
| Audio | 20 | TileMap | 8（tilemap 5+tileset 3） |
| OS | 18 | Debugger | 6 |
| Debug | 16 | Properties | 5 |
| Input | 23 | Docs | 4 |
| Navigation | 15 | Group | 3 |
| Scene | 16（scene 8+scene_tree 8） | SpriteFrames | 3 |
| Config | 13 | Analysis | 3 |
| Testing | 2 | System | 1 |
| Capture | 2 |  |  |

另有 `system_status`（端口/版本/运行时长，`SpecTool`）经 `call_tool` 调用，不直接注册为 MCP 工具；`ToolCatalog`/BM25 index 合计 399 条目（391 域 + `system_status` + 7 元工具）。

### 提示词模板：7 个

`src/prompts/prompt_handlers.cpp` 注册：`create-3d-scene`、`setup-character`、`debug-physics`、`setup-input-map`、`setup-gui`、`tool-usage`、`keycode-reference`。另有调试器专用 prompts（`src/prompts/debugger_prompts.cpp`）。

### MCP 资源：15 个引擎/调试侧 + skills 资源组

`src/resources/resource_handlers.cpp` 注册 8 个：`godot://engine/version`、`godot://scene/tree`、`godot://scene/{path}`、`godot://filesystem/tree`、`godot://filesystem/{path}`、`godot://editor/selection`、`godot://editor/settings/{key}`、`godot://log/recent`。

`src/resources/debugger_resources.cpp` 另注册 7 个调试器资源：`godot://editor/output-log`、`godot://debugger/errors`、`godot://debugger/output`、`godot://debugger/stack-dump`、`godot://debugger/scene-tree`、`godot://debugger/monitors`、`godot://debugger/session`。

09-16 批起新增 skills 资源组（同文件 `register_skill_resources`）：`godot://skills`（目录，JSON 目录清单）、`godot://skills/<name>`（每册 SKILL.md）、`godot://skills/<name>/<file>`（每册 references 文件）——按 8 册 30 个 .md 动态注册，技能经 MCP 协议层可发现可读。

### 运行时桥接（game_* 工具）

编辑器进程的 MCP 服务器与游戏运行时进程经 EngineDebugger 消息通道（`gda:request` / `gda:response` / `gda:ready`）双向通信，`src/runtime/`（`game_bridge.cpp` / `game_bridge_input.cpp` / `game_bridge_eval.cpp` / `game_bridge_verify.cpp` + `gda_protocol.hpp`）实现输入模拟、GDScript 求值、截图捕获、日志/错误回传、场景树查询等 op。工具侧封装见 `game_*` / `get_game_log_entries` 等。详见 [modules/entry_runtime.md](./modules/entry_runtime.md)。

### 编辑器 UI

- 自定义底部日志面板 `McpLogDock`（"GDA Log"，按 LogLevel/LogCategory 过滤、文本搜索、折叠重复、Detail 切换显示诊断 detail、Open Logs 打开 `user://godot_autopilot/logs`；配置面板 "Show timestamps" 开关控制每条日志时间前缀 `[HH:MM:SS]`（本地时、时分秒），开启时默认生效并经 `user://godot_autopilot/config.json` 的 `show_time` 键持久化；折叠合并重复日志时除条数外始终显示最新一条的 `[HH:MM:SS]`，不受总开关控制）。
- 右侧配置面板 `McpConfigDock`（"MCP Config"：端口运行时重启 + 持久化、一键生成 20 个客户端 MCP 项目级配置（09-16 由 8 扩至 20：新增 ZCode/pi/Command Code/Kilo/Roo/Grok Build/Kimi Code/Zed/CodeBuddy/Crush/Copilot VS Code/Reasonix；调研淘汰 Cline 与 Antigravity CLI——项目级可能不生效）、Allow code_execute / Allow game_runtime / Allow user tools 授权复选框（写 `allow` 键持久化，下一次工具调用即生效，`GODOT_AUTOPILOT_ALLOW` 环境变量优先）、Desensitize data 复选框（09-20 新增，写 `desensitize` 键并即时改 `sanitize_policy`，`GODOT_AUTOPILOT_DESENSITIZE` 环境变量优先）、一键生成 8 册 Agent Skills 到项目根 .agents/skills/——Generate Skills / Update Skills 动态按钮，已有旧版技能目录时先清理再重建（详见 [modules/support.md](./modules/support.md)））。
- `ExportGuard`：导出期间拒绝领域工具调用（返回 `{"error":"editor is exporting; ..."}`）。

### Computer Use grounding（09-15）

- **编辑器 UI 语义自动化**：`get_editor_ui_elements` 枚举编辑器 UI 控件（语义 path / 类型 / 文本 / tooltip / 客户区矩形 / window_id，支持 `query`/`type_filter`/`interactive_only`/`max_elements`），`hit_test_editor_point` 返回某客户区点下的控件链（最上层优先）；动作侧 `click_editor_element`（按 path 点击，支持 `button`/`double_click`/`warp`/`observe`）、`type_editor_element_text`（聚焦并整体注入文本，可选 `submit`/`observe`）、`run_editor_shortcut`（解析 `ctrl+s` 形式快捷键并注入），原始坐标回退为 `click_input_mouse`/`scroll_input_mouse`/`drag_input_mouse`/`type_input_text`。
- **坐标映射**：`get_editor_viewport_geometry` 给出视口图像→窗口客户区的 `window = offset + image * scale` 映射（2D 另含 canvas origin/zoom），`get_scene_node_screen_rect` 把编辑场景节点映射到客户区坐标（2D/3D 自动或强制，Control 附 rect，顶层 `mapping` 用于截图像素换算），`get_display_window_rect` 返回窗口客户区矩形与所在屏幕几何/缩放（桌面坐标与客户区坐标互转）。
- **截图增强**：`capture_editor_viewport` 新增 `space`（`viewport`/`window`）、`region`（裁剪）、`max_dimension`（等比缩小、不放大）、`annotate`（编号红框 + elements）与 `diff_against_last`（与上次编辑器截图比较 changed_ratio/changed_bbox；尺寸不同时按公共左上区域比较并返回 `size_mismatch`/`current_size`；不可比较返回 `comparable=false` 与 `reason` ∈ {`no_baseline`, `unsupported_format`, `baseline_too_large`}）。
- **游戏侧**：`queue_game_input`/`sequence_game_inputs` 新增 wheel（`direction`/`amount` 或 button_index 4-7）与单步 `mouse_motion`；`capture_game_viewport` 的 `region`/`max_dimension` 在游戏进程内执行，`annotate` 为游戏 Control 画编号框；`click_game_ui_element` 按节点 path 在游戏进程内完成枚举+坐标注入（一次协议往返，避免主线程死锁），受 `game_runtime` 授权门约束。

## 技术栈

| 层次 | 技术 | 来源 |
|---|---|---|
| 引擎 | Godot 4.x GDExtension（目标 4.7，见 `Example/project.godot` features） | — |
| 绑定层 | godot-cpp，GIT_TAG `10.0.0-stable` | `cmake/FetchDependencies.cmake` |
| MCP 协议 | mcp-cpp-sdk（modelcontextprotocol-cpp-sdk），GIT_TAG `0.3.4`，仓库 `jesspig/modelcontextprotocol-cpp-sdk` | 同上 |
| HTTP / 异步 | mcp-cpp-sdk 自研网络栈（内部 HTTP 线程） | mcp-cpp-sdk 内置（0.3.x 起移除 libhv） |
| JSON | mcp::JsonValue（SDK 内置，自研解析器） | mcp-cpp-sdk 内置（0.3.x 起移除 simdjson） |
| 构建 | CMake 3.28+（`cmake_minimum_required(3.28...4.2)`）、C++17、Ninja 预设（`debug`/`release`） | 根 `CMakeLists.txt`、`CMakePresets.json` |
| 编译 | 优先 Clang/clang-cl 自动探测，MSVC/GCC 回退；sccache/ccache、LTO（ThinLTO/LTCG/IPO）、Unity 构建、作业池 | `cmake/BuildOptimization.cmake` 等 |
| 测试 | googletest（FetchContent 拉取，L1）；自研 `gda_test_runner` + `tests/config/*.json`（L2） | `tests/` |

构建优化参数分两类：`GDA_COMPILE_JOBS`、`GDA_LINK_JOBS` 支持 CACHE（`-D`）与进程环境变量双通道；其余（`GDA_UNITY_BUILD`、`GDA_UNITY_BATCH_SIZE`、`GDA_MAX_COMPILE_MEM_MB`、`GDA_MAX_LINK_MEM_MB`、`GDA_UNITY_MEM_MB`）仅支持 `-D` CACHE 参数；`GDA_ARCH`、`GDA_IS_CI` 为探测结果变量，`GDA_CCACHE`/`GDA_SCCACHE` 为 `find_program` 探测结果（均非用户配置入口）。详见 [build.md](./build.md)。

## 目录结构

```text
godot-self-driving/
├── CMakeLists.txt              # add_library(godot-autopilot SHARED ...)，新 .cpp 必须登记
├── CMakePresets.json           # debug / release（Ninja）
├── build.py                    # 构建 + 部署到 Example/addons/godot-autopilot/
├── cmake/                      # 构建模块：FetchDependencies / BuildOptimization / Lto / Cache / CompilerOptions / Platform / skill_gen
├── tools/                      # 构建辅助脚本：embed_skills.py（经 cmake/skill_gen.cmake 调用，构建期嵌入 skill 内容）
├── src/
│   ├── main.cpp                # GDExtension 入口 + GodotAutopilotPlugin(EditorPlugin) 生命周期
│   ├── core/                   # 基础设施：CommandQueue(header-only)、config、LogSystem、ModeDetector、
│   │                           #   ResourceRegistry、SceneDirtyTracker、ExportGuard、ServerContext、PluginConfig、EditorReadiness、ErrorWatermark、EditorCoords、LogPersist、TraceRecorder、SanitizePolicy
│   ├── tools/                  # 领域工具：48 个 .cpp（*_ops + register_all/dispatch/tool_args/tool_pipeline/...）、
│   │                           #   30 个域 _tools.hpp（391 条 ToolSpec）+ tool_spec.hpp / tool_registry.hpp / dynamic_spec_store.hpp /
│   │                           #   autopilot_tools.{hpp,cpp}（用户脚本动态工具 API）
│   ├── resources/              # MCP Resources：resource_handlers + debugger_resources
│   ├── prompts/                # 提示词模板：7 个主题 + debugger_prompts
│   ├── runtime/                # 游戏运行时桥接：game_bridge(±input/eval/verify) + gda_protocol.hpp
│   ├── ui/                     # 编辑器 UI：mcp_config_dock、mcp_log_dock
│   └── util/                   # 通用编译单元：variant_json、bm25_index、error_util、readback_util、scene_verify、client_config_gen、
│   │                           #   skill_gen、skill_content_generated（构建期嵌入薄胶水）；内容目录
│   │                           #   skill_templates/（8 册模板 = 平铺 30 个 .md：主册 <name>.md + 参考 <name>--<ref>.md，另有 registry.json）；
│   │                           #   header-only：json_godot、rid_registry、scene_path、project_path、type_hint、gdscript_wrap、mcp_image_content
├── tests/                      # L1 gda_unit_tests（285 个 gtest）+ L2 gda_test_runner + config/*.json（29 份）+ guard/ 迁移守卫与注释守卫
├── docs/                       # 本知识库（docs/wiki/，含 modules/、plans/、changelog/）
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

- **线程模型**：所有 Godot API 调用必须经 `CommandQueue::submit()` 或 `execute_sync()` 由主线程执行——HTTP 线程直接调用会崩溃。`dispatch::call_handler` 按 `is_main_thread()` 决定直接执行或入队等待（`dispatch.cpp`）；`call_tool` 元工具的编排回调本身在 MCP 线程执行（等待游戏响应/截图定型不再阻塞主线程消息泵），其内部经 dispatch 把领域工具 handler 路由回主线程。排空点唯一：`GodotAutopilotPlugin::_process()` 先调 `LogPersist::flush_on_main_thread()`（本地持久化写盘）再调 `s_queue.drain()`（`main.cpp`）。
- **生命周期**：`GDExtensionEntryPoint` → SCENE 级别（非编辑器进程注册桥接监听）/ EDITOR 级别（注册 6 个类：4 个插件/UI 类 + 2 个调试器捕获类，随后 `add_by_type`）→ 插件 `_enter_tree()` 建 UI、启 `ServerContext` → `_exit_tree()` 逆序清理。`gda_cmdline_mode()`（`GDA_FORCE_HEADLESS=1` 时禁用）下跳过 UI 与服务器。
- **观测/日志**：`LogCategory { System, Transport, Tools, Resources, Prompts }` 五类、`LogLevel { Debug, Info, Warning, Error }` 四级，内存环形缓冲上限 10000 条（另有 `log_detailed` 诊断 detail）；工具调用 trace 由 `TraceRecorder` 记录（容量 20000），经 `LogPersist` 在主线程 flush 落盘 `user://godot_autopilot/logs/`（人类可读）与 `traces/`（结构化 jsonl），默认脱敏。

## 命名约定

| 项 | 约定 |
|---|---|
| 命名空间 | `godot_autopilot`（部分子模块用子命名空间，如 `dispatch`、`resource_registry`、`scene_dirty_tracker`、`runtime::game_bridge`） |
| 工具命名 | `<动词>_<类别>_<维度>_<对象>_<修饰>`（动词置首，snake_case，如 `intersect_physics_2d_ray`、`create_scene_node`、`set_input_map_action_deadzone`） |
| 代码前缀 | 常量 `GDA_`（如 `GDA_DEFAULT_PORT`）；运行时环境变量 `GODOT_AUTOPILOT_PORT`/`GODOT_AUTOPILOT_HOST`、`GDA_FORCE_HEADLESS` |
| 产物 | `godot-autopilot`（库名/插件目录/`.gdextension` 名） |
| 端口 | 9527（`GDA_DEFAULT_PORT`，`config.hpp`），`GODOT_AUTOPILOT_PORT` 环境变量覆盖，端点 `/mcp`；默认仅绑定 `127.0.0.1`，`GODOT_AUTOPILOT_HOST` 非环回值由 `ServerContext::start()` 拒绝 |

## 关键数字速查

| 项 | 值 |
|---|---|
| MCP 端口 / 端点 | 9527 / `/mcp`（环境变量 `GODOT_AUTOPILOT_PORT` 覆盖；默认仅绑定 `127.0.0.1`，非环回 host 启动时拒绝） |
| 工具总数 | MCP 可达 398 = 7 元 + 391 领域（领域 27 类别，其中 Capture 2）；ToolCatalog/index 399 条目（含 `system_status`） |
| 提示词 / 资源 | 7 模板 / 15 资源 |
| 日志类别 | 5（System / Transport / Tools / Resources / Prompts） |
| 引擎目标 | Godot 4.7（`Example/project.godot` features） |

## 文档一致性核查（速览）

- 端口 9527、`/mcp`、`GODOT_AUTOPILOT_PORT`：README（英/中）、AGENTS.md、代码三方一致 ✓
- 工具总数：`README.md` / `README.en.md` 口径为 badge `tools-385+`（`README.md:8` / `README.en.md:8`）与“385+ 个工具 / Over 385 tools”（域工具约数口径，数量随版本增长以 `search_tools` 查到的为准），wiki 实测 **可达 398 / catalog 399 / 27 类 = 391**（以工具 category 字段统计为准）——两者为“约数 vs 实测”口径差异，README 未列 `system_status` 与 7 元工具；`ToolRegistry`/catalog 口径自洽（registry 399 = 391 域 + `system_status` + 7 元）✓
- CI：`.github/workflows/{ci,release}.yml`（develop 触发 CI、tag 双格式 `v*.*.*` / `[0-9]*.*.*` 触发 Release，另支持 `workflow_dispatch` 手动指定 tag），AGENTS.md 声称属实 ✓
- 目标引擎 4.7：与 `Example/project.godot` 一致 ✓；README 前提已为“Godot 4.7+”（`README.md:5,53` / `README.en.md:5,53`，与 wiki 的 `compatibility_minimum` 4.7 口径一致）
- 详细对照见 [example.md](./example.md) 与 [modules/core.md](./modules/core.md) 的"不一致点"章节

## 相关页面

- 示例工程：[example.md](./example.md)
- 构建与部署：[build.md](./build.md)
- 测试体系：[tests.md](./tests.md)
- 核心模块：[modules/core.md](./modules/core.md)
- 入口与运行时桥接：[modules/entry_runtime.md](./modules/entry_runtime.md)
