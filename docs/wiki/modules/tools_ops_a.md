---
type: 模块文档
title: 领域工具模块（A 组）
description: 13 个领域工具模块（场景/属性/输入/物理/导航/资源/脚本/配置/文档/编辑器）实现细节
tags:
  - 模块
  - 领域工具
  - A组
timestamp: "2026-08-17T01:03:27+08:00"
resource: src/tools/
---

# 领域工具模块（src/tools/，A 组 13 模块）

> 审计日期：2026-08-17（2026-08-12 初稿；08-17 补 YAML frontmatter），基于当前工作树代码逐行核对（不依赖 git 历史）。
> 覆盖范围：`src/tools/` 下 13 对 `.cpp/.hpp`：scene_ops、scene_tree_ops、property_ops、group_ops、input_ops、input_map_ops、physics_ops、nav_ops、resource_ops、script_ops、config_ops、doc_ops、editor_ops。
> 统计口径：`handle_` 函数数取自 `.cpp` 定义；注册工具数取自 `tool_defs.def` 的 `TOOL_ENTRY` 行（以 handler 所属模块分组），两口径全部一致。

## 模块简介

这 13 个模块是领域工具的前半部分：覆盖场景节点操作、属性/信号、分组、输入模拟与 InputMap、物理（2D/3D 双份）、导航、资源生命周期、脚本与 GDScript 执行、项目/引擎/编辑器配置、引擎类文档查询、编辑器会话管理。全部位于命名空间 `godot_autopilot::<模块>_ops`，与 AGENTS.md 约定一致。

`tool_defs.def` 共 332 个 `TOOL_ENTRY`（即 AGENTS.md 所述 332 个领域工具），本页 13 模块占其中 **172 个（51.8%）**。工具经 `register_all.cpp` 的 `TOOL_ENTRY` 宏同时写入 `dispatch::g_handlers` 与 `ToolCatalog`；领域工具不直接注册到 MCP 服务器，统一经元工具 `call_tool` 分发（见 [../modules/tools_registry.md](../modules/tools_registry.md)）。

## 模块总览

| 模块（命名空间） | handle_ 函数数 | 注册工具数 | 核心职责 |
|---|---|---|---|
| `scene_ops` | 4 | 4 | 编辑场景节点创建/删除/实例化/树遍历 |
| `scene_tree_ops` | 8 | 8 | SceneTree 层操作：组调用、暂停、定时器 |
| `property_ops` | 5 | 5 | 节点属性读写、属性列表、信号连接 |
| `group_ops` | 3 | 3 | 节点分组增删查（可撤销、持久化） |
| `input_ops` | 11 | 11 | Input 单例模拟按键/鼠标/手柄/动作 |
| `input_map_ops` | 8 | 8 | InputMap 运行期动作增删改查与持久化 |
| `physics_ops` | 48 | 48 | PhysicsServer 2D/3D 对象与空间查询（含挂靠 Debug 类的 `get_debug_object_info`） |
| `nav_ops` | 15 | 15 | NavigationServer 2D/3D 地图/区域/代理 |
| `resource_ops` | 21 | 21 | 资源加载/保存/创建/UID/导入/依赖 |
| `script_ops` | 10 | 10 | GDScript 执行、脚本附加/属性/调用 |
| `config_ops` | 13 | 13 | ProjectSettings/Engine/EditorSettings |
| `doc_ops` | 4 | 4 | ClassDB 类/方法/属性文档查询 |
| `editor_ops` | 22 | 22 | 编辑器会话：选择/场景/撤销/播放/文件系统 |
| **合计** | **172** | **172** | |

## 公共模式

- **错误返回**：全部失败路径返回 `{"error": "消息"}`，与 AGENTS.md 描述一致；消息前缀常见 `missing required parameter: <name>`、`<X> not available`、`node not found: <path> — <hint>`。`util::error_detail`（`src/util/error_util.cpp`）是扩展形态：把 fact/position/expected/action 拼进 error 字符串（`get_scene_tree` 的无场景错误即此格式）。
- **成功返回**：`{"result": <值>}`；写场景类工具额外带 `undo` 提示字段（如 `create_scene_node` 返回 `undo: "delete node <path> (delete_scene_node)"`），部分带 `note`/`warning`/`serialization_note` 辅助字段。
- **脏标记**：修改编辑场景的模块（scene_ops、group_ops、property_ops、editor_ops）调用 `scene_dirty_tracker::mark_scene_modified()`（见 [../modules/core.md](../modules/core.md)）。
- **异常兜底**：`dispatch::call_handler` 对 handler 的 C++ 异常 `catch(...)` 后返回 `{"error": "internal error in tool '<name>'..."}`；导出期间（ExportGuard 置位）所有领域工具返回固定错误 `{"error":"editor is exporting; retry after export completes"}`。
- **线程**：handler 内部不直接判断线程；`dispatch::call_handler`（`dispatch.cpp`）在非主线程时经 `CommandQueue::submit().get()` 桥接，与 core.md 的线程模型一致。

## scene_ops（4 工具）

职责：编辑场景的节点生命周期与树读取。注册工具：

| 工具名 | 说明 |
|---|---|
| `create_scene_node` | 按类型创建节点（可指定 parent_path） |
| `delete_scene_node` | 删除节点（禁删根节点） |
| `instantiate_scene` | 实例化 PackedScene 到编辑场景 |
| `get_scene_tree` | 编辑场景树（默认深度 8 + 可选属性摘要；旧 `scene_tree_get`/`scene_get_tree` 双名合并） |

关键实现事实：

- `handle_create`：用 `ClassDBSingleton::is_parent_class(type, "Node")` 校验类型；无 `parent_path` 且已存在根节点时返回错误 `scene already has a root, use parent_path to add children`；新建 Node2D/Node3D 自动切换 2D/3D 主屏幕编辑器。`name`/`type` 缺省默认 `"NewNode"`/`"Node"`，**不校验必填**（AGENTS.md 契约缺口，确认属实）。
- `handle_delete`：根节点拒绝删除（提示用 `close_editor_scene`）；`queue_free()` 延迟释放。
- `handle_instance`：`ResourceLoader.load` + `PackedScene.instantiate`；返回 `note` 提醒实例继承源场景的 CONNECT_PERSIST 连接。
- 树导出 `node_to_json`：属性摘要过滤 `metadata/` 前缀、`_` 开头、OBJECT 类型，单节点上限 20 个属性。
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
- 资源赋值接 `resource_ops::try_resolve_resource_value`；**拒绝把 `memory://` 内存资源赋给节点属性**（返回错误，说明会损坏场景文件，要求先 `save_resource` 落盘）。
- 写后 readback 校验：`util::check_readback` 返回 REJECTED 时 `error_detail` 报错（可能只读/不存在/需 type_hint），CONVERTED 时返回 `warning`。
- Camera2D 特例：`enabled`（默认值 true 不序列化）与 `current`（无 setter，需 `code_execute` 调 `make_current()`）返回 `serialization_note` 指导。
- 属性名纠错提示：Levenshtein 距离候选 + `PROPERTY_RENAME_HINTS` 重命名映射表（9 条，如 `frames→sprite_frames`、`cast_to→target_position`、`translation→position`，与 AGENTS.md 中 Godot 4.x 迁移事实一致）。
- 信号连接默认 `persist=true`（CONNECT_PERSIST，随场景保存）；`signal_disconnect` 幂等（`not_connected` 不算错误）。

## group_ops（3 工具）

职责：把节点加入/移出/查询分组（Godot group，区别于 SceneTree 组调用）。注册工具：

| 工具名 | 说明 |
|---|---|
| `add_group_node` / `remove_group_node` | 增删（可撤销、持久化） |
| `has_group_node` | 查询 |

关键实现事实：

- 路径解析 `find_node`：剥除前导 `/` 与 `root/` 前缀，`get_node_or_null` 解析，并沿父链校验节点确实挂在编辑场景根之下（防跨场景误操作）。
- 增删均走 `EditorUndoRedoManager`（`create_action` / `add_do_method` / `add_undo_method` / `commit_action`），无 undo manager 时直接操作。
- `add_to_group(group, true)` 第二参持久化；操作后 `is_in_group` 反向校验，失败返回 `add_to_group failed: node is not in group after operation`。
- 成功响应含 `persistent: true` 与 `node_path`/`group` 回显。

## input_ops（11 工具）

职责：经 `Input` 单例模拟输入（动作、键盘、鼠标、手柄），用于运行中游戏或编辑器预览。注册工具：

| 工具名 | 说明 |
|---|---|
| `press_input_action` / `release_input_action` | 动作按下（strength）/释放 |
| `is_input_action_pressed` / `is_input_action_just_pressed` | 动作状态查询 |
| `press_input_key` / `release_input_key` | 模拟按键 |
| `move_input_mouse` / `press_input_mouse_button` / `release_input_mouse_button` | 鼠标移动/按键 |
| `start_input_gamepad_vibration` / `stop_input_gamepad_vibration` | 手柄振动启停（原 `input_gamepad_simulate` 拆分而来） |

关键实现事实：

- `parse_key`：单字符（a-z/0-9）直接映射 Key 码；命名键表（SPACE/ENTER/ESCAPE/SHIFT/CONTROL|CTRL/ALT/TAB/BACKSPACE/DELETE/方向键）；**未识别键名返回 `KEY_NONE` 而不报错**（潜在静默失败点）。
- `parse_mouse_button`：仅识别 LEFT/RIGHT/MIDDLE，其余返回 `MOUSE_BUTTON_NONE`。
- `press_input_action` 支持可选 `strength`（默认 1.0）；移动用 `InputEventMouseMotion`，坐标从 `{x, y}` 对象解析。
- 全部 handler 日志级别为 Debug（区别于其他模块的 Info）。

## input_map_ops（8 工具）

职责：`InputMap` 运行期动作定义管理（不模拟按键，而是改映射本身）。注册工具：

| 工具名 | 说明 |
|---|---|
| `add_input_map_action` / `erase_input_map_action` / `has_input_map_action` / `get_input_map_actions` | 动作增删查 |
| `add_input_map_action_event` / `erase_input_map_action_event` / `set_input_map_action_deadzone` | 动作事件增删/死区 |
| `save_input_map` | 运行期改动写入 ProjectSettings 并保存到磁盘 |

关键实现事实：

- 内置约 150+ 条 `KEY_NAME_TO_CODE` 常量表（`"KEY_A"→65` 等 Unicode/功能键码）用于把键名转成 `Key` 码。
- 事件构造基于 `InputEvent`（键盘/鼠标/手柄事件）；`action_set_deadzone` 调 `InputMap::action_set_deadzone`。
- `save_input_map` 是**持久化副作用**：把动作写入 `ProjectSettings` 的 `input/<action>` 并 `save()`；`add_input_map_action_event` 同理。二者均在 `tests/runner/traversal.cpp` 的 `kExcludedSideEffectTools` 排除清单中（与 AGENTS.md 遍历排除约定一致）。

## physics_ops（48 工具）

职责：经 `PhysicsServer2D/3D` 与 `PhysicsDirectSpaceState2D/3D` 直接操作物理世界（对象全部为 RID 句柄，非场景节点）。注册工具：

| 工具名（代表性） | 说明 |
|---|---|
| `intersect_physics_2d_ray` / `intersect_physics_3d_ray` | 射线检测（QueryParameters 参数） |
| `intersect_physics_2d_shape` / `intersect_physics_3d_shape`、`intersect_physics_2d_point` / `intersect_physics_3d_point` | 空间查询两对（原 shape_cast/point_query 四工具删除后合并于此） |
| `create_physics_2d_body` / `create_physics_3d_body`、`create_physics_2d_area` / `create_physics_3d_area`、`create_physics_2d_joint` / `create_physics_3d_joint` | 物理对象创建 |
| `create_physics_2d_circle_shape` / `create_physics_3d_sphere_shape`（+ set_data） | 形状 RID 创建 |
| `apply_physics_3d_body_force` / `apply_impulse` / `apply_torque`、`set_mode` / `get_state` / `set_state`、`add_shape`、`set_axis_lock`、`add/remove_collision_exception`、`set_param` | 3D 刚体控制 |
| `set_physics_3d_area_param` / `set_monitorable` / `set_space` / `set_transform` | 3D 区域控制 |
| `set_physics_3d_space_solver_iterations` / `set_solver_params` / `set_param`、`get_physics_2d_space_direct_state` / `get_physics_3d_space_direct_state` | 空间级操作 |
| `create_physics_3d_soft_body` / `set_mesh` | 软体 |
| `get_physics_node_rid` / `get_debug_object_info` | 节点 ↔ 物理 RID 解析 / 对象 ID → 类名与节点路径（挂靠本模块，归 Debug 类） |

关键实现事实：

- **RID 生命周期**：静态 `RidStore`（`std::unordered_map<int64_t, godot::RID>`）把 int64 句柄↔RID 双向映射并保活；跨请求传参一律用整数 id。注意 `create_physics_2d_circle_shape` 的工具描述明确提示：返回的是 `PhysicsServer2D` RID 句柄，**不可直接赋给 `CollisionShape2D.shape`**（其类型为 `Shape2D` 资源），需要挂载时用资源类工具或 `code_execute`。
- 2D/3D 成对命名（`*_physics_2d_*` / `*_physics_3d_*`）覆盖 20 对 + 空间查询，是命名最整齐的模块；`get_debug_object_info` 自 `resolve_object` 迁移而来（不再无前缀）。
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

- RID 经 `rid.get_id()` / `UtilityFunctions::rid_from_int64(id)` 跨 JSON 传递（int64），与 physics_ops 的字符串/整数风格不同（后者用进程内 RidStore）。
- 地图创建默认不激活（`map_set_active` 需显式传 `active: true`）；区域创建支持 `enabled`/`navigation_layers` 初始参数。
- `set_nav_3d_region_navigation_mesh` 接受 `NavigationMesh` 资源（可经 `ResourceLoader` 加载路径传入）；路径返回点为 Vector2/Vector3 数组序列化（`{x,y[,z]}`）。

## resource_ops（21 工具）

职责：资源生命周期管理——加载/保存/创建/复制/UID/文件操作/依赖/导入，是资源类工具（Resources 类别）的完整实现。注册工具：

| 工具名 | 说明 |
|---|---|
| `load_resource` / `save_resource` / `create_resource` / `duplicate_resource` | 加载/保存/创建/复制 |
| `load_resource_threaded` / `get_resource_load_threaded_status` / `get_resource_load_threaded` | 线程化加载三件套 |
| `get_resource_type` / `has_resource` / `get_resource_types` / `get_resource_extensions` | 类型与存在性 |
| `get_resource_dir_files` / `get_resource_uid` / `set_resource_uid` / `remove_resource_file` / `rename_resource_file` | 文件系统操作 |
| `get_resource_dependencies` / `has_resource_dependency` | 依赖查询 |
| `reimport_resource_files` | 编辑器导入（原 `resource_import` 已删除） |
| `get_resource_property` / `set_resource_property` | 内存资源属性 |

关键实现事实：

- **三路解析**（`resolve_resource`）：`object_id_str`/`object_id` → `ObjectDB` + `resource_registry`（`core/resource_registry.cpp` 的 name:/oid 双键内存缓存）→ `path` 兜底 `ResourceLoader.load`；磁盘加载路径返回 `warning`：磁盘副本**不含**内存中的未保存修改。
- 序列化统一返回 `class/path/object_id/object_id_str/name` 五字段（`serialize_ref`）。
- `handle_reimport` 仅编辑器模式可用（`is_editor_hint` 校验）；空参时 `count=0` 返回 `reimport queued for 0 file(s)` 静默成功（AGENTS.md 契约缺口，确认属实）。
- `handle_get_extensions`：缺 `type` 时调 `get_recognized_extensions_for_type("")` 返回全类型（AGENTS.md 契约缺口，确认属实）。
- `remove_resource_file`/`rename_resource_file` 有磁盘副作用；`set_resource_uid` 修改 `.uid` 文件。

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

- `handle_execute_gdscript`：`is_single_expression` 判定（无换行、非纯赋值、首词不在 18 个关键字黑名单如 if/for/func/return/var/await…）时自动把表达式值作为返回值；多行代码需显式 `return`；输出经 `truncate_capture_text` 截断至 8192 字节。
- 节点路径提示常量：执行节点挂在 `/root` 下而非编辑场景内，`NODE_NOT_FOUND_HINT` 指导用 `SceneRoot.get_node(...)` 访问编辑场景节点。
- `handle_reload` 先 `invalidate_cached_resource`（`ResourceLoader` 缓存引用置空路径）再重载，避免旧资源残留。
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
- `get_engine_version` 返回 Engine 版本字符串（`{major, minor, patch}` 结构）；`set_time_scale`/`set_max_fps` 直接写 `Engine` 单例。
- `save_project_settings` 与 `set_editor_settings` 均写磁盘（持久化副作用），在遍历测试排除清单中。
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
- `get_docs_class` 返回 `name/parent_class/api_type/can_instantiate/methods/properties/signals` 结构。

## editor_ops（22 工具）

职责：Godot 编辑器会话操作（EditorInterface/EditorSelection/EditorUndoRedoManager/EditorFileSystem）。注册工具：

| 工具名 | 说明 |
|---|---|
| `get_editor_selection` / `set_editor_selection` / `get_editor_edited_scene_root` | 选择与编辑根 |
| `save_editor_scene` / `save_editor_scene_as` / `save_editor_scenes` / `create_editor_scene` / `open_editor_scene` / `close_editor_scene` / `reload_editor_scene` | 场景生命周期（原 `editor_new_text_resource` 已删除） |
| `create_editor_undo_redo_action` / `commit` / `add_do` / `add_undo` | 撤销栈四件套 |
| `get_editor_file_system_tree` / `scan_editor_file_system` / `get_editor_file_system_status` | 文件系统（原 `editor_import_resource` 已删除） |
| `play_editor_current_scene` / `stop_editor_playing` | 播放 |
| `set_editor_main_scene` / `inspect_editor_resource` / `set_editor_plugin_enabled` | 杂项（原 `editor_get_plugin_list` 已删除） |

关键实现事实：

- `handle_save_all_scenes` 经 `editor->call("get_unsaved_scenes")` 拿未保存场景列表（godot-cpp 无直接 API）。
- `handle_play_current_scene`：`is_playing_scene()` 防重（返回 `already_playing`），play 后再次校验，失败报 `failed to start scene playback: no game process started`；与 `entry_runtime`（game bridge）配合（见 [../modules/entry_runtime.md](../modules/entry_runtime.md)）。
- 文件系统树导出 `dir_to_json` 递归上限 `MAX_TREE_DEPTH=12`，截断时返回 `truncated: true`。
- `set_editor_main_scene` 写 `application/run/main_scene` 并 `ProjectSettings::save()`；`set_editor_plugin_enabled`、`save_editor_scene` 等均在遍历测试排除清单中。
- `create_editor_undo_redo_action` 支持动作分组名称，`add_do`/`add_undo` 以 node_path + 方法 + 参数形式入栈。

## 与现有文档的不一致点

| 文档 | 声称 | 代码事实 | 判定 |
|---|---|---|---|
| AGENTS.md（命名约定） | 工具命名 `<动词>_<类别>_<维度>_<对象>_<修饰>`，动词置首，如 `intersect_physics_2d_ray` | 本组符合；`signal_connect`/`signal_disconnect`（动词置首，无类别段）、config_ops 的 `get_project_settings`/`set_engine_*`/`get_editor_settings`（类别段为 project/engine/editor，与文件名 `config_` 不一致）属规范内的长短变化 | 一致 ✓（动词置首） |
| AGENTS.md（工具总数） | 332 领域工具 | `tool_defs.def` 恰好 332 条 `TOOL_ENTRY`；本页 13 模块 172 条 | 一致 ✓ |
| AGENTS.md（错误模式） | 领域工具返回 `{"error": "消息"}` | 一致；另有 `error_detail` 扩展格式与 dispatch `catch(...)` 兜底、导出期固定错误 | 一致 ✓（有扩展） |
| AGENTS.md（契约缺口 3 项） | `create_scene_node` 不校验必填；`get_resource_extensions` 缺 type 返回全类型；`reimport_resource_files` 空参静默成功 | 三项均在代码中逐一确认（默认 "NewNode"/"Node"；空 type 传 `""`；count=0 返回 queued） | 一致 ✓ |
| AGENTS.md（遍历排除） | 34 个副作用工具排除 | `kExcludedSideEffectTools` 含 `save_input_map`、`add_input_map_action_event`、`save_project_settings`、`set_editor_settings`、`set_editor_main_scene`、`set_editor_plugin_enabled`、`save_editor_scene` 等本组持久化工具 | 一致 ✓ |
| 命名归属 | 类别前缀应反映模块 | `get_scene_tree` 注册在 scene_ops（非 scene_tree_ops）——旧 `scene_tree_get`/`scene_get_tree` 双名已合并，不再语义重叠 | 已消解 ✓ |
| AGENTS.md（架构） | 工具经 `call_tool` 代理、`register_all.cpp` 的 `g_handlers` 映射分发 | `TOOL_ENTRY` 宏同时填 `g_handlers` 与 catalog；`dispatch.cpp` 非主线程走 `queue.submit()` | 一致 ✓ |
| `input_ops` | （无文档声明） | `parse_key`/`parse_mouse_button` 对未识别名称返回 `KEY_NONE`/`MOUSE_BUTTON_NONE` 而非报错 | 文档空缺，建议补充 |

## 相关页面

- 注册管线与分类：[../modules/tools_registry.md](../modules/tools_registry.md)
- 基础设施与线程模型：[../modules/core.md](../modules/core.md)
- 运行时桥接：[../modules/entry_runtime.md](../modules/entry_runtime.md)
- 入口：[../index.md](../index.md) ｜ 总览：[../overview.md](../overview.md) ｜ 测试：[../tests.md](../tests.md)
