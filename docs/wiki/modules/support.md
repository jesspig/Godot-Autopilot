---
type: 模块文档
title: 支撑模块
description: 提示词、MCP 资源、编辑器 UI 与通用工具库实现细节
tags:
  - 模块
  - 提示词
  - 资源
  - UI
  - 工具库
timestamp: "2026-09-14T20:49:22+08:00"
resource:
  - src/prompts/
  - src/resources/
  - src/ui/
  - src/util/
---

# 支撑模块（src/prompts/、src/resources/、src/ui/、src/util/）

> 审计日期：2026-09-13（2026-08-29 随 0.2.2 版本与全量审计同步；09-02 随安全与并行硬化同步；09-08 随 skill_gen 一键生成 Agent Skills 与 skill 内容外置化同步；09-10 随 skill 体系 19→7 册重构、Godot 源码研究发现织入与 dock 按钮动态化同步；09-13 上午随 7→8 册（C# 专册与开发闭环）与 util 新增 mcp_image_content.hpp 同步；09-13 下午随收口批次同步——McpConfigDock Allow code_execute 复选框、VariantJson::deserialize_strict、readback_util 值类型近似比较与 16 类清单、技能 runtime 册日志四路来源；09-13 17:50 随 B 组知识库审计同步——修正 prompt_tool_usage 行数、resource_handlers 静态/模板配比、注册入口行号与 keycode 提示词状态；09-13 20 时随主册技能增强同步——description 必读定位与 autopilot.md 新增查文档时机/引擎状态观测/工具选择/搜索技巧四块正文；09-13 21 时统一 skill 为纯英文——scene-system.md 中文错误示例改英文转述、registry description 英文必读定位；09-13 晚随 0.2.4 版知识库全量审计同步——修正 PACKED_*_ARRAY 为 10 种、scene_path/json_number 消费方计数与 resolve_rid 未命中行为；09-14 随修复批次同步——McpConfigDock 新增 Allow game_runtime 复选框与 `allow_list_add`/`allow_list_remove` 共享切换逻辑（`all` 展开、`code_execute_allowed()` 删除）、Vector2/2i 严格形状与 set_resource_property 接入严格转换、capture 落盘 save 与两侧各保留 20 张、get_game_log_entries filter/matched_lines、batch_execute 异步 pending 计数），基于当前工作树代码逐行核对（不依赖 git 历史）。
> 覆盖范围：`src/prompts/` 9 组文件（18 个）、`src/resources/` 2 组、`src/ui/` 2 组、`src/util/` 13 组（20 个文件，其中 `scene_path.hpp`/`json_godot.hpp`/`rid_registry.hpp`/`type_hint.hpp`/`gdscript_wrap.hpp`/`project_path.hpp`/`mcp_image_content.hpp` 为 header-only；另含内容目录 `skill_templates/` 31 个文件——30 个 .md + registry.json）。注册入口为 `src/core/server_context.cpp:214-220`（`register_tools()` 内五处注册调用：工具 → 资源 → 通用 prompt → 调试资源 → 调试 prompt）。

## 模块简介

这四组文件不直接产生领域工具，而是为 MCP 服务器与编辑器体验提供支撑：

- **Prompts**：向 MCP 客户端暴露 12 个 prompt 模板（7 通用 + 5 调试），全部经 `server.RegisterPrompt` 注册。
- **Resources**：暴露 15 个 MCP resource URI（8 引擎侧 + 7 调试捕获侧），全部为 `godot://` 前缀。
- **UI**：底部日志面板（`McpLogDock`）、右侧配置面板（`McpConfigDock`），消费 `LogSystem` 与服务器生命周期。
- **Util**：与工具层共享的纯工具件——Variant↔JSON 互转、BM25 检索、错误 JSON 构造、写后读回校验、场景路径解析、JSON 数值/几何/RID 辅助、进程内 RID 注册表、类型提示推断、GDScript 包装流水线、MCP 图像内容交付、客户端 MCP 配置生成、Agent Skills 文档生成。

## Prompts（12 个，均经 RegisterPrompt 注册）

### 职责表

| 注册函数 | 文件 | 数量 | 内容生成方式 |
|---|---|---|---|
| `register_all_prompts(server, queue)` | `prompt_handlers.cpp` + 7 个 `prompt_*.cpp/hpp` | 7 | 内容函数经 `queue.submit()` 在主线程执行（`prompt_handlers.cpp:28-113`） |
| `register_debugger_prompts(server)` | `debugger_prompts.cpp/hpp` | 5 | 纯文本模板直接同步构造（不触碰 Godot API，不经 queue） |

所有 prompt 返回值统一为单条 `role=assistant` 的 `TextContent` 消息（两处 `make_result` 实现相同）。注册完成后各写一条 `Prompts` 类别日志："7 prompt templates registered" / "Registered debugger prompts: 5 templates"。

### 通用 prompt 名单（register_all_prompts）

| 注册名 | 描述（注册时） | 内容来源函数 | 正文要点 |
|---|---|---|---|
| `create-3d-scene` | Guide to create a basic 3D scene with camera, lighting, and a test object | `prompt_create_3d_scene()`（130 行） | 分步：Node3D 根 → Camera3D → DirectionalLight3D → 环境/测试对象，各步附 `create_scene_node`/`property_set` JSON 示例 |
| `setup-character` | Guide to set up a 3D character controller with CharacterBody3D, collision, and movement script | `prompt_setup_character()`（86 行） | CharacterBody3D + CollisionShape3D + CapsuleShape3D + `create_script`/`attach_script_to_node`，附完整 player.gd 模板 |
| `debug-physics` | Guide to use physics debugging tools including ray casts, shape casts, and performance monitors | `prompt_debug_physics()`（134 行） | `intersect_physics_3d_ray`/点查询/监视器示例（shape_cast 工具已随重命名删除） |
| `setup-input-map` | Guide to configure input actions in Project Settings and test them | `prompt_setup_input_map()`（144 行） | 经 `call_tool` 调 `set_project_settings`/`save_project_settings` 配置 `input/move_*` 等动作与按键事件 |
| `setup-gui` | Guide to create a simple GUI with CanvasLayer, containers, buttons, and labels | `prompt_setup_gui()`（202 行） | CanvasLayer → VBoxContainer → 标签/按钮 + 信号连接示例 |
| `tool-usage` | Usage examples for the 17 most commonly used MCP tools with JSON input/output and gotchas | `prompt_tool_usage()`（685 行） | 实际含 **19 个**工具章节（见"不一致点"） |
| `keycode-reference` | Godot keycode reference for InputEventKey and Variant type JSON mapping for property operations | `prompt_keycode_reference()`（213 行） | ASCII 键码表、Godot 特殊键码（`KEY_SPECIAL=4194304` 基值）、`add_input_map_action_event` 的 event JSON、Variant→JSON 映射表 |

### 调试 prompt 名单（register_debugger_prompts，可带可选参数）

| 注册名 | 描述 | 支持参数 | 正文引导使用的工具 |
|---|---|---|---|
| `debug-analyze-error` | Analyze a runtime error with full context | `error_text`（追加到文末） | `get_debugger_errors`、`get_debugger_log` |
| `debug-analyze-breakpoint` | Analyze the current breakpoint context: stack trace, scene tree, and variable state | 无 | `execute_game_script`、`get_game_log_entries`、`get_debugger_scene_tree`、`get_debugger_session_info` |
| `debug-review-output` | Review the game's output log for issues | `since`（追加到文末） | `get_debugger_log`、`get_debugger_output` |
| `debug-review-performance` | Review performance monitor data to identify bottlenecks | 无 | `get_debug_monitors`、`get_game_status` |
| `debug-session-status` | Quick summary of the current debug session state | 无 | `get_debugger_session_info` 等 |

## Resources（15 个 URI，均经 RegisterResource/RegisterResourceTemplate 注册）

### 职责表

| 组件 | 文件 | 数量 | 注册方式 | MIME | 执行方式 |
|---|---|---|---|---|---|
| `register_all_resources(server, queue)` | `resource_handlers.cpp/hpp` | 8（5 静态 + 3 模板：`scene-node`/`filesystem-path`/`editor-setting`） | `godot://` URI，`application/json` | 每个 handler 经 `queue.execute_sync()` 在主线程执行 |
| `register_debugger_resources(server, queue)` | `debugger_resources.cpp/hpp` | 7（全静态） | `godot://` URI，`text/plain` | 每个 handler 经 `queue.execute_sync()` 执行，`debugger-session` 的 Godot debugger 查询也在主线程执行 |

**注意：`resource_handlers` 不是"扩展名注册表"**——它注册的是 MCP Resource（URI 分发），而非 Godot 资源扩展名映射；扩展名/类型相关逻辑在 `tools/resource_ops` 侧。

### resource_handlers 名单（application/json）

| 名称 | URI | 输出要点 |
|---|---|---|
| `engine-version` | `godot://engine/version` | `Engine::get_version_info()` 经 `VariantJson::serialize` |
| `scene-tree` | `godot://scene/tree` | 编辑场景树：`name`/`class`/`path`（相对根前缀）/`children` 递归 |
| `scene-node`（模板） | `godot://scene/{path}` | 节点详情：属性（跳过 INTERNAL/GROUP/CATEGORY/SUBGROUP usage）、`groups`、`script`（class+path）、子节点摘要 |
| `filesystem-tree` | `godot://filesystem/tree` | `EditorFileSystemDirectory` 树：`subdirs`/`files`（name/path/type） |
| `filesystem-path`（模板） | `godot://filesystem/{path}` | 目录列出或单文件信息；路径无 `res://` 前缀时自动补全 |
| `editor-selection` | `godot://editor/selection` | 选中节点数组（name/class/相对 path）；无选择则 `[]` |
| `editor-setting`（模板） | `godot://editor/settings/{key}` | 单键编辑器设置值 |
| `log-recent` | `godot://log/recent` | `LogSystem` 最近 50 条：`level`/`category`/`message`/`timestamp`（ISO8601） |

错误统一为 JSON 对象 `{"error": "消息"}`（本文件内 `set_error`，与 `error_util::error_json` 同构）。

### debugger_resources 名单（text/plain）

| 名称 | URI | 数据来源（limit） |
|---|---|---|
| `editor-output-log` | `godot://editor/output-log` | `capture_get_log_text(200)` |
| `debugger-errors` | `godot://debugger/errors` | `capture_get_errors_text(50)` |
| `debugger-output` | `godot://debugger/output` | `capture_get_game_output_text(200)` |
| `debugger-stack-dump` | `godot://debugger/stack-dump` | `capture_get_stack_dump_text()` |
| `debugger-scene-tree` | `godot://debugger/scene-tree` | `capture_get_scene_tree_text()` |
| `debugger-monitors` | `godot://debugger/monitors` | `capture_get_monitors_text(5)` |
| `debugger-session` | `godot://debugger/session` | `capture_get_session_info_text()` |

## UI

### McpLogDock（`mcp_log_dock.cpp/hpp`）

`EditorDock` 子类，标题 "GDA Log"，默认停靠底部槽（`DOCK_SLOT_BOTTOM`），可关闭，最小高度 `DEFAULT_DOCK_HEIGHT = 200`：

- **布局**：VBoxContainer = `RichTextLabel`（threaded、BBCode、scroll_follow、可选择、段落上限 `LINE_LIMIT = 5000` 超限删首段）+ 底部 `HFlowContainer`（Clear 按钮、Collapse 切换按钮、搜索 `LineEdit`、类别 `OptionButton`、4 个级别过滤按钮）
- **过滤**：4 个级别按钮（Debug/Info/Warning/Error，图标 `Debug`/`Popup`/`StatusWarning`/`StatusError`，按钮文本显示各级计数）；类别下拉 All/System/Transport/Tools/Resources/Prompts；搜索为大小写不敏感子串（`findn`）；判定在 `_check_display`
- **时间前缀（Show timestamps）**：配置面板 "Show timestamps" 开关（默认开启，经 `user://godot_autopilot/config.json` 的 `show_time` 键持久化）控制每条日志前缀 `[HH:MM:SS]`（本地时、时分秒）；折叠（合并）重复日志时，除条数 `(N)` 前缀外**始终**显示最新一条的 `[HH:MM:SS]`，该最新时间显示不受总开关控制
- **折叠**：`collapse` 开关，按消息聚合相同文本并显示 `(N)` 次数前缀；折叠模式改动触发全量重建
- **主题**：`_update_theme()` 从编辑器主题取 `error_color`/`warning_color`/`font_color`（无则回退硬编码色）、`output_source*` 字体族、各按钮图标；`NOTIFICATION_ENTER_TREE`/`NOTIFICATION_THEME_CHANGED` 时刷新
- **日志来源**：构造时取 `LogSystem::instance()` 指针；`poll_new_entries()` 用 `query_from(last_index_)` 增量拉取（`main.cpp:_process` 每帧调用）；`refresh()`/`_rebuild_log()` 全量重建；`_on_clear()` 清显示并把 `last_index_` 推进到 `log_system->next_index()`（只清界面，不删 LogSystem 缓冲）

### McpConfigDock（`mcp_config_dock.cpp/hpp`）

`EditorDock` 子类，标题 "MCP Config"，默认停靠右侧槽（`DOCK_SLOT_RIGHT_UR`），可关闭：

- **布局**：VBoxContainer = "MCP Server" 标题 + 端口行（Label + SpinBox 1–65535 + Apply 按钮 + 运行状态 Label）→ Show timestamps 复选框 → Allow code_execute 复选框（09-13 下午新增）→ Allow game_runtime 复选框（09-14 新增）→ 分隔线 → 客户端配置区（`OptionButton` 下拉选择 8 个客户端，label 含配置文件路径）+ Generate 按钮 + 结果 Label + 生效条件提示 Label → 技能生成区（Generate Skills / Update Skills 动态按钮）
- **Allow code_execute / Allow game_runtime 复选框（09-13 下午新增 code_execute，09-14 新增 game_runtime）**：两个勾选框分别转发到共享槽函数 `_on_allow_toggled(capability, box, checked)`——勾选用 `authorization::allow_list_add` 追加能力（已生效或配置含 `all` 时保持原样），取消用 `allow_list_remove` 移除（先把 `all` 展开为全部已知能力再逐项删除）；初值与操作后校准均用 `allow_list_contains` + `set_pressed_no_signal`（dock 私有的 `split_allow`/`join_allow` 与 `code_execute_allowed()` 已删除）。下次工具调用即生效（授权门每次实时读取配置），`GODOT_AUTOPILOT_ALLOW` 环境变量存在时优先于该配置（tooltip 已注明）。`process` 无 dock 开关，需环境变量或手改配置的 `allow` 键；拒绝响应的 `enable` 文案同样只对 `code_execute`/`game_runtime` 提及 dock（`authorization::capability_has_dock_toggle`）。
- **技能生成（动态按钮）**：按钮文本按 `.agents/skills/` 下是否已存在 `godot-autopilot-` 前缀目录动态切换——无 → "Generate Skills"，有 → "Update Skills"；Update 点击先递归删除全部前缀匹配目录再整体重新生成（语义详见下文 skill_gen 小节）
- **端口管理**：`set_server_context(ServerContext*)` 注入服务器（null 时禁用 Apply）；Apply → `ServerContext::restart(port)` → 成功后 `PluginConfig::save_port(port)` 持久化，并刷新面板内运行状态 Label（"Running on port N" / "Server offline"，主题色标注）
- **配置生成**：`_on_generate()` 只处理下拉选中的单个客户端——目标目录 `ProjectSettings::globalize_path("res://")`；文件不存在 → `render_config` 新建；JSON 已存在 → `merge_json_config` 合并（**先解析现有配置，保留其他键，仅更新 `mcp`/`mcpServers` 下的 `godot-autopilot` 条目**，不覆盖用户的其他 agent 配置）；Codex TOML 已含 `mcp_servers` → 跳过并提示；JSON 无法解析 → 跳过不写（防覆盖）；结果单文件报告「创建/更新/跳过」原因
- **主题**：颜色经 `theme_color()` 从编辑器主题取 `success_color`/`error_color`/`warning_color`/`font_disabled_color`（无则回退硬编码色）

### client_config_gen（`src/util/client_config_gen.cpp/hpp`，命名空间 `godot_autopilot::client_config_gen`）

纯函数生成器（**仅依赖 std + `mcp::JsonValue`，无 Godot API**，L1 可测）：

| 客户端 | 文件路径 | 顶层键 | 条目字段 |
|---|---|---|---|
| OpenCode | `opencode.json` | `mcp` | `type=remote`、`url`、`enabled=true` |
| Claude Code | `.mcp.json` | `mcpServers` | `type=http`、`url` |
| Codex | `.codex/config.toml` | `[mcp_servers.godot-autopilot]`（TOML） | `url` |
| Cursor | `.cursor/mcp.json` | `mcpServers` | `url`（无 type） |
| GitHub Copilot | `.github/mcp.json` | `mcpServers` | `type=http`、`url` |
| Trae | `.trae/mcp.json` | `mcpServers` | `url`（无 type） |
| Qoder | `.qoder/settings.json` | `mcpServers` | `type=http`、`url` |
| WorkBuddy | `.workbuddy/mcp.json` | `mcpServers` | `type=http`、`url` |

- 服务器名常量 `kServerName = "godot-autopilot"`；URL 统一 `http://127.0.0.1:<port>/mcp`（DNS rebinding 保护要求 127.0.0.1）
- `render_config(ClientId, port)` — 全新文件内容（JSON `Dump(2)` 缩进 / TOML 字符串）
- `merge_json_config(ClientId, port, existing)` — 空串 → 视为新建；Parse 失败或非对象 → `Unparsable`；否则更新顶层键下同名条目并保留其余内容
- `merge_toml_config(port, existing)` — 已含 `[mcp_servers` 段 → `AlreadyConfigured`；否则追加段（保留原文）
- `display_name` / `file_path` / `description` — UI label 与报告用

### skill_gen（`src/util/skill_gen.cpp/hpp` + `skill_content_generated.cpp` + `skill_templates/`，命名空间 `godot_autopilot::skill_gen`，09-08 新增；09-10 由 19 册重构为 7 册；09-13 增补 C# 专册与开发闭环，共 8 册）

"一键生成 Agent Skills"：把 8 册英文 Agent Skills（符合 agentskills.io 规范）写入项目根 `.agents/skills/`，供外部编码代理加载。与 `client_config_gen` 同为 UI 面板消费的纯函数生成器（**仅依赖 std，无 Godot API**，L1 可测）：

- **结构（09-10 七册化，09-13 增补至八册）**：原 19 册合并为 7 册——1 册插件总纲 `godot-autopilot` + 6 册引擎指南（`godot-autopilot-{scene-system,resources,scripting,runtime,servers,content}`）；09-13 新增 1 册 C# 专册 `godot-autopilot-csharp`（C# 编译循环、`[GlobalClass]` 资源、`[Export]` 属性、无热重载限制）。渲染后每册均带 `references/` 子文档（渐进披露，细节由子文档承载，合计 22 个 references：新增 `autopilot--development-workflow.md`（改→跑→观察→诊断→修→复验的完整闭环）与 `csharp--tool-reference.md`；模板源目录为平铺命名，`<name>--<ref>.md` 经 registry.json 映射为 `references/<ref>.md`）。**全部 84 条 Godot 4.8.0-dev 源码研究发现织入引擎六册**（撤销三历史、RID 不级联、暂停矩阵、`just_pressed` 双计数器、调试三道闸门、uid 优先于 path、导航双缓冲、`set_cell` 静默清格等），行内以 "4.7+"/"4.8" 简注标注适用版本
- **内容承载（09-08 外置化，09-13 随册数同步）**：正文不再由 7 个 `skill_content_*.cpp`（已删除）手写，外置为 `src/util/skill_templates/`（31 个文件——30 个 .md + `registry.json`；registry 含 8 条 name/description/files 映射，聚合顺序固化于此）+ `tools/embed_skills.py`（纯 stdlib）构建期嵌入——生成头 `skill_content_embedded.h` 入 `build/<preset>/generated/`（gitignore 覆盖），**内容变更不再触碰 C++**。构建期 5 项校验：恰 `SKILL_COUNT = 8` 条（常量，与 registry 册数保持同步）、name 规则唯一、description ≤1024、files 结构与 source 存在、孤儿 .md 与重复引用（失败非零退出）；定界符 `gda_s` 碰撞自动避让；`cmake/skill_gen.cmake` 中 py launcher 优先（`find_program(NAMES py python python3 REQUIRED)`，本机 python 为 WindowsApps 存根）并 configure 期 `--version` 自检（失败 FATAL_ERROR）
- **API**：`SkillSpec{name, description, files}`；`skill_gen.hpp` 的 7 个 make_* 声明收敛为 1 个 `make_embedded_skills()`，`all_skills()` 改为其转发（顺序由 registry.json 固化）；`skill_file_path()` 拼生成路径；`render_skill_md()` 渲染 YAML frontmatter——`name`/`description`（值含 ": " 时加引号）/`metadata{author: godot-autopilot, version: "<GDA_VERSION>"}`（version 由 configure_file 注入，与 `server_info` 同源）
- **生成路径**：`<res://>/.agents/skills/<name>/SKILL.md`；`name` 小写连字符且与目录一致、description ≤1024；8 册全部带 `references/` 子文件
- **UI 入口（09-10 动态化）**：McpConfigDock 面板按钮文本动态切换（见上文 McpConfigDock 小节），槽函数 `_on_generate_skills()`：先 `has_existing_skills()` 探测——有既有 `godot-autopilot-` 前缀目录则经 `remove_legacy_skill_dirs()`（`remove_dir_recursive` 后序遍历 + `DirAccess::remove`）整体删除后重建，旧 19 册在用户项目中随之自动退役；随后 `DirAccess::make_dir_recursive_absolute` 建目录并复用 `write_file` 写入；成功文案动态计数 "Generated/Updated N skills in .agents/skills/ (M reference files)"，失败列相对路径
- **覆盖写策略**：写入与清理均限于自有 `godot-autopilot-` 前缀命名空间，不触碰 `.agents/skills/` 下其他内容
- **8 册清单**（name 与吸收来源）：
  - `godot-autopilot`——插件总纲，description 以 "Required reading before using the GDA (godot-autopilot) plugin" 开头（全英文）并以此定位本册（09-13 晚增强；09-13 21 时 skills 内容与 description 统一纯英文）：连接前提、工具发现协议（search first, never guess；Discovery protocol 追加 Search techniques——按对象/动词搜索、同义词重试、`category`/`tags` 过滤、BM25 排序受措辞影响、无果查 references/tool-catalog.md）、引擎文档查询时机（新增 Consult the engine documentation——文档工具直读运行中引擎自带的离线文档缓存、随引擎版本匹配，禁止凭记忆调 API）、引擎状态观测（新增 Watch the engine state——写操作后先读 `new_errors_since_last_call`，经编辑器日志/调试器/磁盘日志/插件日志取真实错误行，不连续盲操作）、任务到工具选择清单（Task routing 前置 Choosing the right tool——改属性→`property_set` 并回读、查属性→`property_get`/`property_get_list`、瓦片→`set_tilemap_cell`/`set_tilemap_cells` 与 tileset 工具、节点/资源/脚本/文档/运行时/日志各归其工具、批量→`batch_execute`、循环与数学→`code_execute`）、错误协议（watermark/retryable）、改→跑→观察→诊断→修→复验的开发闭环，吸收原 usage / direct-http / tool-map / tips-gotchas（references：development-workflow、http-fallback、tool-catalog、tool-gotchas）
  - `godot-autopilot-scene-system`——场景系统：场景生命周期、节点增删改名换父、属性 JSON 值形状、信号接线、undo 历史与 .tscn 序列化，吸收原 scene-building + properties-signals + inspection 的编辑侧
  - `godot-autopilot-resources`——资源与文件：load/save/create/duplicate、事务化 rename/move 与引用改写、UID 与依赖管理、导入管线，吸收原 resources-files
  - `godot-autopilot-scripting`——GDScript：四执行通道、@tool 语义、static 初始化与编辑器/游戏进程边界，原 scripting 就地重写保留
  - `godot-autopilot-runtime`——运行与调试：启停游戏、帧精确输入注入、暂停语义、日志四路来源（编辑器引擎日志/调试会话/磁盘日志/插件 LogSystem `get_plugin_log`）与 watermark 确认环、运行时检查，吸收原 running-games + debugging + inspection 的运行侧
  - `godot-autopilot-servers`——服务器层 RID 工具：RenderingServer/PhysicsServer/NavigationServer 语义与静默失败、物理 tick 次序、导航同步槽，吸收原 physics-navigation + rendering-text
  - `godot-autopilot-content`——内容管线：TileMap/TileSet、AnimationPlayer/Tree、音频总线与播放、Control 主题与布局及各域静默失败，吸收原 tilemap + animation + audio + ui-theming + spriteframes
  - `godot-autopilot-csharp`（09-13 新增）——C#/.NET 项目指南：`build_csharp_assembly` 编译循环、经 `get_debugger_log` 读编译/解析错误、运行确认（`get_debugger_errors` + 错误水印）、`[GlobalClass]` 资源经 `create_resource`、`[Export]` 节点与 typed-array 的 `property_set` 转换、附加 C# 脚本，以及 .NET 程序集无热重载、GDScript 不可见等限制（references：tool-reference）
- **测试**：`tests/unit/skill_gen_test.cpp` 7 用例（SkillGenTest 6 个 TEST + SkillRegistryFixture 1 个 TEST_F）——8 册完整性与名单精确比对（`AllEightSkillsPresent`）、name 规范 `^[a-z0-9]+(-[a-z0-9]+)*$` 且与目录一致、description ≤1024、文件布局无 PLACEHOLDER、frontmatter 渲染、工具名回验（对照运行时 catalog∪schema 参数名∪176 项白名单，注册管线见 [tools_registry.md](tools_registry.md)）、每册 ≥1 references 断言（`EverySkillDeclaresReferences`）；8 册化后用例数保持 7 项（L1 总数见 [tests.md](../tests.md)）
- **构建接线**：`cmake/skill_gen.cmake` 经 `add_custom_command` 生成嵌入头 + `add_custom_target(gda_skill_embed_header)`；根 `CMakeLists.txt` add_library 与 `tests/CMakeLists.txt` GDA_UNIT_BUSINESS_SOURCES 的 skill 源各为 2 个（`src/util/skill_gen.cpp` + `src/util/skill_content_generated.cpp` 薄胶水：`embedded::all()` → `SkillSpec`），并各 `add_dependencies(... gda_skill_embed_header)`；`skill_templates/*.md` 为数据文件，不进 add_library

## Util

### VariantJson（`variant_json.cpp/hpp`）

静态方法 `serialize(Variant) → JsonValue`、`deserialize(JsonValue, type_hint = "") → Variant`、`deserialize_strict(JsonValue, type_hint) → Variant`（09-13 下午新增：命中的类型走严格形状校验，违规抛 `std::runtime_error`；未命中类型退回 `deserialize`）。AGENTS.md 标注的 "`VariantJson::serialize/deserialize` 用于 `godot::Variant ↔ mcp::JsonValue` 互转" 与实现一致。

**serialize 类型覆盖**（switch 全量分支，另加特例）：

| 类型组 | JSON 形态 |
|---|---|
| NIL / BOOL / INT / FLOAT / STRING | null / bool / number / string |
| VECTOR2/2I/3/3I/4/4I | `{x,y,z,w}` |
| RECT2 / AABB | `{position:{...}, size:{...}}` |
| PLANE | `{normal:{x,y,z}, d}` |
| QUATERNION | `{x,y,z,w}` |
| TRANSFORM2D / PROJECTION | `{columns:[[...]]}`（3/4 列） |
| BASIS | `{rows:[[...]]}` |
| TRANSFORM3D | `{basis:{rows}, origin}` |
| COLOR | `{r,g,b,a}` |
| STRING_NAME / NODE_PATH | 字符串 |
| RID | `{"id": N}`（**注意：并非 number**，见不一致点） |
| OBJECT | `{"class": ..., 属性...}`：跳过下划线开头/`script`/非 `PROPERTY_USAGE_STORAGE`/GROUP/SUBGROUP/CATEGORY 属性；实例 ID 环检测（路径环，允许 DAG 共享）→ `"[<circular ref>"`；深度超 32 → `"[depth exceeded]"` |
| CALLABLE / SIGNAL | `{object_id, object_id_str, method\|signal}` |
| DICTIONARY / ARRAY | object / array 递归 |
| PACKED_*_ARRAY（10 种） | 数组（byte→int；string 为字符串数组；vector2/3/color/vector4 元素为对象） |
| 其余 | `str_from_variant` 兜底 |

**deserialize 两条路径**：
- `type_hint` 非空 → `parse_type_hint`（约 50 个别名，含 snake_case 与紧凑别名如 `stringname`/`dict`/`packedbytearray`，大小写不敏感）→ `deserialize_typed`（覆盖全部值类型 + OBJECT + RID；**RID 只回空 Variant**）；hint 不是类型名时：字符串 → `ResourceLoader.load(path, type_hint)`（失败写 Resources 类别 Warning 日志）；对象 → `deserialize_as_object`（**09-13 起先识别 `__node_ref__` 并解析为编辑场景内节点引用，解析失败返回空 Variant 而非实例化游离节点**；否则 `ClassDB.instantiate` + `get_property_list` 元数据按属性类型反序列化 + 嵌套 `class` 递归，`class` 字段本身跳过）
- 无 type_hint → `deserialize_inferred`：按 JSON 值类型推断；对象特例识别 `__node_ref__`（共用 `try_deserialize_node_ref`：经 `EditorInterface` 取编辑场景根、上溯到顶再 `get_node_or_null`）、`object_id_str`（stoll 解析失败写 System 类别 Warning 日志）/`object_id`（`ObjectDB::get_instance`）；否则递归为 Dictionary

**deserialize_strict 严格形状（09-13 下午新增；09-14 增补 Vector2/2i）**：Rect2/Rect2i 的 `size` 必须为对象且提供 `w`+`h`（或别名 `x`+`y`；同轴两种拼写并存且值不同即报 conflict），`position` 可省（默认 0）；AABB 的 `size` 需 `w`+`h`+`d`（或 `x`+`y`+`z`）；Transform2D 必须提供 `columns`（≥3 列、每列 ≥2 数字）；Transform3D 必须提供 `basis.rows`（≥3 行、每行 ≥3 数字）；Vector2/Vector2i 必须为 `{x,y}` 对象，数组输入报 `invalid Vector2: expected a JSON object, e.g. {"x":0,"y":0}`（此前数组静默写 0）；其余类型透传 `deserialize`。接入方：`property_set`/`create_scene_node`（09-13 起）与 `set_resource_property`（09-14 起）。

数值读取使用宽容辅助 `as_int64/as_double`（int/double 互转，非数字回默认 0）。`COLOR` 的 `a` 缺省 1.0。

### Bm25Index（`bm25_index.cpp/hpp`）

工具检索索引（`search_tools` 元工具的后端，`ServerContext` 持有一个实例）。`add_entry(name, description, category, tags)` 将四者拼接后预分词存入 `Document`。

| 参数/要点 | 值/行为 |
|---|---|
| K1 / B | `1.5` / `0.75`（`static constexpr`） |
| tokenize | ASCII 段按 `isalnum` 切分转小写；**CJK 码点（08-24 起）逐字收集后输出 bigram**（`bm25_index.cpp`，覆盖 0x3400-0x4DBF/0x4E00-0x9FFF/0xF900-0xFAFF 三块），中文查询可用；无词干化、无停用词 |
| IDF | `ln(1 + (N − df + 0.5) / (df + 0.5))`，df 按候选集内文档去重计数 |
| 长度归一 | `avg_dl` 为全库平均 token 数（含候选外文档），下限 1.0 |
| 过滤 | `category` 相等 + `tags` 全包含（原始字符串精确匹配） |
| 特例 | 空文本 + category → 返回该类别全部条目（score 0）；查询文本压平后是名称子串 → **+2.0 加分**；仅 `score > 0` 的条目返回 |
| 截断/排序 | 按 score 降序，截断 `max_results`（默认 10） |
| 线程安全 | 内部 `mutable std::mutex` 保护 add/search/clear/size |

### error_util（`error_util.cpp/hpp`，命名空间 `godot_autopilot::util`）

| 函数 | 形态 |
|---|---|
| `error_json(msg)` | `{"error": msg}`——与 AGENTS.md "领域工具返回 `{"error": "消息"}` JSON" 一致 |
| `ok_result(value)` | `{"result": value}` |
| `error_detail(fact, position, expected, action)` | `error_json(fact + " — position: " + ... + " — expected: " + ... + " — action: " + ...)`（分隔符为 em dash `—`） |
| `to_std(String)` | Godot String → UTF-8 `std::string` |

### readback_util（`readback_util.cpp/hpp`）

`check_readback(expected_value, old_value, actual_value, out_detail, type_sensitive = false) → ReadbackStatus`：基础路径以 `VariantJson::serialize(...).Dump()` 字符串相等比较读回结果。`type_sensitive=true` 时（09-13 下午起值类型同样启用，`property_ops`/`resource_ops` 与运行时 eval `set_property` 均传入）：
- **16 类值类型**（Vector2/2i、Rect2/2i、Vector3/3i、Transform2D、Vector4/4i、Plane、Quaternion、AABB、Basis、Transform3D、Projection、Color）按分量近似比较（1e-5 相对 + 1e-6 绝对）——匹配返回 `MATCHED`；回读仍等于旧值判 `REJECTED`（"设置未生效"）；被引擎调整判 `CONVERTED`（附 warning）
- **Object/数组形态判定**——期望 OBJECT 而回读非对象、期望非空数组而回读为空、或数组元素由非 nil 变 nil 时判 `REJECTED`（`property rejected: value not applied (...)` 文案）

| 状态 | 判定 | detail |
|---|---|---|
| `REJECTED` | expected 非 nil 而 actual 为 nil；`type_sensitive` 形态未通过（对象/数组）；值类型不一致或近似比较不通过且回读等于旧值 | "property rejected: ..." / "value not applied: property stayed at old value (...)" |
| `MATCHED` | 两 dump 相等，或值类型分量近似相等 | "value matches expected" |
| `NOOP` | 通用路径下与 old_value dump 相等（值类型路径先经近似比较判 `MATCHED`） | "value already at target (default)" |
| `CONVERTED` | 其余（引擎调整了值） | "engine adjusted value: <expected> -> <actual>" |

用途：属性写入后的读回校验（`property_ops.cpp:748` 传 `type_sensitive`、`resource_ops.cpp:2853`、`runtime/game_bridge_eval.cpp:376`），区分"被拒/命中/本就在目标/引擎换算"；`property_ops` 对 REJECTED 还会尝试恢复旧值并把恢复结果写入错误文案（09-13 起）。

### scene_path.hpp（header-only，命名空间 `godot_autopilot::util`）

| 函数 | 行为 |
|---|---|
| `scene_path_hint(root)` | 无根 → `"当前没有可编辑的场景根节点"`；有根 → 输出根名及合法路径写法提示（`根名/子路径`、`/root/根名/子路径`、`子路径`） |
| `strip_leading_segment_if(path, segment)` | 若 path 恰为 segment 则清空；否则去掉 `segment/` 前缀 |
| `resolve_scene_node(path_str, scene_root, out_hint)` | 依次剥前导 `/`、`root`、根节点名三段后 `get_node_or_null`；且校验结果在 `scene_root` 子树内（parent 链上溯）；失败经 `out_hint` 输出提示并返回 nullptr |

消费方：12 个模块共 32 处调用（scene_ops 6、property_ops 7、script_ops 5、animation_ops 5 等；节点路径参数解析的统一入口）。

### json_godot.hpp（header-only，命名空间 `godot_autopilot::util`，08-22 新增）

| 函数 | 行为 |
|---|---|
| `json_number(value, fallback)` | `JsonValue*` 宽容取数（int/double 统一转 double，缺失/非数字回 fallback）——64 处外部调用（另 `json_godot.hpp` 内部复用 9 处），数值三元式的收敛点 |
| `json_to_vec2/vec3/rect2/color(j)` | JSON 对象 → Godot 几何类型（缺字段默认 0，color 的 a 缺省 1.0 由调用方处理） |
| `vec2_to_json/vec3_to_json(v)` | Godot 向量 → `{x,y[,z]}` 对象 |
| `rid_from_json/rid_to_json(…)` | RID ↔ `{"rid": <int64>}`（底层 `UtilityFunctions::rid_from_int64`） |

消费方：audio/environment/nav/physics/render/spriteframes 等 ops（数值读取与几何参数解析、RID 互转的共享实现）。

### rid_registry.hpp（header-only，命名空间 `godot_autopilot::util`，08-22 新增）

- `class RidStore`：`std::unordered_map<int64_t, godot::RID>` 句柄↔RID 双向映射并保活。
- `template <typename Domain> RidStore &rid_store()`：按域标签取独立实例（physics 与 text 各自命名空间互不串号）。
- `template <typename Domain> RID resolve_rid(args, key)`：从工具参数解析整数 id 并查表，未命中返回空 `RID`（由调用方检查 `is_valid()` 后报错，如 `physics_ops.cpp:190`）。

消费方：physics_ops、text_ops。

### type_hint.hpp（header-only，命名空间 `godot_autopilot::util`，08-22 新增）

- `infer_type_hint(dict, …)`：从参数字典推断 Variant 反序列化所需的 type_hint 字符串（供 `VariantJson::deserialize` 第二参）。
- `parse_type_hint_int(text, out)` / `struct ArrayElementHint{known, type, hint, hint_string}` / `parse_array_element_hint(prop_info)`（09-13 新增）：从属性元数据的 `hint_string`（`PROPERTY_HINT_TYPE_STRING` 形如 `type/hint:class`）解析数组元素类型/hint/类名，供 `property_ops` 的数组逐元素转换构建 typed array；无法解析返回 `std::nullopt`（`known=false` 表示类型 id 越界）。

消费方：property_ops（含数组元素转换）、resource_ops。

### gdscript_wrap.hpp（header-only，命名空间 `godot_autopilot::util`，08-22 新增）

GDScript 包装流水线共享件（script_ops 与 code_exec_ops 共用）：

| 符号 | 说明 |
|---|---|
| `MAX_CAPTURE_BYTES = 8192` | 输出截断上限常量 |
| `NODE_NOT_FOUND_HINT` | 节点路径错误提示常量（指导用 `SceneRoot.get_node(...)`） |
| `truncate_capture_text(text)` | 超 8192 字节截断 |
| `strip_extends_lines / has_top_level_func_def / defines_function_named` | 单表达式判定与 extends 剥离 |
| `IndentStyle / scan_indent_style / indent_prefix / reindent_lines` | 包装时缩进风格探测与重排 |

### project_path.hpp（header-only，命名空间 `godot_autopilot::util`，09-02 新增）

工程资源路径规范化与边界校验的单一入口：`normalize_project_path(raw, allow_user, allow_root = true)` 返回 `ProjectPath{value, error}`——反斜杠归一为 `/` 后词法消解，拒绝 `..` 路径穿越；绝对路径经 `ProjectSettings` 定位工程根做大小写不敏感前缀校验，工程外报错；scheme 仅 `res://`/`user://`（`user://` 受 `allow_user` 开关控制），未知 scheme 报错；`allow_root = false` 时拒绝命名空间根本身。失败返回结构化 `error`，不以空路径兜底。消费方：`text_ops`/`resource_ops` 全量入口（边界要求见 [../security_contract.md](../security_contract.md)）。

### mcp_image_content.hpp（header-only，命名空间 `godot_autopilot::util`，09-13 新增）

MCP 截图交付的纯逻辑件（仅依赖 `mcp::JsonValue`/`mcp::ContentVariant`）：

- `is_image_capture_tool(name)`：截图工具白名单 `capture_editor_viewport`/`capture_game_viewport`/`capture_display_screen`。
- `try_attach_image_content(tool_name, result_json, content_out, reason?)`：命中白名单且响应中顶层或 `result` 下的 `data` 为非空字符串、`format` 缺省或为 `"png"` 时，把 base64 转成 `mcp::ImageContent{"image", base64, "image/png"}` 推入 `content_out`，并把 JSON 中 `data` 改写为 `"<attached-as-image-content>"`、加 `image_attached:true`；不满足条件返回 false（可选回填 `reason`）。

消费方：`register_all.cpp` 的 `call_tool` 回调——仅元工具 `call_tool` 直调截图工具时附加 image content 块，`batch_execute`/`code_execute` 内不附加（JSON 保留完整 base64）。L1 覆盖 `tests/unit/mcp_image_content_test.cpp`。

## 与现有文档的不一致点

| 文档 | 声称 | 代码事实 | 判定 |
|---|---|---|---|
| `prompt_handlers.cpp:93`（tool-usage 描述） | "Usage examples for the **17** most commonly used MCP tools" | `prompt_tool_usage.cpp` 正文自称 **19 个**工具，实际含 19 个章节 | 文档间冲突 |
| `prompt_setup_input_map.cpp:69` | `Enter=4194310（KEY_ENTER）` | `prompt_keycode_reference.cpp:61` 表 `KEY_ENTER=4194312`、`KEY_META=4194310`（87 行） | 文档间冲突 |
| `prompt_keycode_reference.cpp` 映射表 | （原）`Rid → number`、`PackedByteArray → string (base64)` | 09-13 已修复：RID → `{"id": number}` 对象（行 159）、PackedByteArray → 数字数组（行 162），与 `variant_json.cpp` 实现一致 | 已修复 ✓ |
| `prompt_keycode_reference.cpp:57/85` | KEY_SHIFT 表格出现两行 | 分别位于特殊键表（Shift）与修饰键表（左/右 Shift），语境不同，非重复缺陷 | 已澄清 ✓ |
| AGENTS.md（架构） | "所有 Godot API 调用必须通过队列执行" | `debugger_resources` 已统一经 `CommandQueue::execute_sync()` 执行，`debugger-session` 的 Godot debugger 查询也在主线程 | 已与代码一致 |
| AGENTS.md（日志类别） | 仅 System/Transport/Tools/Resources/Prompts 五类 | `resource_handlers.cpp` 的 `category_to_string` 同五类 + `unknown` 兜底；UI 类别下拉一致 | 一致 ✓ |
| AGENTS.md（错误模式） | 领域工具返回 `{"error": "消息"}` | `error_util::error_json` 与资源侧 `set_error` 同构 | 一致 ✓ |
| AGENTS.md | `VariantJson::serialize/deserialize` 用于 Variant ↔ JsonValue 互转 | 完全一致（另含 OBJECT 环检测/深度上限/对象反序列化等扩展） | 一致 ✓ |

## 相关页面

- 入口与生命周期（ServerContext 注册顺序、`_process` 排空/轮询）：[../modules/entry_runtime.md](../modules/entry_runtime.md)
- 消费方——`search_tools`（Bm25Index）与属性/场景/资源/脚本工具（VariantJson、readback、scene_path）：[../modules/tools_ops_b.md](../modules/tools_ops_b.md)
- 基础设施（LogSystem、CommandQueue、ServerContext）：[../modules/core.md](../modules/core.md)
