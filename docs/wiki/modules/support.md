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
timestamp: "2026-08-22T15:10:00+08:00"
resource:
  - src/prompts/
  - src/resources/
  - src/ui/
  - src/util/
---

# 支撑模块（src/prompts/、src/resources/、src/ui/、src/util/）

> 审计日期：2026-08-22（2026-08-12 初稿；08-17 随配置面板新增、状态栏移除同步并补 YAML frontmatter；08-22 15 时全量一致性审计——新增 json_godot/rid_registry/type_hint/gdscript_wrap 四个 header-only util 小节、覆盖范围计数 10 组/15 文件、注册入口行号校准），基于当前工作树代码逐行核对（不依赖 git 历史）。
> 覆盖范围：`src/prompts/` 9 组文件（18 个）、`src/resources/` 2 组、`src/ui/` 2 组、`src/util/` 10 组（15 个文件，其中 `scene_path.hpp`/`json_godot.hpp`/`rid_registry.hpp`/`type_hint.hpp`/`gdscript_wrap.hpp` 为 header-only）。注册入口在 `src/core/server_context.cpp:140-143`。

## 模块简介

这四组文件不直接产生领域工具，而是为 MCP 服务器与编辑器体验提供支撑：

- **Prompts**：向 MCP 客户端暴露 12 个 prompt 模板（7 通用 + 5 调试），全部经 `server.RegisterPrompt` 注册。
- **Resources**：暴露 15 个 MCP resource URI（8 引擎侧 + 7 调试捕获侧），全部为 `godot://` 前缀。
- **UI**：底部日志面板（`McpLogDock`）、右侧配置面板（`McpConfigDock`），消费 `LogSystem` 与服务器生命周期。
- **Util**：与工具层共享的纯工具件——Variant↔JSON 互转、BM25 检索、错误 JSON 构造、写后读回校验、场景路径解析、JSON 数值/几何/RID 辅助、进程内 RID 注册表、类型提示推断、GDScript 包装流水线、客户端 MCP 配置生成。

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
| `tool-usage` | Usage examples for the 17 most commonly used MCP tools with JSON input/output and gotchas | `prompt_tool_usage()`（625 行） | 实际含 **19 个**工具章节（见"不一致点"） |
| `keycode-reference` | Godot keycode reference for InputEventKey and Variant type JSON mapping for property operations | `prompt_keycode_reference()`（213 行） | ASCII 键码表、Godot 特殊键码（`KEY_SPECIAL=4194304` 基值）、`add_input_map_action_event` 的 event JSON、Variant→JSON 映射表 |

### 调试 prompt 名单（register_debugger_prompts，可带可选参数）

| 注册名 | 描述 | 支持参数 | 正文引导使用的工具 |
|---|---|---|---|
| `debug-analyze-error` | Analyze a runtime error with full context | `error_text`（追加到文末） | `get_debugger_errors`、`get_debugger_log` |
| `debug-analyze-breakpoint` | Analyze the current breakpoint context: stack trace, scene tree, and variable state | 无 | `get_debugger_stack_dump`、`get_debugger_scene_tree`、`get_debugger_session_info` |
| `debug-review-output` | Review the game's output log for issues | `since`（追加到文末） | `get_debugger_log`、`get_debugger_output` |
| `debug-review-performance` | Review performance monitor data to identify bottlenecks | 无 | `get_debugger_monitors` |
| `debug-session-status` | Quick summary of the current debug session state | 无 | `get_debugger_session_info` 等 |

## Resources（15 个 URI，均经 RegisterResource/RegisterResourceTemplate 注册）

### 职责表

| 组件 | 文件 | 数量 | 注册方式 | MIME | 执行方式 |
|---|---|---|---|---|---|
| `register_all_resources(server, queue)` | `resource_handlers.cpp/hpp` | 8（6 静态 + 2 模板） | `godot://` URI，`application/json` | 每个 handler 经 `queue.submit()` 在主线程执行 |
| `register_debugger_resources(server)` | `debugger_resources.cpp/hpp` | 7（全静态） | `godot://` URI，`text/plain` | 直接同步调用 `tools/debugger_ops` 的 `capture_*` 文本函数（只读 `DebuggerCapture` 捕获缓冲，不触碰引擎 API，故无需 queue） |

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

`EditorDock` 子类，标题 "MCP Log"，默认停靠底部槽（`DOCK_SLOT_BOTTOM`），可关闭，最小高度 `DEFAULT_DOCK_HEIGHT = 200`：

- **布局**：VBoxContainer = `RichTextLabel`（threaded、BBCode、scroll_follow、可选择、段落上限 `LINE_LIMIT = 5000` 超限删首段）+ 底部 `HFlowContainer`（Clear 按钮、Collapse 切换按钮、搜索 `LineEdit`、类别 `OptionButton`、4 个级别过滤按钮）
- **过滤**：4 个级别按钮（Debug/Info/Warning/Error，图标 `Debug`/`Popup`/`StatusWarning`/`StatusError`，按钮文本显示各级计数）；类别下拉 All/System/Transport/Tools/Resources/Prompts；搜索为大小写不敏感子串（`findn`）；判定在 `_check_display`
- **折叠**：`collapse` 开关，按消息聚合相同文本并显示 `(N)` 次数前缀；折叠模式改动触发全量重建
- **主题**：`_update_theme()` 从编辑器主题取 `error_color`/`warning_color`/`font_color`（无则回退硬编码色）、`output_source*` 字体族、各按钮图标；`NOTIFICATION_ENTER_TREE`/`NOTIFICATION_THEME_CHANGED` 时刷新
- **日志来源**：构造时取 `LogSystem::instance()` 指针；`poll_new_entries()` 用 `query_from(last_index_)` 增量拉取（`main.cpp:_process` 每帧调用）；`refresh()`/`_rebuild_log()` 全量重建；`_on_clear()` 清显示并把 `last_index_` 推进到 `log_system->next_index()`（只清界面，不删 LogSystem 缓冲）

### McpConfigDock（`mcp_config_dock.cpp/hpp`）

`EditorDock` 子类，标题 "MCP Config"，默认停靠右侧槽（`DOCK_SLOT_RIGHT_UR`），可关闭：

- **布局**：VBoxContainer = 端口区（Label + SpinBox 1–65535 + Apply 按钮 + 运行状态 Label）→ 分隔线 → 客户端配置区（`OptionButton` 下拉选择 8 个客户端，label 含配置文件路径）+ Generate 按钮 + 结果 Label + 生效条件提示 Label
- **端口管理**：`set_server_context(ServerContext*)` 注入服务器（null 时禁用 Apply）；Apply → `ServerContext::restart(port)` → 成功后 `PluginConfig::save_port(port)` 持久化，并刷新面板内运行状态 Label（"Running on port N" / "Server offline"，主题色标注）
- **配置生成**：`_on_generate()` 只处理下拉选中的单个客户端——目标目录 `ProjectSettings::globalize_path("res://")`；文件不存在 → `render_config` 新建；JSON 已存在 → `merge_json_config` 合并（**先解析现有配置，保留其他键，仅更新 `mcp`/`mcpServers` 下的 `godot-autopilot` 条目**，不覆盖用户的其他 agent 配置）；Codex TOML 已含 `mcp_servers` → 跳过并提示；JSON 无法解析 → 跳过不写（防覆盖）；结果单文件报告「创建/更新/跳过」原因
- **主题**：颜色经 `theme_color()` 从编辑器主题取 `success_color`/`error_color`/`warning_color`/`font_disabled_color`（无则回退硬编码色）

### client_config_gen（`client_config_gen.cpp/hpp`，命名空间 `godot_autopilot::client_config_gen`）

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

## Util

### VariantJson（`variant_json.cpp/hpp`）

静态方法 `serialize(Variant) → JsonValue`、`deserialize(JsonValue, type_hint = "") → Variant`。AGENTS.md 标注的 "`VariantJson::serialize/deserialize` 用于 `godot::Variant ↔ mcp::JsonValue` 互转" 与实现一致。

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
| PACKED_*_ARRAY（9 种） | 数值数组（byte→int；vector2/3/color/vector4 元素为对象） |
| 其余 | `str_from_variant` 兜底 |

**deserialize 两条路径**：
- `type_hint` 非空 → `parse_type_hint`（约 50 个别名，含 snake_case 与紧凑别名如 `stringname`/`dict`/`packedbytearray`，大小写不敏感）→ `deserialize_typed`（覆盖全部值类型 + OBJECT + RID；**RID 只回空 Variant**）；hint 不是类型名时：字符串 → `ResourceLoader.load(path, type_hint)`（失败写 Resources 类别 Warning 日志）；对象 → `deserialize_as_object`（`ClassDB.instantiate` + `get_property_list` 元数据按属性类型反序列化 + 嵌套 `class` 递归，`class` 字段本身跳过）
- 无 type_hint → `deserialize_inferred`：按 JSON 值类型推断；对象特例识别 `__node_ref__`（经 `EditorInterface` 取编辑场景根、上溯到顶再 `get_node_or_null`）、`object_id_str`（stoll 解析失败写 System 类别 Warning 日志）/`object_id`（`ObjectDB::get_instance`）；否则递归为 Dictionary

数值读取使用宽容辅助 `as_int64/as_double`（int/double 互转，非数字回默认 0）。`COLOR` 的 `a` 缺省 1.0。

### Bm25Index（`bm25_index.cpp/hpp`）

工具检索索引（`search_tools` 元工具的后端，`ServerContext` 持有一个实例）。`add_entry(name, description, category, tags)` 将四者拼接后预分词存入 `Document`。

| 参数/要点 | 值/行为 |
|---|---|
| K1 / B | `1.5` / `0.75`（`static constexpr`） |
| tokenize | 仅 `isalnum` 字符、转小写、按非字母数字切分；**无词干化、无停用词** |
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

`check_readback(expected_value, old_value, actual_value, out_detail) → ReadbackStatus`：以 `VariantJson::serialize(...).Dump()` 的字符串相等比较读回结果。

| 状态 | 判定 | detail |
|---|---|---|
| `REJECTED` | expected 非 nil 而 actual 为 nil | "property rejected: expected <dump> but readback is nil" |
| `MATCHED` | 两 dump 相等 | "value matches expected" |
| `NOOP` | 与 old_value dump 相等 | "value already at target (default)" |
| `CONVERTED` | 其余（引擎调整了值） | "engine adjusted value: <expected> -> <actual>" |

用途：属性写入后的读回校验（`property_ops.cpp:383`、`resource_ops.cpp:1273`、`runtime/game_bridge_eval.cpp:374`），区分"被拒/命中/本就在目标/引擎换算"。

### scene_path.hpp（header-only，命名空间 `godot_autopilot::util`）

| 函数 | 行为 |
|---|---|
| `scene_path_hint(root)` | 无根 → `"当前没有可编辑的场景根节点"`；有根 → 输出根名及合法路径写法提示（`根名/子路径`、`/root/根名/子路径`、`子路径`） |
| `strip_leading_segment_if(path, segment)` | 若 path 恰为 segment 则清空；否则去掉 `segment/` 前缀 |
| `resolve_scene_node(path_str, scene_root, out_hint)` | 依次剥前导 `/`、`root`、根节点名三段后 `get_node_or_null`；且校验结果在 `scene_root` 子树内（parent 链上溯）；失败经 `out_hint` 输出提示并返回 nullptr |

消费方：`scene_ops`/`property_ops`/`script_ops`/`editor_ops` 共 15+ 处调用（节点路径参数解析的统一入口）。

### json_godot.hpp（header-only，命名空间 `godot_autopilot::util`，08-22 新增）

| 函数 | 行为 |
|---|---|
| `json_number(value, fallback)` | `JsonValue*` 宽容取数（int/double 统一转 double，缺失/非数字回 fallback）——约 80 处数值三元式的收敛点 |
| `json_to_vec2/vec3/rect2/color(j)` | JSON 对象 → Godot 几何类型（缺字段默认 0，color 的 a 缺省 1.0 由调用方处理） |
| `vec2_to_json/vec3_to_json(v)` | Godot 向量 → `{x,y[,z]}` 对象 |
| `rid_from_json/rid_to_json(…)` | RID ↔ `{"rid": <int64>}`（底层 `UtilityFunctions::rid_from_int64`） |

消费方：audio/environment/nav/physics/render/spriteframes 等 ops（数值读取与几何参数解析、RID 互转的共享实现）。

### rid_registry.hpp（header-only，命名空间 `godot_autopilot::util`，08-22 新增）

- `class RidStore`：`std::unordered_map<int64_t, godot::RID>` 句柄↔RID 双向映射并保活。
- `template <typename Domain> RidStore &rid_store()`：按域标签取独立实例（physics 与 text 各自命名空间互不串号）。
- `template <typename Domain> RID resolve_rid(args, key)`：从工具参数解析整数 id 并查表，未命中报错。

消费方：physics_ops、text_ops。

### type_hint.hpp（header-only，命名空间 `godot_autopilot::util`，08-22 新增）

- `infer_type_hint(dict, …)`：从参数字典推断 Variant 反序列化所需的 type_hint 字符串（供 `VariantJson::deserialize` 第二参）。

消费方：property_ops、resource_ops。

### gdscript_wrap.hpp（header-only，命名空间 `godot_autopilot::util`，08-22 新增）

GDScript 包装流水线共享件（script_ops 与 code_exec_ops 共用）：

| 符号 | 说明 |
|---|---|
| `MAX_CAPTURE_BYTES = 8192` | 输出截断上限常量 |
| `NODE_NOT_FOUND_HINT` | 节点路径错误提示常量（指导用 `SceneRoot.get_node(...)`） |
| `truncate_capture_text(text)` | 超 8192 字节截断 |
| `strip_extends_lines / has_top_level_func_def / defines_function_named` | 单表达式判定与 extends 剥离 |
| `IndentStyle / scan_indent_style / indent_prefix / reindent_lines` | 包装时缩进风格探测与重排 |

## 与现有文档的不一致点

| 文档 | 声称 | 代码事实 | 判定 |
|---|---|---|---|
| `prompt_handlers.cpp:93`（tool-usage 描述） | "Usage examples for the **17** most commonly used MCP tools" | `prompt_tool_usage.cpp` 正文自称 **19 个**工具，实际含 19 个章节 | 文档间冲突 |
| `prompt_setup_input_map.cpp:69` | `Enter=4194310（KEY_ENTER）` | `prompt_keycode_reference.cpp:61` 表 `KEY_ENTER=4194312`、`KEY_META=4194310`（87 行） | 文档间冲突 |
| `prompt_keycode_reference.cpp` 映射表 | `Rid → number`、`PackedByteArray → string (base64)` | `variant_json.cpp` 实现：RID → `{"id": N}` 对象；PackedByteArray → 数字数组 | 提示内容与实现不符 |
| `prompt_keycode_reference.cpp:85-86` | KEY_SHIFT 表格出现两行 | 重复行（4194307，第二行为"左/右 Shift"） | 文档瑕疵 |
| AGENTS.md（架构） | "所有 Godot API 调用必须通过 queue.submit()" | `debugger_prompts`/`debugger_resources` 不经 queue 直接同步执行——因 `capture_*` 只读 `DebuggerCapture` 捕获缓冲、不触碰引擎 API | 存在例外，描述不完整 |
| AGENTS.md（日志类别） | 仅 System/Transport/Tools/Resources/Prompts 五类 | `resource_handlers.cpp` 的 `category_to_string` 同五类 + `unknown` 兜底；UI 类别下拉一致 | 一致 ✓ |
| AGENTS.md（错误模式） | 领域工具返回 `{"error": "消息"}` | `error_util::error_json` 与资源侧 `set_error` 同构 | 一致 ✓ |
| AGENTS.md | `VariantJson::serialize/deserialize` 用于 Variant ↔ JsonValue 互转 | 完全一致（另含 OBJECT 环检测/深度上限/对象反序列化等扩展） | 一致 ✓ |

## 相关页面

- 入口与生命周期（ServerContext 注册顺序、`_process` 排空/轮询）：[../modules/entry_runtime.md](../modules/entry_runtime.md)
- 消费方——`search_tools`（Bm25Index）与属性/场景/资源/脚本工具（VariantJson、readback、scene_path）：[../modules/tools_ops_b.md](../modules/tools_ops_b.md)
- 基础设施（LogSystem、CommandQueue、ServerContext）：[../modules/core.md](../modules/core.md)
