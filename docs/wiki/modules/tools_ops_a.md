---
type: 模块文档
title: 领域工具模块（A 组）
description: 16 个领域工具模块（场景/属性/输入/物理/导航/资源/脚本/配置/文档/编辑器/编辑器 UI）实现细节
tags:
  - 模块
  - 领域工具
  - A组
timestamp: "2026-09-15T07:09:53+08:00"
resource: src/tools/
---

# 领域工具模块（src/tools/，A 组 16 模块）

> 审计日期：2026-09-15（2026-08-12 初稿；08-17 补 YAML frontmatter；08-20 随 rename 事务化 + 新工具同步；08-21 随 ToolBase 类重构同步——`tool_defs.def`/`TOOL_ENTRY` 移除，注册与计数口径改为 `<域>_tools.hpp`/`ToolRegistry`；08-22 15 时全量一致性审计——editor 计数 23、RENAME_HINTS 10 条、resolve_scene_node/RidStore/rid_from_json/NODE_NOT_FOUND_HINT 归一至 util 共享头、get_docs_class 补 enums/constants；08-29 随 0.2.2 版本与全量审计同步——336→363、35→42、174→178、26→30 域重核；09-13 上午随反馈修复批次同步——363→365、42→49、178→180，property 数组/NodeType 转换与写失败 fail fast、资源 CoW/reload_resource/copy_resource_file/标签清理、create_editor_scene timeout_ms 诊断；09-13 下午随收口批次同步——A 组 180 不变（新增 get_plugin_log 归 B 组，域工具 365→366）、严格 JSON 形状与值类型 readback、save_resource 磁盘重读 verified、ray 空间诊断字段、get_editor_settings enum 元数据；09-13 17:50 随 B 组知识库审计同步——补全 scene_ops 工具表（rename_scene_node/reparent_node）与 rename stale_references 字段口径；09-13 晚随 0.2.4 版知识库全量审计同步——修正 handle_delete 的 undo/redo 口径、input 未识别键/按键实际报错（删除"静默失败点"表述）、physics 配对计数（15 对含空间查询）与 rid_store<PhysicsRidDomain> 标签名、script handle_reload 缓存失效归属、editor save_all_scenes 直接调用与文件树截断 max_depth 字段、config get_engine_version string 字段、property_set undo 注册、group find_node 3 处与响应字段、editor 撤销四件套全名、resource 收集函数实名等代码-文档偏差；A 组 180/366 复核不变；09-14 随修复批次同步——property_set/严格形状增 Vector2/2i 对象要求与数组报错、set_resource_property 接入严格转换、get_scene_tree 的 include_properties 属性摘要修复；09-15 随 Computer Use grounding 批次同步——域工具 379 = 322 非 SIDE + 57 SIDE，A 组 191（新增 editor_ui_ops 3 / editor_ui_actions 3 / input_click_ops 4，scene +1、input +4、editor +6）、scene_ops 的 get_scene_node_screen_rect、catalog/index 387），基于当前工作树代码逐行核对（不依赖 git 历史）。
> 覆盖范围：`src/tools/` 下 16 对 `.cpp/.hpp`：scene_ops、scene_tree_ops、property_ops、group_ops、input_ops、input_map_ops、physics_ops、nav_ops、resource_ops、script_ops、config_ops、doc_ops、editor_ops、editor_ui_ops、editor_ui_actions、input_click_ops。
> 统计口径：工具数以 `src/tools/*_tools.hpp` 的 `GDA_TOOL_CLASS(`/`GDA_TOOL_CLASS_SIDE(` 声明计数为准（一个工具 = 一个 `ToolBase` 真类，`handle_` 函数与之逐一对应，两口径一致）；当前 379 个域工具 = 322 个 `GDA_TOOL_CLASS(` + 57 个 `GDA_TOOL_CLASS_SIDE(`，L2 遍历只枚举前者（322 个）。注册经 `register_all.cpp` 调用 30 组 `<域>_tools::make_tools()` 汇入单一 `ToolRegistry`。

## 模块简介

这 16 个模块是领域工具的前半部分：覆盖场景节点操作、属性/信号、分组、输入模拟与 InputMap、物理（2D/3D 双份）、导航、资源生命周期、脚本与 GDScript 执行、项目/引擎/编辑器配置、引擎类文档查询、编辑器会话管理，以及编辑器 UI 语义自动化（元素枚举/命中/点击/文本/快捷键）与合成鼠标输入。全部位于命名空间 `godot_autopilot::<模块>_ops`，与 AGENTS.md 约定一致。

30 个域 `_tools.hpp` 共声明 **379 个领域工具**（322 个常规 + 57 个以 `GDA_TOOL_CLASS_SIDE(` 标记的副作用工具），本页 16 模块占其中 **191 个（50.4%）**。`register_all.cpp` 注册 30 组 `<域>_tools::make_tools()`（另加 `system_status` 1 与 7 个元工具，catalog/index 共 387）汇入单一 `ToolRegistry`，catalog/index/`dispatch::g_handlers`/`server.RegisterTool()` 全部由其派生；领域工具不直接注册到 MCP 服务器，统一经元工具 `call_tool` 分发（见 [../modules/tools_registry.md](../modules/tools_registry.md)）。

## 模块总览

| 模块（命名空间） | handle_ 函数数 | 注册工具数 | 核心职责 |
|---|---|---|---|
| `scene_ops` | 7 | 7 | 编辑场景节点创建/删除/实例化/树遍历/节点屏幕坐标映射 |
| `scene_tree_ops` | 8 | 8 | SceneTree 层操作：组调用、暂停、定时器 |
| `property_ops` | 5 | 5 | 节点属性读写、属性列表、信号连接 |
| `group_ops` | 3 | 3 | 节点分组增删查（可撤销、持久化） |
| `input_ops` | 11 | 15（4 个新增合成输入 handler 在 input_click_ops） | Input 单例模拟按键/鼠标/手柄/动作 |
| `input_map_ops` | 8 | 8 | InputMap 运行期动作增删改查与持久化 |
| `physics_ops` | 48 | 48 | PhysicsServer 2D/3D 对象与空间查询（含挂靠 Debug 类的 `get_debug_object_info`） |
| `nav_ops` | 15 | 15 | NavigationServer 2D/3D 地图/区域/代理 |
| `resource_ops` | 26 | 26 | 资源加载/保存/创建/重载/复制/UID/导入/依赖/反查（含 09-13 新增 reload_resource/copy_resource_file） |
| `script_ops` | 10 | 10 | GDScript 执行、脚本附加/属性/调用 |
| `config_ops` | 13 | 13 | ProjectSettings/Engine/EditorSettings |
| `doc_ops` | 4 | 4 | ClassDB 类/方法/属性文档查询 |
| `editor_ops` | 23 | 29（6 个新增 UI handler 在 editor_ui_ops / editor_ui_actions） | 编辑器会话：选择/场景/撤销/播放/文件系统/C# 构建 |
| `editor_ui_ops` | 3 | —（工具注册计入 editor_ops 行） | 编辑器 UI：元素枚举、命中测试、视口几何映射（只读） |
| `editor_ui_actions` | 3 | —（工具注册计入 editor_ops 行） | 编辑器 UI 动作：按元素 path 点击、文本输入、快捷键注入（副作用） |
| `input_click_ops` | 4 | —（工具注册计入 input_ops 行） | 编辑器进程合成鼠标点击/滚轮/拖拽与焦点文本输入（副作用） |
| **合计** | **191** | **191** | |

## 公共模式

- **错误返回**：全部失败路径返回 `{"error": "消息"}`，与 AGENTS.md 描述一致；消息前缀常见 `missing required parameter: <name>`、`<X> not available`、`node not found: <path> — <hint>`。`util::error_detail`（`src/util/error_util.cpp`）是扩展形态：把 fact/position/expected/action 拼进 error 字符串（`get_scene_tree` 的无场景错误即此格式）。
- **成功返回**：`{"result": <值>}`；写场景类工具额外带 `undo` 提示字段（如 `create_scene_node` 返回 `undo: "delete node <path> (delete_scene_node)"`），部分带 `note`/`warning`/`serialization_note` 辅助字段。
- **脏标记**：scene_ops、group_ops、property_ops 修改编辑场景后调用 `scene_dirty_tracker::mark_scene_modified()`；editor_ops 不标记脏，而是在保存/新建/打开场景成功后调用 `clear_scene_modified()`（见 [../modules/core.md](../modules/core.md)）。
- **异常兜底**：`dispatch::call_handler` 对 handler 的 C++ 异常 `catch(...)` 后返回 `{"error": "internal error in tool '<name>'..."}`；导出期间（ExportGuard 置位）所有领域工具返回固定错误 `{"error":"editor is exporting; retry after export completes"}`。
- **线程**：handler 内部不直接判断线程；`dispatch::call_handler`（`dispatch.cpp`）在非主线程时经 `CommandQueue::submit().get()` 桥接，与 core.md 的线程模型一致。

## scene_ops（7 工具）

职责：编辑场景的节点生命周期与树读取。注册工具：

| 工具名 | 说明 |
|---|---|
| `create_scene_node` | 按类型创建节点（可指定 parent_path） |
| `delete_scene_node` | 删除节点（禁删根节点，可撤销） |
| `rename_scene_node` | 重命名节点（可撤销） |
| `reparent_node` | 变更节点父级（可撤销） |
| `instantiate_scene` | 实例化 PackedScene 到编辑场景 |
| `get_scene_tree` | 编辑场景树（默认深度 8 + 可选属性摘要；旧 `scene_tree_get`/`scene_get_tree` 双名合并） |
| `get_scene_node_screen_rect` | 节点 → 编辑器窗口客户区坐标映射（09-15 新增，只读） |

关键实现事实：

- `handle_create`：用 `ClassDBSingleton::is_parent_class(type, "Node")` 校验类型；无 `parent_path` 且已存在根节点时返回错误 `scene already has a root, use parent_path to add children`；新建 Node2D/Node3D 自动切换 2D/3D 主屏幕编辑器。`name`/`type` 缺省默认 `"NewNode"`/`"Node"`，**不校验必填**（AGENTS.md 契约缺口，确认属实）。
- **属性应用与异常安全（09-13 下午起）**：`properties` 经与 `property_set` 相同的严格转换链路应用；`applied_properties` 只列无 error 的属性，引擎调整值（CONVERTED）时追加 `property_warnings: [{path, property, warning}]`（无警告时字段缺席）；严格校验抛异常时删除已挂载的新节点并重新抛出（错误经 dispatch 透出完整 `ex.what()`），失败路径不再残留节点。
- `handle_delete`：根节点拒绝删除（提示用 `close_editor_scene`）；有编辑器 undo/redo 时以 action `Delete Node <name>` 执行（do: remove_child + queue_free；undo: move_child/set_owner/add_child），响应附 `undoable`、`undo` 信息对象与 note（延迟释放销毁后无法复活）；无 undo manager 时直接 `queue_free()`。
- `handle_instance`：`ResourceLoader.load` + `PackedScene.instantiate`；返回 `note` 提醒实例继承源场景的 CONNECT_PERSIST 连接。
- 树导出 `node_to_json`：属性摘要过滤 `metadata/` 前缀、`_` 开头、OBJECT 类型，单节点上限 20 个属性；09-14 修复属性名类型判定（STRING 与 STRING_NAME 均接受），`include_properties:true` 此前恒空的问题解除。
- **`get_scene_node_screen_rect`（09-15 新增，只读）**：`paths` 为 1-50 个编辑场景节点路径；`viewport` 取 `auto`（默认，按节点类型选 2D canvas 变换或 3D 视口相机）、`2d`/`3d`（强制坐标空间，类型不符按项报错）；Node3D 经 3D 编辑器视口相机投影（`behind` 标记相机背后点），Node2D/Control 经 2D 编辑器视口映射且 Control 另附变换后 `rect`；每项返回 `{path, ok, type, screen_position, rect?, behind?}`，单项错误不使整体调用失败；顶层 `mapping`（scale_x/scale_y/offset_x/offset_y）把 `capture_editor_viewport` 截图像素换算为客户区坐标。
- 路径解析复用 `util::resolve_scene_node`（`src/util/scene_path.hpp`）。

## scene_tree_ops（8 工具）

职责：`SceneTree` 级操作（运行期/编辑器通用）。注册工具：

| 工具名 | 说明 |
|---|---|
| `call_scene_tree_group` / `notify_scene_tree_group` | 对组内节点调用方法 / 发通知 |
| `create_scene_tree_timer` | 创建 SceneTreeTimer |
| `get_scene_tree_nodes_in_group` | 组内节点路径列表 |
| `is_scene_tree_paused` / `set_scene_tree_pause` | 暂停状态读写 |
| `set_scene_tree_debug_collisions_hint` | 碰撞调试绘制开关 |
| `reload_scene_tree_current_scene` | 重载当前场景 |

关键实现事实：

- `get_tree()` 解析顺序：`EditorInterface.get_edited_scene_root().get_tree()` → 回退 `Engine.get_main_loop()` cast 为 `SceneTree`；两者皆无返回 `{"error": "SceneTree not available"}`。
- `handle_call_group`：参数经 `VariantJson::deserialize` 转 Godot 数组后传 `tree->call_group(group, method, args)`；无参数时调用两参版本。
- `handle_create_timer` 返回 `SceneTreeTimer` 序列化结果；`handle_reload_current_scene` 直接返回 `godot::Error` 枚举整数值（不映射为可读字符串）。

## property_ops（5 工具）

职责：编辑场景节点属性读写与信号连接（属性层复用 `scene_path` 节点解析）。注册工具：

| 工具名 | 说明 |
|---|---|
| `property_get` / `property_set` / `property_get_list` | 属性读/写/列表 |
| `signal_connect` / `signal_disconnect` | 信号连接/断开（幂等） |

关键实现事实：

- `handle_set` 类型推导：`type_hint` 未指定时从属性字典自动推导——OBJECT 类型或 `PROPERTY_HINT_RESOURCE_TYPE` 用 `hint_string` 作为类名，否则用 Variant 类型名。
- **严格 JSON 形状（09-13 下午起）**：值转换改走 `VariantJson::deserialize_strict`（`variant_json.hpp`）——Rect2/Rect2i 的 `size` 需 `{w,h}`（或别名 `{x,y}`，同轴两拼写并存且值不同即报错），AABB 的 `size` 需 `{w,h,d}`（或 `{x,y,z}`），Transform2D 必须 `columns`（≥3 列、每列 ≥2 数字），Transform3D 必须 `basis.rows`（≥3 行、每行 ≥3 数字）；违规抛 `std::runtime_error`，经 dispatch 以 `internal error in tool 'property_set': invalid ...` 透出完整提示（此前文档形状与实现不一致会导致静默零写入）。09-14 起 Vector2/Vector2i 亦纳入严格分支：必须为 `{x,y}` 对象，数组输入报 `invalid Vector2: expected a JSON object, e.g. {"x":0,"y":0}`（此前数组静默写 0）。
- 资源赋值接 `resource_ops::try_resolve_resource_value`；**拒绝把 `memory://` 内存资源赋给节点属性**（返回错误，说明会损坏场景文件，要求先 `save_resource` 落盘）。
- **写后 undo 注册**：写入成功后把变更注册进编辑器 undo 栈（action `Set Property <property> on <path>`，do/undo 均写该属性），旧值随响应 `undo` 字段（path/property/old_value）返回；旧值或新值为 Object 引用时不注册，返回 `undoable:false` + `undo_skip_reason`（编辑器撤销栈无法可靠恢复对象引用）。
- **Node 类型属性 + 节点路径自动转引用（09-13 扩展）**：`handle_set` 检测目标属性 `type==OBJECT`、hint 为 `PROPERTY_HINT_NODE_TYPE`（引擎值 34，含 C# `[Export]` 节点字段），或 hint 为 `PROPERTY_HINT_RESOURCE_TYPE` 且 `hint_string` 指向 Node 子类时，若 `value` 为字符串（节点路径），以编辑场景根 `get_node_or_null()` 解析后赋**节点对象引用**（而非 NodePath Variant），使其保存场景时正确落入 `node_paths`、实例化后还原为节点引用。解析失败返回 `error_detail`（含当前场景根与合法路径写法指引，不再静默 ok）；成功结果带 `converted_node_path` 字段标注。判断辅助 `parse_hint_class`/`is_node_class`（容错 `Type:` 前缀、逗号 token）。
- **数组属性逐元素转换（09-13 新增）**：属性 `type==ARRAY` 时不再整体反序列化——① 非数组 JSON 值直接报错 `refusing to write (a non-array value would silently clear the array)`（传 `[]` 显式清空）；② 从属性元数据 `hint_string`（`PROPERTY_HINT_TYPE_STRING` 形如 `type/hint:class`）解析元素类型（`util::parse_array_element_hint`，`type_hint.hpp`）；Node 元素接受路径字符串或 `{"__node_ref__": "..."}`，资源元素接受 `res://`/`memory://` 字符串或 `{"path": ...}`/`{"resource": ...}`，`null` 留空槽；③ 元素类型无法确定而元素是对象/数组时拒绝写入；④ 依声明类型构建 typed array，元素不匹配时报错。无法安全表达的元素一律报错，不静默丢弃。
- 写后 readback 校验：`util::check_readback` 的 `type_sensitive` 参数（`readback_util.hpp`）在 Object/数组属性外，09-13 下午起对 16 类值类型（Vector2/2i、Rect2/2i、Vector3/3i、Transform2D、Vector4/4i、Plane、Quaternion、AABB、Basis、Transform3D、Projection、Color）同样启用——先按分量近似比较（1e-5 相对 + 1e-6 绝对）判定 MATCHED；回读值仍等于旧值判 REJECTED（"设置未生效"）；值被引擎调整判 CONVERTED（附 `warning`）。Object/数组路径保留原语义（回读为 nil、数组被清空或元素为 null 判 REJECTED，返回 `value not applied` 错误并尝试恢复旧值，恢复结果写入错误文案）。
- `property_get_list` 新增可选过滤（09-13 起）：`only_script_variables`（仅 usage 含 `PROPERTY_USAGE_SCRIPT_VARIABLE`（4096）的脚本声明变量）与 `property_filter`（属性名大小写敏感子串）。
- Camera2D 特例：`enabled`（默认值 true 不序列化）与 `current`（无 setter，需 `code_execute` 调 `make_current()`）返回 `serialization_note` 指导。
- 属性名纠错提示：Levenshtein 距离候选 + `PROPERTY_RENAME_HINTS` 重命名映射表（10 条，如 `frames→sprite_frames`、`cast_to→target_position`、`rect_position→position`、`translation→position` 等，与 AGENTS.md 中 Godot 4.x 迁移事实一致；表定义在 `property_ops.cpp:67-78`）。
- 信号连接默认 `persist=true`（CONNECT_PERSIST，随场景保存）；`signal_disconnect` 幂等（`not_connected` 不算错误）。

## group_ops（3 工具）

职责：把节点加入/移出/查询分组（Godot group，区别于 SceneTree 组调用）。注册工具：

| 工具名 | 说明 |
|---|---|
| `add_group_node` / `remove_group_node` | 增删（可撤销、持久化） |
| `has_group_node` | 查询 |

关键实现事实：

- 路径解析 `find_node`：本地薄包装，转发 `util::resolve_scene_node`（`util/scene_path.hpp`；全仓 audio_ops/group_ops/tilemap_ops 三处定义本地 `find_node`，均复用该实现）——依次剥除前导 `/`、`root`、场景根名三段，`get_node_or_null` 解析，并沿父链校验节点确实挂在编辑场景根之下（防跨场景误操作）。
- 增删均走 `EditorUndoRedoManager`（`create_action` / `add_do_method` / `add_undo_method` / `commit_action`），无 undo manager 时直接操作。
- `add_to_group(group, true)` 第二参持久化；操作后 `is_in_group` 反向校验，失败返回 `add_to_group failed: node is not in group after operation`。
- add 成功响应含 `persistent: true`；增/删成功响应均回显 `node_path`/`group`（remove 响应无 `persistent` 字段）。

## input_ops（15 工具）

职责：经 `Input` 单例向编辑器进程注入输入（动作、键盘、鼠标、手柄）；运行中游戏是独立进程，不接收这里的编辑器侧注入（游戏内输入见 B 组 `queue_game_input` 系列）。注册工具：

| 工具名 | 说明 |
|---|---|
| `press_input_action` / `release_input_action` | 动作按下（strength）/释放 |
| `is_input_action_pressed` / `is_input_action_just_pressed` | 动作状态查询 |
| `press_input_key` / `release_input_key` | 模拟按键 |
| `move_input_mouse` / `press_input_mouse_button` / `release_input_mouse_button` | 鼠标移动/按键 |
| `click_input_mouse` / `scroll_input_mouse` / `drag_input_mouse` | 一次调用完成合成点击 / 滚轮 / 拖拽（09-15 新增，副作用） |
| `type_input_text` | 向当前键盘焦点控件整体注入文本（09-15 新增，副作用） |
| `start_input_gamepad_vibration` / `stop_input_gamepad_vibration` | 手柄振动启停（原 `input_gamepad_simulate` 拆分而来） |

关键实现事实：

- `parse_key`：单字符（a-z/0-9）直接映射 Key 码；命名键表（SPACE/ENTER/ESCAPE/SHIFT/CONTROL|CTRL/ALT/TAB/BACKSPACE/DELETE/方向键）；未识别键名时 `parse_key` 返回 `KEY_NONE`，`press_input_key`/`release_input_key` 随即返回错误 `invalid key: <name>`（不静默）。
- `parse_mouse_button`：仅识别 LEFT/RIGHT/MIDDLE，其余返回 `MOUSE_BUTTON_NONE`，press/release handler 随即返回错误 `invalid mouse button: <name>`。
- 鼠标注入坐标（09-13 下午起描述明确）：`move_input_mouse`/`press_input_mouse_button`/`release_input_mouse_button` 的 `position` 为**聚焦窗口客户区**像素（编辑主窗口持有焦点时即主窗口，含 dock 区域），`(0,0)` 是该窗口左上角——不是 `get_display_mouse_position` 的桌面屏幕坐标，也不是被编辑视口坐标。驱动编辑器 dock/UI 时与 `warp_display_mouse`（同一客户区基准）配对使用：先 warp、再 move、后 press/release。
- `press_input_action` 支持可选 `strength`（默认 1.0）；移动用 `InputEventMouseMotion`，坐标从 `{x, y}` 对象解析。
- **合成输入四件套（09-15 新增，handler 在 `input_click_ops`）**：`click_input_mouse` 一次注入 warp + 移动 + 按下 + 释放（`double_click` 追加第二对；`button` left/right/middle 默认 left），`scroll_input_mouse` 需 `direction`（up/down/left/right，非法值报 `invalid scroll direction: <值>`）与 `amount`（1-10 默认 1，越界报错），`drag_input_mouse` 注入 `from`→`to` 与 `steps`（1-64 默认 8）段插值移动，`type_input_text` 向当前焦点控件推送单个文本事件（不逐键模拟）且需先聚焦（`click_input_mouse` 或 `click_editor_element`），可选 `submit` 追加回车。四者坐标基准与既有 move/press/release 一致（聚焦窗口客户区），均带可选 `observe`（追加一张编辑器视口截图），且依赖物理鼠标的 hover/拖拽效果可能要到下一帧才稳定；以 `GDA_TOOL_CLASS_SIDE(` + `ModifiesWindow` 声明，遍历自动排除。
- 全部 handler 日志级别为 Debug（区别于其他模块的 Info）。

## input_map_ops（8 工具）

职责：`InputMap` 运行期动作定义管理（不模拟按键，而是改映射本身）。注册工具：

| 工具名 | 说明 |
|---|---|
| `add_input_map_action` / `erase_input_map_action` / `has_input_map_action` / `get_input_map_actions` | 动作增删查 |
| `add_input_map_action_event` / `erase_input_map_action_event` / `set_input_map_action_deadzone` | 动作事件增删/死区 |
| `save_input_map` | 运行期改动写入 ProjectSettings 并保存到磁盘 |

关键实现事实：

- 内置 171 条 `KEY_NAME_TO_CODE` 常量表（`"KEY_A"→65` 等 Unicode/功能键码）用于把键名转成 `Key` 码。
- 事件构造基于 `InputEvent`（键盘/鼠标/手柄事件）；`action_set_deadzone` 调 `InputMap::action_set_deadzone`。
- `save_input_map` 是**持久化副作用**：把动作写入 `ProjectSettings` 的 `input/<action>` 并 `save()`；`add_input_map_action_event` 同理。二者均以 `GDA_TOOL_CLASS_SIDE(` 声明为副作用工具（`side_effect` 非空）；L2 契约遍历的解析器只匹配 `GDA_TOOL_CLASS(`，副作用工具天然不入枚举，`side_effect` 字段保底判定（与 AGENTS.md 遍历排除约定一致）。

## physics_ops（48 工具）

职责：经 `PhysicsServer2D/3D` 与 `PhysicsDirectSpaceState2D/3D` 直接操作物理世界（对象全部为 RID 句柄，非场景节点）。注册工具：

| 工具名（代表性） | 说明 |
|---|---|
| `intersect_physics_2d_ray` / `intersect_physics_3d_ray` | 射线检测（QueryParameters 参数） |
| `intersect_physics_2d_shape` / `intersect_physics_3d_shape`、`intersect_physics_2d_point` / `intersect_physics_3d_point` | 空间查询两对（原 shape_cast/point_query 四工具删除后合并于此） |
| `create_physics_2d_body` / `create_physics_3d_body`、`create_physics_2d_area` / `create_physics_3d_area`、`create_physics_2d_joint` / `create_physics_3d_joint` | 物理对象创建 |
| `create_physics_2d_circle_shape` / `create_physics_3d_sphere_shape`（+ `set_physics_2d_shape_data` / `set_physics_3d_shape_data`） | 形状 RID 创建 |
| `apply_physics_3d_body_force` / `apply_physics_3d_body_impulse` / `apply_physics_3d_body_torque`、`set_physics_3d_body_mode` / `get_physics_3d_body_state` / `set_physics_3d_body_state`、`add_physics_3d_body_shape`、`set_physics_3d_body_axis_lock`、`add_physics_3d_body_collision_exception` / `remove_physics_3d_body_collision_exception`、`set_physics_3d_body_param` | 3D 刚体控制 |
| `set_physics_3d_area_param` / `set_physics_3d_area_monitorable` / `set_physics_3d_area_space` / `set_physics_3d_area_transform` | 3D 区域控制 |
| `set_physics_3d_space_solver_iterations` / `set_physics_3d_space_solver_params` / `set_physics_3d_space_param`、`get_physics_2d_space_direct_state` / `get_physics_3d_space_direct_state` | 空间级操作 |
| `create_physics_3d_soft_body` / `set_physics_3d_soft_body_mesh` | 软体 |
| `get_physics_node_rid` / `get_debug_object_info` | 节点 ↔ 物理 RID 解析 / 对象 ID → 类名、名称、节点路径或资源路径（挂靠本模块，归 Debug 类） |

关键实现事实：

- **`intersect_physics_2d_ray` 空间诊断（09-13 下午起）**：省略 `space_rid` 时自动解析编辑场景的 World2D 空间（编辑场景根的 SubViewport），响应新增 `space_rid`（int64）与 `auto_detected` 字段，命中、未命中与 `space_get_direct_state returned null` 三种出口均携带。编辑器进程的物理世界与运行中游戏的物理世界相互独立——同一坐标在游戏内命中，在编辑器查询中可能为空；工具描述与技能文档已写明该边界。
- **RID 生命周期**：`RidStore`（`std::unordered_map<int64_t, godot::RID>`，定义于 `util/rid_registry.hpp` 的 `rid_store<PhysicsRidDomain>()` 模板标签实例——跨域共享，text_ops 复用 `rid_store<TextRidDomain>()`）把 int64 句柄↔RID 双向映射并保活；跨请求传参一律用整数 id。注意 `create_physics_2d_circle_shape` 的工具描述明确提示：返回的是 `PhysicsServer2D` RID 句柄，**不可直接赋给 `CollisionShape2D.shape`**（其类型为 `Shape2D` 资源），需要挂载时用资源类工具或 `code_execute`。
- 2D/3D 成对命名（`*_physics_2d_*` / `*_physics_3d_*`）共 15 对（含 3 对空间查询 `intersect_*`），另有 16 个 3D 专属工具与 2 个通用工具（`get_physics_node_rid`/`get_debug_object_info`），是命名最整齐的模块；`get_debug_object_info` 自 `resolve_object` 迁移而来（不再无前缀）。
- 3D 空间查询参数类：`PhysicsRayQueryParameters3D` / `PhysicsShapeQueryParameters3D` / `PhysicsPointQueryParameters3D`（2D 同构）。
- 变换参数解析：`parse_vec2/vec3`（`{x,y[,z]}` 对象）、`parse_t2d`（origin/rotation/scale）、`parse_basis`（3×3 数组），数值缺省为 0。

## nav_ops（15 工具）

职责：经 `NavigationServer2D/3D` 单例做导航数据与寻路（同样 RID 句柄体系）。注册工具：

| 工具名 | 说明 |
|---|---|
| `create_nav_2d_map` / `create_nav_3d_map`（+ `set_nav_3d_map_cell_size`） | 导航地图创建/配置 |
| `create_nav_2d_region` / `create_nav_3d_region`（+ `set_nav_3d_region_navigation_mesh`） | 导航区域 |
| `get_nav_2d_map_path` / `get_nav_3d_map_path` / `get_nav_3d_map_closest_point_to_segment` | 寻路查询 |
| `create_nav_2d_agent` / `set_nav_2d_agent_velocity`、`create_nav_3d_agent` / `set_nav_3d_agent_velocity` / `get_nav_3d_agent_state` | 代理 |
| `create_nav_3d_obstacle` | 障碍 |

关键实现事实：

- RID 跨 JSON 传递经 `util/json_godot.hpp` 的 `rid_to_json`/`rid_from_json` 封装（序列化形态为 `{"rid": <int64>}`，底层仍是 `UtilityFunctions::rid_from_int64`），与 physics_ops 的进程内 RidStore 风格不同。
- 地图创建默认不激活（`map_set_active` 需显式传 `active: true`）；区域创建支持 `enabled`/`navigation_layers` 初始参数。
- `set_nav_3d_region_navigation_mesh` 接受 `NavigationMesh` 资源（可经 `ResourceLoader` 加载路径传入）；路径返回点为 Vector2/Vector3 数组序列化（`{x,y[,z]}`）。

## resource_ops（26 工具）

职责：资源生命周期管理——加载/保存/重载/创建/复制/UID/文件操作/依赖/反查/导入，是资源类工具（Resources 类别）的完整实现。注册工具：

| 工具名 | 说明 |
|---|---|
| `load_resource` / `reload_resource` / `save_resource` / `create_resource` / `duplicate_resource` / `copy_resource_file` | 加载/强制重载缓存/保存/创建/内存复制/文件拷贝 |
| `load_resource_threaded` / `get_resource_load_threaded_status` / `get_resource_load_threaded` | 线程化加载三件套 |
| `get_resource_type` / `has_resource` / `get_resource_types` / `get_resource_extensions` | 类型与存在性 |
| `get_resource_dir_files` / `get_resource_uid` / `set_resource_uid` / `remove_resource_file` / `rename_resource_file` / `move_resource_file` / `create_directory` | 文件系统操作 |
| `get_resource_dependencies` / `has_resource_dependency` / `get_resource_references` | 依赖正向查询 / 反查（谁引用了某资源） |
| `reimport_resource_files` | 编辑器导入（原 `resource_import` 已删除） |
| `get_resource_property` / `set_resource_property` | 内存资源属性 |

关键实现事实：

- **三路解析**（`resolve_resource`）：`object_id_str`/`object_id` → `ObjectDB` + `resource_registry`（`core/resource_registry.cpp` 的 `name:<name>` 与 oid 数字键双键内存缓存）→ `path` 兜底 `ResourceLoader.load`；`path` 加载成功会把实例注册进 `resource_registry`（09-13 起），使后续调用经 `object_id`/`name:` 定位到同一实例、跨调用修改存活。磁盘加载路径返回 `warning`：磁盘副本**不含**内存中的未保存修改。
- 序列化统一返回 `class/path/object_id/object_id_str/name` 五字段（`serialize_ref`）。
- **`save_resource` copy-on-write（09-13 起）**：`path` 为定位路径与默认目标，`dest_path≠path` 且资源经 `path` 定位时，先 `duplicate(true)` 深拷贝再写盘——源缓存实例不被改写；响应增 `copy_on_write: true` 与 `source_path`，且验证加载时排除了"回读到源实例"的假阳性。`verified` 字段自 09-13 下午起改用 `CACHE_MODE_IGNORE` 从磁盘重读判定（绕过缓存，校验的是落盘内容）；Windows 安全保存会短暂替换文件，并发读者可能瞬时打开失败，工具描述已注明重试即可。
- **`set_resource_property` readback（09-13 下午起）**：写后校验启用 `type_sensitive`（值类型走近似比较）——引擎调整值返回 `warning`、设置未生效（回读等于旧值）返回 REJECTED；与运行时 eval `set_property` 路径一致。值转换自 09-14 起同样走 `VariantJson::deserialize_strict`（严格形状同 `property_set`，含 Vector2/2i 数组输入报错）。
- **`reload_resource`（09-13 新增）**：对 `path` 以 `CACHE_MODE_REPLACE` 强制重新加载，替换编辑器缓存实例；响应 `{result:"reloaded", path, replaced:true, reused_cached_instance}`——`replaced` 表示缓存已随 reload 刷新（当前实现恒为 true），`reused_cached_instance:true` 表示引擎复用了原实例而非新建。文件缺失/无法加载时报错。
- `handle_reimport` 仅编辑器模式可用（`is_editor_hint` 校验）；空参时 `count=0` 返回 `reimport queued for 0 file(s)` 静默成功（AGENTS.md 契约缺口，确认属实）。**reimport 只对带 `.import` 伴生的文件有效**：对 `.tres/.gd` 等直接文本文件引擎会记 BUG 且无导入效果，工具仍报 queued——刷新这类文件用 `reload_resource`。
- `handle_get_extensions`：缺 `type` 时调 `get_recognized_extensions_for_type("")` 返回全类型（AGENTS.md 契约缺口，确认属实）。
- `remove_resource_file` 有磁盘副作用；`set_resource_uid` 修改 `.uid` 文件。
- **创建/复制支持脚本全局类（09-13 起）**：`create_resource` 的 `type` 接受 ClassDB 引擎类（原有）与全局脚本类（GDScript `class_name` / C# `[GlobalClass]`，经 `ProjectSettings.get_global_class_list()` 定位脚本，脚本不可实例化或基类非 Resource 时报错）；`get_resource_types` 列表同样并入可实例化的全局脚本类（排序后追加）。
- **`duplicate_resource` 新增可选 `name`（09-13 起）**：提供时设置资源名；无论是否提供都会注册进 `resource_registry`（oid 键保活副本，提供 `name` 时另有 name 键，可经 `memory://name` 再次定位）。
- **`copy_resource_file`（09-13 新增）**：`path`→`dest_path` 逐字节复制（两者必填，父目录递归创建、目标已存在则覆盖），随后 `update_file` + 编辑器文件系统扫描；**不复制 `.import`/`.uid` sidecar、不改写引用**（导入资产需 reimport），响应 `{result:"copied", from, to}`。需要保留引用关系时改用 rename/move。
- **`rename_resource_file` 事务化（08-20 起）**：改名不再只是 Move+索引重建——① 先搬运伴生 `.uid` 文件（保 uid 不重生成，P0-2 修复）；② rename 前扫描 res:// 全部 `.tscn/.tres` 收集依赖，rename 后对命中 `path="<旧>"` 的依赖做文本回写（P0-1 修复），写前关闭读句柄、写后 flush/close 并回读验证，验证不过入 `stale_references` 而非乐观上报；③ 返回结构化影响报告 `{result, updated_files:[{file,changes}], stale_references:[{file,token}], uid_preserved}`（P2-1；`token` 为残留引用字面量，二进制资源另见 `unsupported_binary_references:[{file,reason}]`）；④ `script_class` 因无法可靠确认全局类名（曾依赖 `ResourceLoader.load` 重入编辑器文件系统，有崩溃风险）一律列入 `stale_references` 由调用方人工确认。**目录参数 fail fast（09-13 起）**：`path` 是目录时报"directory rename not supported"并指引改用 `move_resource_file`（目录改名不会改写依赖引用）。`get_resource_references` 复用同一扫描器反查引用（path/uid/class_name 三选一）。
- **目录 move 排除 sidecar（09-13 起）**：`move_resource_file` 目录遍历时跳过 `.uid`/`.import` sidecar，主文件各自走引用改写事务、sidecar 随主文件迁移再统一清理，避免 sidecar 被当独立文件重复搬运。
- **移动/删除后自动关闭旧路径标签（09-13 起）**：rename/move/remove 后仍指向旧路径的已打开场景标签会被自动关闭，响应附 `closed_tabs`；当前正在编辑的标签或无法关闭的标签留开并在 `closed_tabs_warning` 中说明；`remove_resource_file` dry-run 另在 `open_tabs` 中预告受影响标签。
- 工程约束：res:// 下文本/文件递归收集（`collect_matching_files(_budget)`/`collect_all_file_paths(_budget)`）统一用 `join_path` 拼接（`res://` 根免产生 `res:///` 三重斜杠，否则 WRITE 提交会失败并残留 `.tmp`）。

## script_ops（10 工具）

职责：GDScript 生命周期与执行（与元工具 `code_execute` 并列的另一执行通道，`code_execute` 本体在 `code_exec_ops.cpp`）。注册工具：

| 工具名 | 说明 |
|---|---|
| `execute_script` | 执行 GDScript（单表达式自动返回） |
| `load_script` / `create_script` / `reload_script` | 脚本资源加载/创建/重载 |
| `attach_script_to_node` / `detach_script_from_node` | 附加/分离 |
| `get_script_property` / `set_script_property` / `call_script_node` | 属性与方法调用 |
| `get_script_property_list` | 变量枚举 |

关键实现事实：

- `handle_execute_gdscript`：`is_single_expression` 判定（无换行、非纯赋值、首词不在 18 个关键字黑名单如 if/for/func/return/var/await…）时自动把表达式值作为返回值；多行代码需显式 `return`；输出经 `truncate_capture_text` 截断至 `MAX_CAPTURE_BYTES = 8192` 字节（两常量均定义于共享头 `util/gdscript_wrap.hpp`，与 code_exec_ops 共用）。
- 节点路径提示常量：执行节点挂在 `/root` 下而非编辑场景内，`NODE_NOT_FOUND_HINT`（`util/gdscript_wrap.hpp`）指导用 `SceneRoot.get_node(...)` 访问编辑场景节点。
- `handle_reload`：加载脚本后调 `script->reload(keep_state)`（`keep_state` 默认 false）并返回 Error 整数码；`invalidate_cached_resource`（缓存引用路径置空）只在 `create_script` 路径使用（编译失败与保存成功后各调一次）。
- 资源序列化 `serialize_resource` 返回 `class/path/object_id/object_id_str`（无 name）。

## config_ops（13 工具）

职责：项目/引擎/编辑器三层配置读写。注册工具：

| 工具名 | 说明 |
|---|---|
| `get_project_settings` / `set_project_settings` / `has_project_settings` / `save_project_settings` | ProjectSettings |
| `get_engine_version` / `get_engine_fps` / `get_engine_frames_drawn` / `set_engine_time_scale` / `get_engine_time_scale` / `set_engine_max_fps` | Engine |
| `get_editor_settings` / `set_editor_settings` / `has_editor_settings` | EditorSettings |

关键实现事实：

- `set_project_settings` 的值类型推断：显式 `{"type": ..., "value": ...}` 包裹优先；int/float/bool 原生类型直转；字符串时先查已有设置的 Variant 类型再按该类型反序列化（`VariantJson::deserialize(vp, type_name)`），避免设置被存成错误类型。
- **`get_editor_settings` 元数据（09-13 下午起）**：除 `result`（原始值）外附带属性列表元数据——`hint`（整数 PropertyHint）与非空的 `hint_string`；枚举设置（hint 2）另含 `enum_options: [{label, value}]` 解码（如 `run/window_placement/game_embed_mode` 的裸整数），无需凭猜测解读。
- `get_engine_version` 返回 Engine 版本信息对象（`{major, minor, patch, string}`，`string` 为完整版本标签）；`set_time_scale`/`set_max_fps` 直接写 `Engine` 单例。
- `save_project_settings` 与 `set_editor_settings` 均写磁盘（持久化副作用），以 `GDA_TOOL_CLASS_SIDE(` 声明（`side_effect` 非空），遍历自动排除。
- 命名前缀三套并存（`project_settings_*`、`engine_*`、`editor_settings_*`），与文件名 `config_` 不一致（见下节）。

## doc_ops（4 工具）

职责：经 `ClassDBSingleton` 查询引擎类元数据（文档检索，非运行对象）。注册工具：

| 工具名 | 说明 |
|---|---|
| `get_docs_class` | 完整类信息（父类/API 类型/可实例化/方法/属性/信号/常量） |
| `find_docs_class` | 类名搜索 |
| `get_docs_method` / `get_docs_property` | 按类查方法/属性条目 |

关键实现事实：

- 全部基于 `ClassDBSingleton`：`class_exists` / `get_parent_class` / `class_get_api_type` / `can_instantiate` / `class_get_method_list` / `class_get_property_list` / `class_get_signal_list`。
- 自带 `variant_to_json` 序列化（BOOL/INT/FLOAT/STRING/字典/数组/打包数组，其余 `stringify()` 兜底），未复用 `VariantJson`。
- `get_docs_class` 返回 `name/parent_class/api_type/can_instantiate/methods/properties/signals` 结构，另含 `enums`/`constants` 两数组与 `note`（无 docstring 时提示）。

## editor_ops（23 工具）

职责：Godot 编辑器会话操作（EditorInterface/EditorSelection/EditorUndoRedoManager/EditorFileSystem）。注册工具：

| 工具名 | 说明 |
|---|---|
| `get_editor_selection` / `set_editor_selection` / `get_editor_edited_scene_root` | 选择与编辑根 |
| `save_editor_scene` / `save_editor_scene_as` / `save_editor_scenes` / `create_editor_scene` / `open_editor_scene` / `close_editor_scene` / `reload_editor_scene` | 场景生命周期（原 `editor_new_text_resource` 已删除） |
| `create_editor_undo_redo_action` / `commit_editor_undo_redo` / `add_editor_undo_redo_do` / `add_editor_undo_redo_undo` | 撤销栈四件套 |
| `get_editor_file_system_tree` / `scan_editor_file_system` / `get_editor_file_system_status` | 文件系统（原 `editor_import_resource` 已删除） |
| `play_editor_current_scene` / `stop_editor_playing` | 播放 |
| `set_editor_main_scene` / `inspect_editor_resource` / `set_editor_plugin_enabled` | 杂项（原 `editor_get_plugin_list` 已删除） |
| `build_csharp_assembly` | C# 工程编译（`dotnet build` 子进程，见下） |

关键实现事实：

- `handle_save_all_scenes` 直接调 `editor->save_all_scenes()`；`get_unsaved_scenes` 经 `editor->call()` 反射获取（godot-cpp 无该 API 绑定），用于 create/open/close 的未保存检测与 `editor_scene_state_json` 诊断。
- **`create_editor_scene` 可靠性（09-13 起）**：新增 `timeout_ms`（默认 `GDA_NEW_SCENE_SWITCH_WAIT_MS=2000`，允许 50-30000，非整数或越界直接报错）；超时失败时释放未被编辑器接管的临时根节点（`node_released`），并返回 `waited_ms`/`timeout_ms`/`node_released`/`editor_state`（`edited_root`/`unsaved_scenes`/`file_system{scanning,progress}`）诊断；`close_current` 各失败路径先释放临时节点再报错。
- **`save_editor_scene` 失败返回 `error` 字段**（含 error code 与路径，不再只回原始枚举值）；**`save_editor_scene_as` 成功返回 `{result:"saved"}`** 并附 `tab_path_registered`/`tab_rebuilt`——Godot 4.7 的 `EditorInterface::save_scene_as` 不更新编辑器内部标签路径，插件在路径未注册时 close+reopen 重建标签以保持路径一致，重建失败附 `warning`（09-13 起）。
- **`reload_editor_scene` 未打开的场景返回 error**（"scene is not open"）而非假成功；成功返回 `{result:"reloaded", scene_path, observed?}`（`observed:true` 表示已确认编辑根路径匹配）（09-13 起）。
- **`open_editor_scene` 的 unsaved 错误补恢复指引**：除"先保存"外，提示可对列出的路径调 `reload_editor_scene` 丢弃改动后重试（09-13 起）。
- `handle_play_current_scene`：`is_playing_scene()` 防重（返回 `already_playing`），play 后再次校验，失败报 `failed to start scene playback: no game process started`；与 `entry_runtime`（game bridge）配合（见 [../modules/entry_runtime.md](../modules/entry_runtime.md)）。
- 文件系统树导出 `dir_to_json` 递归上限 `MAX_TREE_DEPTH=12`，截断时响应附 `max_depth: 12`（`dir_to_json` 的局部 truncated 返回值不直接进入响应）。
- `set_editor_main_scene` 写 `application/run/main_scene` 并 `ProjectSettings::save()`；`set_editor_plugin_enabled`、`save_editor_scene` 等均以 `GDA_TOOL_CLASS_SIDE(` 声明（`side_effect` 非空），遍历自动排除。
- `create_editor_undo_redo_action` 支持动作分组名称，`add_do`/`add_undo` 以 node_path + 方法 + 参数形式入栈。
- **`build_csharp_assembly`（08-20 新增）**：在 res:// 根定位 `.csproj/.sln`（无则报非 C# 工程），经 `OS::create_process` 异步启动 `dotnet build --nologo <project>`（非阻塞，沿用 os_ops 模式，避免冻结编辑器），返回 `{project_file, command, started, pid, note}`；`started` 为假时附 `error` 说明 SDK 缺失。`note` 明示能力边界：GDExtension(C++) 无公共 API 触发编辑器内 C# 程序集热重载（该能力在引擎 `modules/mono` 内部），仅能在 CI/命令行式编译验证；编辑器内类/签名变更后的验证仍须用户在编辑器点 Build 或重启。以 `GDA_TOOL_CLASS_SIDE(` 声明（`side_effect` 非空），遍历自动排除。

## editor_ui_ops（3 工具，只读）

职责：编辑器 UI 语义查询（09-15 新增，`src/tools/editor_ui_ops.cpp/hpp`）——为 UI 自动化提供元素清单、命中链与视口几何映射。注册工具：

| 工具名 | 说明 |
|---|---|
| `get_editor_ui_elements` | 枚举编辑器 UI 控件：语义 path / 类型 / 文本 / tooltip / name / placeholder / enabled / 客户区矩形 / window_id（`query` 子串匹配 path/name/text/tooltip/placeholder、`type_filter` 精确类名、`interactive_only` 只保留可交互控件、`max_elements` 1-1000 默认 100；超限 `truncated:true`） |
| `hit_test_editor_point` | 返回客户区 `position` 下的控件链（最上层优先）；`max_results` 1-64 默认 10；命中规则近似引擎实现（clipping / mouse_filter / visibility） |
| `get_editor_viewport_geometry` | 视口图像 → 窗口客户区映射 `window = offset + image * scale`；`viewport` 取 2d（默认）/3d，`index` 选第 3D 视口（0-3）；2D 另含 canvas origin 与 zoom |

关键实现事实：

- 三者均以 `GDA_TOOL_CLASS(` 声明（只读），不进入副作用排除；`window_id` 默认 0（编辑器主窗口），支持子窗口元素。
- 元素 path 是 `click_editor_element` / `type_editor_element_text` 的输入；`hit_test_editor_point` 可从截图坐标预览命中的控件。
- `get_editor_viewport_geometry` 的 `mapping` 与 `get_scene_node_screen_rect` 顶层 `mapping` 同构（`window = offset + image * scale`），用于把 `capture_editor_viewport` 截图像素换算为客户区坐标。

## editor_ui_actions（3 工具，副作用）

职责：编辑器 UI 动作注入（09-15 新增，`src/tools/editor_ui_actions.cpp/hpp`）——按元素 path 或快捷键驱动编辑器 UI。注册工具：

| 工具名 | 说明 |
|---|---|
| `click_editor_element` | 按元素 path 一次注入 warp + 移动 + 按下 + 释放（`double_click` 追加第二对；`button` left/right/middle 默认 left；`warp` 默认 true，hover 敏感控件可关闭；可选 `observe`） |
| `type_editor_element_text` | 聚焦元素（未聚焦时 `grab_focus`）并推送单个文本事件（非逐键模拟）；可选 `submit` 追加回车、`observe` 追加截图 |
| `run_editor_shortcut` | 解析并注入编辑器快捷键：`+` 分隔的修饰键（ctrl/control、shift、alt、meta/super，大小写不敏感）+ 主键（与输入工具同名键，另接受 F1-F12）；非法格式/未知键报错 |

关键实现事实：

- 三者均以 `GDA_TOOL_CLASS_SIDE(` + `ModifiesWindow` 声明，遍历自动排除；仅作用于编辑器进程，运行中游戏是独立进程收不到。
- 元素 path 来自 `get_editor_ui_elements` 或 `hit_test_editor_point`；比原始坐标 `click_input_mouse` 更稳，后者用于 2D canvas / 3D 视口 / 自绘控件。
- 点击的 hover/拖拽效果可能到下一帧才稳定，建议用 `observe` 或 `capture_editor_viewport` 复验；本批未新增授权能力门（`ModifiesWindow` 不在 `code_execute`/`game_runtime`/`process` 能力集）。

## 与现有文档的不一致点

| 文档 | 声称 | 代码事实 | 判定 |
|---|---|---|---|
| AGENTS.md（命名约定） | 工具命名 `<动词>_<类别>_<维度>_<对象>_<修饰>`，动词置首，如 `intersect_physics_2d_ray` | 本组符合；`signal_connect`/`signal_disconnect`（动词置首，无类别段）、config_ops 的 `get_project_settings`/`set_engine_*`/`get_editor_settings`（类别段为 project/engine/editor，与文件名 `config_` 不一致）属规范内的长短变化 | 一致 ✓（动词置首） |
| AGENTS.md（工具总数） | 379 领域工具 | 30 个 `*_tools.hpp` 恰为 379 个 `GDA_TOOL_CLASS(`/`GDA_TOOL_CLASS_SIDE(`；本页 16 模块 191 个 | 一致 ✓ |
| AGENTS.md（错误模式） | 领域工具返回 `{"error": "消息"}` | 一致；另有 `error_detail` 扩展格式与 dispatch `catch(...)` 兜底、导出期固定错误 | 一致 ✓（有扩展） |
| AGENTS.md（契约缺口 3 项） | `create_scene_node` 不校验必填；`get_resource_extensions` 缺 type 返回全类型；`reimport_resource_files` 空参静默成功 | 三项均在代码中逐一确认（默认 "NewNode"/"Node"；空 type 传 `""`；count=0 返回 queued） | 一致 ✓ |
| AGENTS.md（遍历排除） | 57 个副作用工具排除 | 57 个副作用工具以 `GDA_TOOL_CLASS_SIDE(` 声明并经 `ISideEffect` 暴露 `side_effect`；L2 遍历解析器仅匹配 `GDA_TOOL_CLASS(`（322 个入枚举），经 `get_tool_detail` 返回的 `tool.side_effect` 非空再兜底排除，不再硬编码清单 | 一致 ✓ |
| 命名归属 | 类别前缀应反映模块 | `get_scene_tree` 注册在 scene_ops（非 scene_tree_ops）——旧 `scene_tree_get`/`scene_get_tree` 双名已合并，不再语义重叠 | 已消解 ✓ |
| AGENTS.md（架构） | 工具经 `call_tool` 代理、`register_all.cpp` 的 `g_handlers` 映射分发 | 工具由 `<域>_tools::make_tools()` 汇入 `ToolRegistry`，`register_all.cpp` 由 registry 派生填充 `g_handlers` 与 catalog；`dispatch.cpp` 非主线程走 `queue.submit()` | 一致 ✓ |
| `input_ops` | （无文档声明） | `parse_key`/`parse_mouse_button` 对未识别名称返回 `KEY_NONE`/`MOUSE_BUTTON_NONE`，但 `press/release_input_key`、`press/release_input_mouse_button` 随即返回 `invalid key`/`invalid mouse button` 错误 | 一致 ✓（本页已注明，非静默失败） |

## 相关页面

- 注册管线与分类：[../modules/tools_registry.md](../modules/tools_registry.md)
- 基础设施与线程模型：[../modules/core.md](../modules/core.md)
- 运行时桥接：[../modules/entry_runtime.md](../modules/entry_runtime.md)
- 入口：[../index.md](../index.md) ｜ 总览：[../overview.md](../overview.md) ｜ 测试：[../tests.md](../tests.md)
