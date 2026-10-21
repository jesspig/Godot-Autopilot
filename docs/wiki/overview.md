---
type: 项目总览
title: 项目总览
description: 项目定位、整体架构、技术栈与目录结构的权威总览
tags:
  - 总览
  - 架构
  - 技术栈
timestamp: "2026-10-21T20:08:37+08:00"
resource:
  - README.md
  - README.zh-CN.md
  - src/
---

# 项目总览（Overview）

> 审计日期：2026-10-21（**10-21 随 C# 程序集构建与 .NET 环境判定修复批次更新**——目录树中的 L1 计数 687→**734** 个 gtest（62→**65** 文件：新增 `dotnet_env_test` 6 / `instance_guard_test` 14 / `transport_lifecycle_test` 17 三文件 37 项，`authorization_dock_toggle_test` 12→**22** 项）；面板可勾选能力由五项扩为**六项**（新增 `process`）；**10-21 随面板授权开关补齐批次更新**——目录树中的 L1 计数 675→**687** 个 gtest（61→**62** 文件，新增 `authorization_dock_toggle_test` 12 项）；**10-21 随数值同步批次更正**——「关键数字速查」的 MCP skills 资源 **46→45**（与 [测试体系](tests.md) 数值总表的 45 一致）；遍历基线数值不在本页复制，口径见 [tests.md](./tests.md)「数值核算总表」；10-21 随开放接口面落地批次同步——目录树三处计数同步：技能书 34→**35 个 .md**（新增 REST 接口参考）、CLI `src/` 10 模块 → **29 个 .ts**（新增 `src/api/client.ts`、`src/instances.ts`、`src/probe.ts`、`src/target.ts` 与 `src/commands/` 的 REST 子命令）、CLI TS 单测 132→**180**（9→11 文件）、L1 gtest 467→**675**（44→61 文件）、`tests/` 补 `probe/` 传输探针目录；数值权威见 [tests.md](./tests.md)「数值核算总表」；10-20 随 `code_execute` 超时上限收敛批次同步——长阻塞派发预算收敛为**单值 29000ms**（`GDA_DISPATCH_WAIT_LONG_BLOCKING_MS = GDA_DISPATCH_WAIT_MAX_MS`，删去「常量 30000 / 实际生效 29000」双值写法）；关键数值表补 `code_execute` 超时行（上界 = `min(27000, 生效长阻塞预算)`）；10-20 随技能书调用阶梯收敛批次同步——技能书 36→**34 个 .md**（9 份 `SKILL.md` + 25 份 references，无脚本文件），技能书自带的脚本桥 `gda.mjs` 与 `references/script-access.md` 删除，调用阶梯由五级收敛为三级（原生 MCP → GDA CLI → 手动兜底）；`register_skill_resources` 动态注册 46→**44** 个资源；10-20 随知识库事实描述页收敛批次同步——正文 7 处「392 域工具 / catalog 400」旧口径全部更正为实测 451 域 / catalog 459；类别分布表按 33 个域 `*_tools.hpp` 的 `ToolSpec.category` 重算 27 类、求和 451（原表分类不全、求和仅 384）；提示词 7→12（`RegisterPrompt` 实测调用数：通用 7 + 调试 5）；`src/tools/` 48→52 个 .cpp、域文件 30→33；`src/util/` 补 `inline_resource_json.hpp`/`owned_exception.hpp`（25 个文件 = 8 cpp + 17 hpp，header-only 10 个）；`src/resources/` 补 `skill_resources.hpp`（skills 资源组动态注册 44 个资源）；技能书 36→**34 个 .md**（9 份 `SKILL.md` + 25 份 references，无 .mjs）；`add_library` 89→93 个 .cpp；主线程派发预算新增「长阻塞 29000 ms」一档（常量 `GDA_DISPATCH_WAIT_LONG_BLOCKING_MS = GDA_DISPATCH_WAIT_MAX_MS` = 29000，经 `[1000, 29000]` 钳制；原为无限等待）；`main_thread_guard` 静态守卫入守卫清单（现共 6 个）；`side_effect` 对只读工具回显 `"none"`（原为空串）；`code_execute`/`batch_execute` 的 `side_effect` 改为 `SideEffect::CodeExecute`；README 引用改指实际存在的 `README.md`（英）/ `README.zh-CN.md` / `README.zh-TW.md` / `README.ja.md` / `README.ko.md`（`README.en.md` 不存在）；L1 436→460 gtest（43 个 unit 文件）、L2 38→41 份、CLI TS 111→132 用例（9 个文件）；数值核算总表见 [tests.md](./tests.md)）；2026-10-19（10-19 随 CLI 引擎定位收窄批次同步——`gda start` 不再读 `GODOT_PATH` 与 `<project>/.env`（前者仅 L2 引擎测试使用，生产环境不存在），可执行文件链收敛为 `--godot` > PATH，未命中即报错提示 `--godot`；新增 `cli/tests/process.test.ts` 3 项（TS 108→111、expect 301→310）；10-19 随 mcp-cpp-sdk 0.3.7→0.3.8 与 OpenSSL 显式关闭批次同步——子模块 gitlink `7dfe088`→`d79f9b1`（tag `0.3.8`），技术栈表 MCP 协议行同步；2026-10-19（10-19 随 MCP 协议版本协商修正批次同步——CLI 载体 `cli/src/mcp/client.ts` 与技能书 `gda.mjs` 脚本桥声明版本 `2025-03-26`→`2025-11-25`（SDK 旧世代当前版本）；`cmake/Dependencies.cmake` 的 `CMAKE_DISABLE_FIND_PACKAGE_OpenSSL ON` 删除、改 `MCP_WITH_OPENSSL "OFF"` CACHE STRING FORCE（仅 127.0.0.1 明文 HTTP，无 TLS 需求）；configure 实测 `[mcp] SDK version: 0.3.8` + `[mcp] OpenSSL disabled (MCP_WITH_OPENSSL=OFF)`，构建 1254/1254；详见 [build.md](./build.md) 页头 10-19 条目）；2026-10-18（10-18 随 CLI 载体落地批次同步——新增 `cli/`（Bun + TS 源码直跑，零运行时依赖）、根 `package.json`/`bun.lock`；目录树补 CLI 条目；双载体协议握手升级为标准 MCP `initialize` 流程；ctest 注册点 476→478（`skill_scripts` 由 `cli_contract` + `cli_unit` 取代，新增 `version_sync_guard`）；详见 [changelog/2026-10-18-log.md](./changelog/2026-10-18-log.md)）；2026-10-16（10-16 随 mcp-cpp-sdk 0.3.4→0.3.7 迁移同步——子模块 gitlink `e2ed352`→`7dfe088`（tag `0.3.7`），本页技术栈表 MCP 协议行同步；调用点改造与验证证据见 [build.md](./build.md) 页头 10-16 条目；10-16 晚随 `parse_csharp_classes` 缺陷修复与计数复核批次同步——域工具 448→451（+3：`csharp_list_members`/`create_csharp_script`/`dotnet_environment`）、全系统 456→459、schema 459 = 393 + 66、遍历枚举 452（10-16 实测 111 排除 / 341 候选），`register_all.cpp` 的 `call_tool` 描述行同步为“452 非元（451 域 + `system_status`）”，L2 全量 38/38、L1 全量 438；证据见 [changelog/2026-10-16-log.md](./changelog/2026-10-16-log.md)）；10-10 随依赖 submodule 化同步——godot-cpp / mcp-cpp-sdk / googletest 改为 `third_party/` 下的 git submodule（`cmake/Dependencies.cmake`，FetchContent 全部移除），版本由 gitlink commit 锁定，技术栈表、目录树与依赖版本段同步；工具与测试计数未变）；09-29 随 B1–B7 修复批次同步——工具 399→400（域工具 392，新增 `remove_resource_file`；8 个 SceneTree 工具改经 `GDA_OP_SCENE_TREE` 转发到游戏进程）、schema 基线 400 = 338 非空 + 62 空、遍历排除 62→71 / 候选 330→322；L1 315→391（unit 文件 33→36）、L2 30→37、ctest 注册点 431；游戏通道就绪门（`wait_ready` 排队重放，`ready_policy.hpp`）与 dispatch 主线程等待预算（默认 27000ms / 健康探针 5000ms）落地。详见 [测试体系](tests.md) 与 [核心模块](modules/core.md)；09-26 随 L2 测试床迁移同步——`Example/` 已移除，床改 `tests/testbed/`；2026-08-29 随 0.2.2 版本与全量审计同步；09-02 随安全与并行硬化同步；09-08 随 skill_gen 与测试数值同步，同日随 skill 内容外置化修正 src/util 目录树注释；09-10 随 skill 体系 19→7 册重构同步目录树注释与 McpConfigDock 描述；09-13 上午随反馈修复批次同步工具数 365/372/373、Resources 26、L1 114、L2 8 份与技能 8 册；09-13 下午随 T22 收口批次同步工具数 366/373/374、L1 126、L2 9 份、授权门与 get_plugin_log；09-13 晚随 A 组知识库审计修复批次同步 docs 与 cmake 目录树补正、skill_templates 平铺命名澄清；随后跨组复核纠正类别分布口径——以工具 category 字段统计为准（physics_tools.hpp 48 个工具中 get_debug_object_info 归类 Debug，故 Physics 47/Debug 16，27 类合计 366）并同步 README 口径 ~366），基于当前工作树文件与代码逐项核对（不依赖 git 历史）；09-13 晚随 0.2.4 版知识库全量审计同步——补正目录树（core 注释补 EditorReadiness/ErrorWatermark，新增 tools/ 嵌入脚本条目）、EDITOR 级别注册类数 4→6、InputMap 并入 Input 措辞、catalog/index 374 派生来源表述；工具数 366/373/374、27 类、L1 126、L2 9 份、ctest 135 复核无误；09-14 随修复批次同步——McpConfigDock 增列 Allow game_runtime 复选框表述；09-14 修复批次后 L1 140、L2 9、ctest 149；09-15 随 Computer Use grounding 增强批次同步工具数 379/386/387、非 SIDE 322 / SIDE 57、新增 13 个编辑器 UI/输入/显示/场景/游戏工具与截图增强参数、L1 172、L2 11、ctest 183；09-15 随 README/README_zh 工具数同步批次——两版 README 已同步 ~379 域工具口径，本页类别表 Input 15→23 纠正（27 类合计 379）；09-16 随失败修复批次（feature/failure-remediation）文档收尾同步——工具数 384/391/392、非 SIDE 324 / SIDE 60（`fill_tilemap_rect` 以 SIDE 宏声明但 side_effect=None）、新增 5 工具（Editor 2 / TileMap 1 / Game 2）、类别 Editor 29→31 / TileMap 7→8 / Game 10→12（27 类合计 384）、MCP 资源新增 skills 组、L1 229、L2 17 份、ctest 246，两版 README 同步 ~384；09-16 文档复核——删除临时批次汇总页（内容已并入功能页与 changelog），消除本页失效链接；09-16 随客户端配置生成器扩容批次同步——McpConfigDock 一键生成 8→20 个客户端（新增 ZCode/pi/Command Code/Kilo/Roo/Grok Build/Kimi Code/Zed/CodeBuddy/Crush/Copilot VS Code/Reasonix，调研淘汰 Cline 与 Antigravity CLI）、目录树 L1 238；09-16 随视觉辅助场景批次同步——域工具 384→385（Capture 1→2，新增 `review_scene_visually`）、可达 391→392、catalog/index 392→393、L1 gtest 238→246（新增 `vision_assist` 8 用例）、L2 17→18 份（新增 `17_vision_assist.json`）、ctest 246→264）。09-16 随坐标换算/键名统一/脚本新鲜度/UID 守卫/CJK 往返/open 幂等/属性两轮批次同步——L1 246→267（新增 game_ui_coords 6 / keycode_alias 6 / script_freshness 7 / uid_guard 2）、unit 文件 21→25、L2 18→25 份（新增 18_script_freshness / 19_cjk_roundtrip / 22_uid_guard / 23_click_ui_coords / 24_keycode_alias / 25_open_scene_idempotent / 28_sprite_frames_animation）、ctest 264→292（267 L1 + 25 L2）；09-17 随 T09+T12 文档同步批次更新技术栈依赖口径为 godot-cpp rc2 / mcp-cpp-sdk 0.3.4。09-29 随技能书源标准目录迁移同步。
> 09-18 补记：`README_zh.md` 已删除（英文 `README.md` + 中文 `README.zh-CN.md`/`README.zh-TW.md`/`README.ja.md`/`README.ko.md` 四个译本；工具口径 badge `tools-443+` / "Over 443 tools"），本页事实来源与工具总数段已改引新版 README。
> 09-20 代码-文档一致性审计：`register_all.cpp` 的 `call_tool` 描述字符串实测仍为旧口径"386 非元（385 域 + `system_status`）"，与 391 域实测不符（09-18"已修正"注记失实，系审计记录错误，源码并未改动），本轮已随源码修正为"392 非元（391 域 + `system_status`）"（`register_all.cpp:407`；仅描述文本，无行为影响，L1 无精确断言）。
> 09-19 ToolSpec 数据化重构：391 域工具 + `system_status` + 7 元工具全量改为 `ToolSpec` 数据记录（`make_spec_tool`），删除 `GDA_TOOL_CLASS`/`fn_tool.hpp`/`meta_tools.hpp`/`IMetaTool`/8 个 `schema_*_ops.cpp`/`schema_fills.hpp`/`tool_input_schema`；新增 `tool_args`（取参器）、`tool_pipeline`（`kObserve` 截图后处理）、`dynamic_spec_store`/`autopilot_tools`（用户脚本动态工具 + `user_tools` 授权门）、`tests/guard/` 迁移守卫；MCP 恒 7 元、catalog 399 不变；L1 243→255、L2 26→27 份（新增 `26_user_tools_code_mode`）、`ctest -E "^gda_runner_"` 256 项；遍历改为运行时枚举 392 条后按 `side_effect`/`mutating`/`dynamic` 排除（330 个进入两步骤全过）。
> 事实来源：根 `README.md` / `README.zh-CN.md` / `AGENTS.md`、`CMakeLists.txt`、`cmake/Dependencies.cmake`、`.gitmodules`、`third_party/`、`src/main.cpp`、`src/core/server_context.cpp`、`src/tools/tool_spec.hpp`、`src/tools/tool_registry.hpp`、`src/tools/*_tools.hpp`、`src/tools/dispatch.cpp`、`src/prompts/prompt_handlers.cpp`、`src/prompts/debugger_prompts.cpp`、`src/resources/resource_handlers.cpp`、`demo/*/project.godot`、`demo/*/docs/`、`tests/testbed/project.godot`。
> 09-20 随 Release 触发修复同步一致性核查行——tag 触发改为双格式（`v*.*.*` / `[0-9]*.*.*`）+ `workflow_dispatch` 手动指定 tag；同日代码-文档一致性审计——`call_tool` 描述口径随源码修正为 392 非元（391 域 + `system_status`），tests 目录数 255/27→269/28。
> 09-20 可重放监控与双目录持久化批次：新增 `src/core/` 的 LogPersist（日志/trace 双目录落盘与修剪）、TraceRecorder（结构化事件环形缓冲）、sanitize_policy（脱敏开关），`LogSystem` 增 `log_detailed`/`detail` 且 `filter_text` 兼匹配 detail；`SpecTool::execute`/`dispatch`/`tool_invoke` 埋点记录 trace（跨线程 span 上下文 + 排队等待）；McpLogDock 增 Detail 切换与 Open Logs、McpConfigDock 增 Desensitize data 复选框；`write_file` APPEND 首建回退修复。计数：L1 269→285（unit 31 文件）、L2 28→29（`29_trace_persistence`）、ctest 299→316；工具总数 399 不变。详见 [测试体系](tests.md) 与 [核心模块](modules/core.md)。
> 09-20 晚依赖升级：godot-cpp 10.0.0-rc2→10.0.0-stable（commit `507ed9d`）；mcp-cpp-sdk 维持 0.3.4（commit `e2ed352`，依赖升级范围经复核收缩，configure 实测 `[mcp] SDK version: 0.3.4`）；`AGENTS.md` 与 wiki 三页（build/conventions/overview）同步。
> 09-20 行号漂移修正（代码-文档一致性审计）：`register_all.cpp` 的 `call_tool` 描述字符串在 `:407`（`:406` 为 `ToolSpec` 起始）；目录树计数同步——`src/tools/` 48 个 .cpp、`src/util/` 8 个（补 `scene_verify.cpp`）、`src/runtime/` 4 个（补 `game_bridge_verify.cpp`），与 [build.md](./build.md) 分目录计数一致。
> 09-21 可重放监控与日志系统批次：新增 `src/core/` 的 `monitor`（统一埋点门面，纯 std，可被 MCP worker 线程调用）、`monitor_env`（`environment_snapshot()` 仅主线程写 `kind=snapshot`）、`perf_sampler`（主线程周期采样 `kind=perf` + 请求超时看门狗）；`TraceRecorder` 增 `TraceKind` 与 `TraceEvent` 新字段（jsonl schema v2）、`LogEntry` 增 `trace_id`/`span_id`、`LogPersist` 增健康计数与 `trace_dir`、`CommandQueue` 增 `Stats`；`server_context` 经 `opts.on_request`/`opts.outgoing_filters` 关联协议请求（`begin_request`/`end_request`）、元工具经 `RequestScope`+`RequestSpanGuard` 端到端关联；McpLogDock 增 Open Traces 与 Trace 视图。计数：`add_library` 86→89（core 12→15）、L1 285→312（unit 33 文件）、L2 29→30（`30_observability`）、ctest 316→344（L1 过滤 287→314）；工具总数 399 不变。详见 [测试体系](tests.md) 与 [核心模块](modules/core.md)。
> 09-25 技能脚本通道批次：skill 8→9 册（新增 `godot-autopilot-tools` 调用专册：调用协议/CLI 通道/工具目录/用户工具/手动兜底）、模板 35 md + 1 mjs + registry、references 22→24；L1 312→315、ctest 注册点 344→348（过滤 314→318）；MCP 仍为主产品通道，skill 与 CLI 为体验层。该脚本通道已于 10-20 随调用阶梯收敛为三档而删除，当前技能书为 35 个 .md。详见 [测试体系](tests.md) 与 [支撑模块](modules/support.md)。

## 项目定位

Godot-Autopilot 是一个 **MCP（Model Context Protocol）服务器**，以 **GDExtension 插件**形式**进程内加载到 Godot 编辑器**，在原生引擎 API 层面把 AI Agent 与 Godot 连接（场景树、物理服务器、渲染服务器、音频、导航、输入模拟、脚本执行等），而非模拟用户 UI 操作。

- **进程内 GDExtension**：插件随编辑器启动而启动、随编辑器关闭而停止，无独立桥接进程（`src/main.cpp` 的 `GDExtensionEntryPoint` 注册 `GodotAutopilotPlugin`，`_enter_tree()` 中创建并启动 `ServerContext`）。
- **产物名**：`godot-autopilot`（`.dll` / `.so` / `.dylib` + `.gdextension`），部署目录 `demo/*/addons/godot-autopilot/`（5 个官方示例工程）。
- **附属工程**：`demo/` 下 5 个官方示例工程的素材与干净配置（脚本/场景未入库），`main.py build` 构建后部署插件到其中。

## 核心能力

### MCP 工具：可达 458 个 = 7 元工具 + 451 领域工具

数值以 `src/tools/tool_spec.hpp`（`ToolSpec` 数据层）、`src/tools/tool_registry.hpp`（`ToolRegistry` 单一来源）与 `src/tools/*_tools.hpp`（451 域工具的 `make_tools()`，每工具一份 `ToolSpec`）为准，catalog/index 459 条目由 `register_all.cpp` 从 registry 派生；`get_active_registry()` 暴露活跃 registry，数量随插件版本变化，运行时可经 `search_tools` 确认。工具名遵循 `<动词>_<类别>_<维度>_<对象>_<修饰>`（动词置首，snake_case）。

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

**451 个领域工具**（经 `call_tool` 代理，`g_handlers` 映射分发），按 27 个类别组织（InputMap 并入 Input；TileMap 含 tileset），各类别数量以工具 category 字段/catalog 统计为准（如 `get_debug_object_info` 定义于 `physics_tools.hpp` 但归类 Debug）：

| 类别 | 数量 | 类别 | 数量 |
|---|---:|---|---:|
| Render | 52 | Theme | 10 |
| Physics | 50 | Animation | 11 |
| Resources | 34 | Text | 10 |
| Editor | 33 | Scripts | 18 |
| Display | 25 | Game | 23 |
| Audio | 24 | OS | 22 |
| Input | 24 | Config | 17 |
| Scene | 23 | Debug | 16 |
| Navigation | 15 | TileMap | 13 |
| Debugger | 6 | Properties | 7 |
| Docs | 4 | Group | 3 |
| SpriteFrames | 3 | Analysis | 3 |
| Testing | 2 | System | 1 |
| Capture | 2 |  |  |

（27 类求和 = 451，与域工具总数一致；统计口径为 33 个域 `*_tools.hpp` 的 `ToolSpec.category` 字段，TileMap 13 = tilemap 5 + tileset 3 + gridmap 5，Scene 23 = scene 15 + scene_tree 8，Properties 7 含 `set_node_transform`。）

另有 `system_status`（端口/版本/运行时长，`SpecTool`）经 `call_tool` 调用，不直接注册为 MCP 工具；`ToolCatalog`/BM25 index 合计 459 条目（451 域 + `system_status` + 7 元工具）。

### 提示词模板：12 个（7 通用 + 5 调试）

`src/prompts/prompt_handlers.cpp` 注册 7 个通用 prompt：`create-3d-scene`、`setup-character`、`debug-physics`、`setup-input-map`、`setup-gui`、`tool-usage`、`keycode-reference`。`src/prompts/debugger_prompts.cpp` 另注册 5 个调试专用 prompt：`debug-analyze-error`、`debug-analyze-breakpoint`、`debug-review-output`、`debug-review-performance`、`debug-session-status`。

### MCP 资源：15 个引擎/调试侧 + skills 资源组

`src/resources/resource_handlers.cpp` 注册 8 个：`godot://engine/version`、`godot://scene/tree`、`godot://scene/{path}`、`godot://filesystem/tree`、`godot://filesystem/{path}`、`godot://editor/selection`、`godot://editor/settings/{key}`、`godot://log/recent`。

`src/resources/debugger_resources.cpp` 另注册 7 个调试器资源：`godot://editor/output-log`、`godot://debugger/errors`、`godot://debugger/output`、`godot://debugger/stack-dump`、`godot://debugger/scene-tree`、`godot://debugger/monitors`、`godot://debugger/session`。

09-16 批起新增 skills 资源组（`register_skill_resources`，`src/resources/skill_resources.hpp` 提供 URI 解析）：`godot://skills`（目录，JSON 目录清单）、`godot://skills/<name>`（每册 SKILL.md）、`godot://skills/<name>/<file>`（每册 references 文件）——按 9 册 35 个 .md 动态注册，共 45 个资源（1 目录 + 9 单册 + 35 单文件），技能经 MCP 协议层可发现可读。

### 运行时桥接（game_* 工具）

编辑器进程的 MCP 服务器与游戏运行时进程经 EngineDebugger 消息通道（`gda:request` / `gda:response` / `gda:ready`）双向通信，`src/runtime/`（`game_bridge.cpp` / `game_bridge_input.cpp` / `game_bridge_eval.cpp` / `game_bridge_verify.cpp` + `gda_protocol.hpp`）实现输入模拟、GDScript 求值、截图捕获、日志/错误回传、场景树查询与 SceneTree 操作（09-29 新增 `scene_tree` op）等 op；游戏工具默认 `wait_ready=true`——游戏已启动但未就绪时请求排队、收到 `gda:ready` 后自动重放，无游戏进程时立即返回 `game not running: ...`。工具侧封装见 `game_*` / `get_game_log_entries` 等。详见 [modules/entry_runtime.md](./modules/entry_runtime.md)。

### 编辑器 UI

- 自定义底部日志面板 `McpLogDock`（"GDA Log"，按 LogLevel/LogCategory 过滤、文本搜索（09-21 起同匹配 message 与 detail）、折叠重复、Detail 切换显示诊断 detail、Open Logs 打开工程级日志目录（`<集中根>/projects/<项目名>-<路径哈希前 8 位>/logs`）、09-21 新增 Open Traces 打开工程级轨迹目录（`<集中根>/projects/<项目名>-<路径哈希前 8 位>/traces`）与 Trace 视图（日志行带可点击 `[trace]` 标记，点击按 trace_id 渲染结构化事件列表，默认关闭）；配置面板 "Show timestamps" 开关控制每条日志时间前缀 `[HH:MM:SS]`（本地时、时分秒），开启时默认生效并经集中根下的全局级 `config.json`（`<集中根>/config.json`，默认 `~/.godot-autopilot/config.json`；新路径缺该键时回退读旧 `user://godot_autopilot/config.json`）的 `show_time` 键持久化；折叠合并重复日志时除条数外始终显示最新一条的 `[HH:MM:SS]`，不受总开关控制）。
- 右侧配置面板 `McpConfigDock`（"MCP Config"：端口运行时重启 + 持久化、一键生成 20 个客户端 MCP 项目级配置（09-16 由 8 扩至 20：新增 ZCode/pi/Command Code/Kilo/Roo/Grok Build/Kimi Code/Zed/CodeBuddy/Crush/Copilot VS Code/Reasonix；调研淘汰 Cline 与 Antigravity CLI——项目级可能不生效）、Allow code_execute / Allow game_runtime / Allow user tools / Allow write / Allow destructive 授权复选框（写 `allow` 键持久化，下一次工具调用即生效，`GODOT_AUTOPILOT_ALLOW` 环境变量优先）、Desensitize data 复选框（09-20 新增，写 `desensitize` 键并即时改 `sanitize_policy`，`GODOT_AUTOPILOT_DESENSITIZE` 环境变量优先）、一键生成 9 册 Agent Skills 到项目根 .agents/skills/——Generate Skills / Update Skills 动态按钮，已有旧版技能目录时先清理再重建（详见 [modules/support.md](./modules/support.md)））。
- `ExportGuard`：导出期间拒绝领域工具调用（返回 `{"error":"editor is exporting; retry after export completes"}`）；导出期拒绝现覆盖**工具调用 / MCP 资源 / MCP 提示词**三类入口。导出标志为**实例级**——每个 `ExportGuard` 实例各持一份 `ExportState`，`is_exporting()` 遍历实例注册表，任一实例处于导出期即返回 true。

### Computer Use grounding（09-15）

- **编辑器 UI 语义自动化**：`get_editor_ui_elements` 枚举编辑器 UI 控件（语义 path / 类型 / 文本 / tooltip / 客户区矩形 / window_id，支持 `query`/`type_filter`/`interactive_only`/`max_elements`），`hit_test_editor_point` 返回某客户区点下的控件链（最上层优先）；动作侧 `click_editor_element`（按 path 点击，支持 `button`/`double_click`/`warp`/`observe`）、`type_editor_element_text`（聚焦并整体注入文本，可选 `submit`/`observe`）、`run_editor_shortcut`（解析 `ctrl+s` 形式快捷键并注入），原始坐标回退为 `click_input_mouse`/`scroll_input_mouse`/`drag_input_mouse`/`type_input_text`。
- **坐标映射**：`get_editor_viewport_geometry` 给出视口图像→窗口客户区的 `window = offset + image * scale` 映射（2D 另含 canvas origin/zoom），`get_scene_node_screen_rect` 把编辑场景节点映射到客户区坐标（2D/3D 自动或强制，Control 附 rect，顶层 `mapping` 用于截图像素换算），`get_display_window_rect` 返回窗口客户区矩形与所在屏幕几何/缩放（桌面坐标与客户区坐标互转）。
- **截图增强**：`capture_editor_viewport` 新增 `space`（`viewport`/`window`）、`region`（裁剪）、`max_dimension`（等比缩小、不放大）、`annotate`（编号红框 + elements）与 `diff_against_last`（与上次编辑器截图比较 changed_ratio/changed_bbox；尺寸不同时按公共左上区域比较并返回 `size_mismatch`/`current_size`；不可比较返回 `comparable=false` 与 `reason` ∈ {`no_baseline`, `unsupported_format`, `baseline_too_large`}）。
- **游戏侧**：`queue_game_input`/`sequence_game_inputs` 新增 wheel（`direction`/`amount` 或 button_index 4-7）与单步 `mouse_motion`；`capture_game_viewport` 的 `region`/`max_dimension` 在游戏进程内执行，`annotate` 为游戏 Control 画编号框；`click_game_ui_element` 按节点 path 在游戏进程内完成枚举+坐标注入（一次协议往返，避免主线程死锁），受 `game_runtime` 授权门约束。

## 技术栈

| 层次 | 技术 | 来源 |
|---|---|---|
| 引擎 | Godot 4.x GDExtension（目标 4.7，见 `demo/*/project.godot` features） | — |
| 绑定层 | godot-cpp，tag `10.0.0-stable`，submodule commit `507ed9d` | `third_party/godot-cpp`、`cmake/Dependencies.cmake` |
| MCP 协议 | mcp-cpp-sdk（modelcontextprotocol-cpp-sdk），tag `0.3.8`，submodule commit `d79f9b1`，仓库 `jesspig/modelcontextprotocol-cpp-sdk` | 同上 |
| HTTP / 异步 | mcp-cpp-sdk 自研网络栈（内部 HTTP 线程） | mcp-cpp-sdk 内置（0.3.x 起移除 libhv） |
| JSON | mcp::JsonValue（SDK 内置，自研解析器） | mcp-cpp-sdk 内置（0.3.x 起移除 simdjson） |
| 构建 | CMake 3.28+（`cmake_minimum_required(3.28...4.2)`）、C++17、生成器双路径预设（有 ninja 用 `debug`/`release`，无则 `debug-portable`/`release-portable`） | 根 `CMakeLists.txt`、`CMakePresets.json`、`scripts/config.py` |
| 编译 | 优先 Clang/clang-cl 自动探测，MSVC/GCC 回退；sccache/ccache、LTO（ThinLTO/LTCG/IPO）、Unity 构建、作业池（仅 Ninja 注册）、按 ninja 可用性自动选预设 | `cmake/BuildOptimization.cmake`、`scripts/cmake_ops.py` 等 |
| 测试 | googletest（`third_party/googletest` 子模块，tag `v1.15.2` / commit `b514bdc`，L1）；自研 `gda_test_runner` + `tests/config/*.json`（L2） | `tests/` |

构建优化参数分两类：`GDA_COMPILE_JOBS`、`GDA_LINK_JOBS` 支持 CACHE（`-D`）与进程环境变量双通道；其余（`GDA_UNITY_BUILD`、`GDA_UNITY_BATCH_SIZE`、`GDA_MAX_COMPILE_MEM_MB`、`GDA_MAX_LINK_MEM_MB`、`GDA_UNITY_MEM_MB`）仅支持 `-D` CACHE 参数；`GDA_ARCH`、`GDA_IS_CI` 为探测结果变量，`GDA_CCACHE`/`GDA_SCCACHE` 为 `find_program` 探测结果（均非用户配置入口）。详见 [build.md](./build.md)。

## 目录结构

```text
godot-self-driving/
├── CMakeLists.txt              # add_library(godot-autopilot SHARED ...)，新 .cpp 必须登记
├── CMakePresets.json           # 6 个 configure 预设：base-debug/base-release（hidden 基座）+ debug/release（Ninja）+ debug-portable/release-portable（平台默认生成器）
├── main.py                     # 构建入口（build/package/clean/tui 子命令）
├── scripts/                    # 构建模块：config/cmake_ops/deploy/package/clean_ops/tui
├── cmake/                      # 构建模块：Dependencies / BuildOptimization / Lto / Cache / CompilerOptions / Platform / skill_gen
├── tools/                      # 构建辅助脚本：embed_skills.py（经 cmake/skill_gen.cmake 调用，构建期嵌入 skill 内容）
├── third_party/                # git submodule 依赖：godot-cpp / mcp-cpp-sdk / googletest（版本由 gitlink commit 锁定，见 .gitmodules）
├── src/
│   ├── main.cpp                # GDExtension 入口 + GodotAutopilotPlugin(EditorPlugin) 生命周期
│   ├── core/                   # 基础设施：CommandQueue(header-only)、config、LogSystem、ModeDetector、
│   │                           #   ResourceRegistry、SceneDirtyTracker、ExportGuard、ServerContext、PluginConfig、EditorReadiness、ErrorWatermark、EditorCoords、LogPersist、TraceRecorder、SanitizePolicy、Monitor、MonitorEnv、PerfSampler
│   ├── tools/                  # 领域工具：52 个 .cpp（*_ops + register_all/dispatch/tool_args/tool_pipeline/...）、
│   │                           #   33 个域 _tools.hpp（451 条 ToolSpec）+ tool_spec.hpp / tool_registry.hpp / dynamic_spec_store.hpp /
│   │                           #   ready_policy.hpp（游戏通道就绪决策，header-only）/ autopilot_tools.{hpp,cpp}（用户脚本动态工具 API）
│   ├── resources/              # MCP Resources：resource_handlers + debugger_resources + skill_resources（skills 资源组，45 个资源）
│   ├── prompts/                # 提示词模板：12 个（prompt_handlers 7 通用 + debugger_prompts 5 调试）
│   ├── runtime/                # 游戏运行时桥接：game_bridge(±input/eval/verify) + gda_protocol.hpp
│   ├── ui/                     # 编辑器 UI：mcp_config_dock、mcp_log_dock
│   └── util/                   # 通用编译单元：variant_json、bm25_index、error_util、readback_util、scene_verify、client_config_gen、
│   │                           #   skill_gen、skill_content_generated（构建期嵌入薄胶水）；共 25 个文件（8 cpp + 17 hpp）
│   │                           #   header-only（10 个）：json_godot、rid_registry、scene_path、project_path、type_hint、gdscript_wrap、resource_fs、mcp_image_content、inline_resource_json、owned_exception
├── skills/                     # 插件技能书源（标准 Agent Skills 目录布局：9 册各一 SKILL.md + references/；共 35 个文件 = 9 个 SKILL.md + 26 个 references .md）
├── cli/                        # CLI 载体（Bun + TS 源码直跑，零运行时依赖）：bin/gda.ts 入口 + src/ 29 个 .ts（含 src/mcp/ 协议客户端、src/api/ REST 客户端、src/commands/ 15 个）+ tests/ 11 文件 180 用例
├── tests/                      # L1 gda_unit_tests（734 个 gtest，65 文件）+ L2 gda_test_runner + config/*.json（41 份）+ testbed/（L2 测试床工程）+ guard/ 迁移守卫、注释守卫、主线程守卫、版本同步守卫 + scripts/ CLI 契约与单测 + probe/ 传输探针
├── docs/                       # 本知识库（docs/wiki/，含 modules/、plans/、changelog/）
├── package.json                # CLI 包定义（name godot-autopilot-cli，bin gda → cli/bin/gda.ts，零运行时依赖）
├── bun.lock                    # CLI devDependency 锁（@types/bun + typescript）
└── demo/                       # 官方 demo 合集（5 个工程：文档/素材/工程配置，插件部署目标之一）
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

- **线程模型**：所有 Godot API 调用必须经 `CommandQueue` 在主线程执行（`submit()` 或带预算的等待入口 `dispatch::run_on_main_thread_with_budget` / `dispatch::run_bounded_on_main_thread`）——HTTP 线程直接调用会崩溃。`CommandQueue::execute_sync()` 是**遗留入口**：`src/` 内已无任何调用点，实现是无界的 `submit(...).get()`，**不得新增调用**。`dispatch::call_handler` 按 `is_main_thread()` 决定直接执行或入队等待（`dispatch.cpp`）；09-29 起非主线程调用经 `run_on_main_thread_with_budget` 设主线程等待预算（默认 27000ms、健康探针 5000ms、长阻塞 29000ms（常量 `GDA_DISPATCH_WAIT_LONG_BLOCKING_MS = GDA_DISPATCH_WAIT_MAX_MS`，env `GODOT_AUTOPILOT_LONG_BLOCKING_TIMEOUT_MS` 可下调）），超时取消未执行任务并返回 `main_thread_timeout`；`call_tool` 元工具的编排回调本身在 MCP 线程执行（等待游戏响应/截图定型不再阻塞主线程消息泵），其内部经 dispatch 把领域工具 handler 路由回主线程。排空点唯一：`GodotAutopilotPlugin::_process()` 先调 `perf_sampler::tick(delta)`（09-21 起周期采样与请求超时看门狗），再调 `LogPersist::flush_on_main_thread()`（本地持久化写盘），最后 `s_queue.drain()`（`main.cpp`）。
- **生命周期**：`GDExtensionEntryPoint` → SCENE 级别（非编辑器进程注册桥接监听）/ EDITOR 级别（注册 6 个类：4 个插件/UI 类 + 2 个调试器捕获类，随后 `add_by_type`）→ 插件 `_enter_tree()` 建 UI、启 `ServerContext` → `_exit_tree()` 逆序清理。`gda_cmdline_mode()`（`GDA_FORCE_HEADLESS=1` 时禁用）下跳过 UI 与服务器。
- **观测/日志**：`LogCategory { System, Transport, Tools, Resources, Prompts }` 五类、`LogLevel { Debug, Info, Warning, Error }` 四级，内存环形缓冲上限 10000 条（另有 `log_detailed` 诊断 detail 与 `trace_id`/`span_id` 关联字段）；09-21 起所有埋点统一经 `src/core/monitor.*` 门面写入（可被 MCP SDK worker 线程调用），工具/协议请求/生命周期/数据流/性能/持久化健康等事件由 `TraceRecorder` 记录（容量 20000，`kind` 区分类型、`request_id` 串联端到端），`perf_sampler` 主线程周期采样、`monitor_env` 写 `kind=snapshot`；经 `LogPersist` 在主线程 flush 落盘到工程级目录（`<集中根>/projects/<项目名>-<路径哈希前 8 位>/logs/` 人类可读、`<集中根>/projects/<项目名>-<路径哈希前 8 位>/traces/` 结构化 jsonl，schema v2），默认脱敏。

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
| 工具总数 | MCP 可达 458 = 7 元 + 451 领域（领域 27 类别，其中 Capture 2）；ToolCatalog/index 459 条目（含 `system_status`） |
| 提示词 / 资源 | 12 模板（7 通用 + 5 调试）/ 15 引擎与调试资源 + 45 skills 资源 |
| 派发预算 | 主线程等待三档：默认 27000 ms / 健康探针 5000 ms / 长阻塞 29000 ms（常量 `GDA_DISPATCH_WAIT_LONG_BLOCKING_MS = GDA_DISPATCH_WAIT_MAX_MS`，env `GODOT_AUTOPILOT_DISPATCH_TIMEOUT_MS` / `GODOT_AUTOPILOT_HEALTH_TIMEOUT_MS` / `GODOT_AUTOPILOT_LONG_BLOCKING_TIMEOUT_MS`，后者可下调至 `[1000, 29000]`） |
| `code_execute` 超时 | `timeout_ms` 上界 = `min(GDA_CODE_EXEC_MAX_TIMEOUT_MS = 27000, 生效长阻塞派发预算)`，默认环境下 27000 ms；schema `maximum` 与提示词同源 |
| side_effect | 只读工具回显 `"none"`（`side_effect_name(SideEffect::None)`）；`code_execute`/`batch_execute` 声明为 `code_execute`（`SideEffect::CodeExecute`） |
| 守卫 | 6 个：migration_guard / comment_guard / main_thread_guard / version_sync_guard / cli_contract / cli_unit |
| 日志类别 | 5（System / Transport / Tools / Resources / Prompts） |
| 引擎目标 | Godot 4.7（`demo/*/project.godot` features） |

## 文档一致性核查（速览）

- 端口 9527、`/mcp`、`GODOT_AUTOPILOT_PORT`：`README.md`（英）/ `README.zh-CN.md`、AGENTS.md、代码三方一致 ✓
- 工具总数：`README.md`（英）/ `README.zh-CN.md` 口径为 badge `tools-451+`（`README.md:8` / `README.zh-CN.md:8`）与“Over 451 tools”（`README.md:26`，域工具口径，数量随版本增长以 `search_tools` 查到的为准），与 wiki 实测的 **451 域工具**（以工具 category 字段统计为准）一致；wiki 另计 `system_status` 与 7 元工具，故 MCP 可达 **458** / catalog **459**；`ToolRegistry`/catalog 口径自洽（registry 459 = 451 域 + `system_status` + 7 元）✓
- CI：`.github/workflows/{ci,release}.yml`（develop 触发 CI、tag 双格式 `v*.*.*` / `[0-9]*.*.*` 触发 Release，另支持 `workflow_dispatch` 手动指定 tag），AGENTS.md 声称属实 ✓
- 目标引擎 4.7：与 `demo/*/project.godot` 及 `tests/testbed/project.godot` 的 features 一致 ✓；README 前提已为“Godot 4.7+”（`README.md:5,53` / `README.zh-CN.md:5,53`，与 wiki 的 `compatibility_minimum` 4.7 口径一致）
- 详细对照见 [modules/core.md](./modules/core.md) 的"不一致点"章节

## 相关页面

- 构建与部署：[build.md](./build.md)
- 测试体系：[tests.md](./tests.md)
- 核心模块：[modules/core.md](./modules/core.md)
- 入口与运行时桥接：[modules/entry_runtime.md](./modules/entry_runtime.md)
