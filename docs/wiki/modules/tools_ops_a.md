---
type: 模块文档
title: 领域工具模块（A 组）
description: 13 个领域工具模块（场景/属性/输入/物理/导航/资源/脚本/配置/文档/编辑器）实现细节
tags:
  - 模块
  - 领域工具
  - A组
timestamp: "2026-09-13T03:47:12+08:00"
resource: src/tools/
---

# 领域工具模块（src/tools/，A 组 13 模块）

> 审计日期：2026-08-29（2026-08-12 初稿；08-17 补 YAML frontmatter；08-20 随 rename 事务化 + 新工具同步；08-21 随 ToolBase 类重构同步——`tool_defs.def`/`TOOL_ENTRY` 移除，注册与计数口径改为 `<域>_tools.hpp`/`ToolRegistry`；08-22 15 时全量一致性审计——editor 计数 23、RENAME_HINTS 10 条、resolve_scene_node/RidStore/rid_from_json/NODE_NOT_FOUND_HINT 归一至 util 共享头、get_docs_class 补 enums/constants；08-29 随 0.2.2 版本与全量审计同步——336→363、35→42、174→178、26→30 域重核；09-13 随反馈修复批次同步——363→365、42→49、178→180，property 数组/NodeType 转换与写失败 fail fast、资源 CoW/reload_resource/copy_resource_file/标签清理、create_editor_scene timeout_ms 诊断），基于当前工作树代码逐行核对（不依赖 git 历史）。
> 覆盖范围：`src/tools/` 下 13 对 `.cpp/.hpp`：scene_ops、scene_tree_ops、property_ops、group_ops、input_ops、input_map_ops、physics_ops、nav_ops、resource_ops、script_ops、config_ops、doc_ops、editor_ops。
> 统计口径：工具数以 `src/tools/*_tools.hpp` 的 `GDA_TOOL_CLASS(`/`GDA_TOOL_CLASS_SIDE(` 声明计数为准（一个工具 = 一个 `ToolBase` 真类，`handle_` 函数与之逐一对应，两口径一致）；当前 365 个域工具 = 316 个 `GDA_TOOL_CLASS(` + 49 个 `GDA_TOOL_CLASS_SIDE(`，L2 遍历只枚举前者（316 个）。注册经 `register_all.cpp` 调用 30 组 `<域>_tools::make_tools()` 汇入单一 `ToolRegistry`。

## 模块简介

这 13 个模块是领域工具的前半部分：覆盖场景节点操作、属性/信号、分组、输入模拟与 InputMap、物理（2D/3D 双份）、导航、资源生命周期、脚本与 GDScript 执行、项目/引擎/编辑器配置、引擎类文档查询、编辑器会话管理。全部位于命名空间 `godot_autopilot::<模块>_ops`，与 AGENTS.md 约定一致。

30 个域 `_tools.hpp` 共声明 **365 个领域工具**（316 个常规 + 49 个以 `GDA_TOOL_CLASS_SIDE(` 标记的副作用工具），本页 13 域占其中 **180 个（49.3%）**。`register_all.cpp` 注册 30 组 `<域>_tools::make_tools()`（另加 `system_status` 1 与 7 个元工具，catalog/index 共 373）汇入单一 `ToolRegistry`，catalog/index/`dispatch::g_handlers`/`server.RegisterTool()` 全部由其派生；领域工具不直接注册到 MCP 服务器，统一经元工具 `call_tool` 分发（见 [../modules/tools_registry.md](../modules/tools_registry.md)）。

## 模块总览

| 模块（命名空间） | handle_ 函数数 | 注册工具数 | 核心职责 |
|---|---|---|---|
| `scene_ops` | 6 | 6 | 编辑场景节点创建/删除/实例化/树遍历 |
| `scene_tree_ops` | 8 | 8 | SceneTree 层操作：组调用、暂停、定时器 |
| `property_ops` | 5 | 5 | 节点属性读写、属性列表、信号连接 |
| `group_ops` | 3 | 3 | 节点分组增删查（可撤销、持久化） |
| `input_ops` | 11 | 11 | Input 单例模拟按键/鼠标/手柄/动作 |
| `input_map_ops` | 8 | 8 | InputMap 运行期动作增删改查与持久化 |
| `physics_ops` | 48 | 48 | PhysicsServer 2D/3D 对象与空间查询（含挂靠 Debug 类的 `get_debug_object_info`） |
| `nav_ops` | 15 | 15 | NavigationServer 2D/3D 地图/区域/代理 |
| `resource_ops` | 26 | 26 | 资源加载/保存/创建/重载/复制/UID/导入/依赖/反查（含 09-13 新增 reload_resource/copy_resource_file） |
| `script_ops` | 10 | 10 | GDScript 执行、脚本附加/属性/调用 |
| `config_ops` | 13 | 13 | ProjectSettings/Engine/EditorSettings |
| `doc_ops` | 4 | 4 | ClassDB 类/方法/属性文档查询 |
| `editor_ops` | 23 | 23 | 编辑器会话：选择/场景/撤销/播放/文件系统/C# 构建 |
| **合计** | **180** | **180** | |

## 公共模式

- **错误返回**：全部失败路径返回 `{"error": "消息"}`，与 AGENTS.md 描述一致；消息前缀常见 `missing required parameter: <name>`、`<X> not available`、`node not found: <path> — <hint>`。`util::error_detail`（`src/util/error_util.cpp`）是扩展形态：把 fact/position/expected/action 拼进 error 字符串（`get_scene_tree` 的无场景错误即此格式）。
- **成功返回**：`{"result": <值>}`；写场景类工具额外带 `undo` 提示字段（如 `create_scene_node` 返回 `undo: "delete node <path> (delete_scene_node)"`），部分带 `note`/`warning`/`serialization_note` 辅助字段。
- **脏标记**：修改编辑场景的模块（scene_ops、group_ops、property_ops、editor_ops）调用 `scene_dirty_tracker::mark_scene_modified()`（见 [../modules/core.md](../modules/core.md)）。
- **异常兜底**：`dispatch::call_handler` 对 handler 的 C++ 异常 `catch(...)` 后返回 `{"error": "internal error in tool '<name>'..."}`；导出期间（ExportGuard 置位）所有领域工具返回固定错误 `{"error":"editor is exporting; retry after export completes"}`。
- **线程**：handler 内部不直接判断线程；`dispatch::call_handler`（`dispatch.cpp`）在非主线程时经 `CommandQueue::submit().get()` 桥接，与 core.md 的线程模型一致。

## scene_ops（6 工具）

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
- **Node 类型属性 + 节点路径自动转引用（09-13 扩展）**：`handle_set` 检测目标属性 `type==OBJECT`、hint 为 `PROPERTY_HINT_NODE_TYPE`（引擎值 34，含 C# `[Export]` 节点字段），或 hint 为 `PROPERTY_HINT_RESOURCE_TYPE` 且 `hint_string` 指向 Node 子类时，若 `value` 为字符串（节点路径），以编辑场景根 `get_node_or_null()` 解析后赋**节点对象引用**（而非 NodePath Variant），使其保存场景时正确落入 `node_paths`、实例化后还原为节点引用。解析失败返回 `error_detail`（含当前场景根与合法路径写法指引，不再静默 ok）；成功结果带 `converted_node_path` 字段标注。判断辅助 `parse_hint_class`/`is_node_class`（容错 `Type:` 前缀、逗号 token）。
- **数组属性逐元素转换（09-13 新增）**：属性 `type==ARRAY` 时不再整体反序列化——① 非数组 JSON 值直接报错 `refusing to write (a non-array value would silently clear the array)`（传 `[]` 显式清空）；② 从属性元数据 `hint_string`（`PROPERTY_HINT_TYPE_STRING` 形如 `type/hint:class`）解析元素类型（`util::parse_array_element_hint`，`type_hint.hpp`）；Node 元素接受路径字符串或 `{"__node_ref__": "..."}`，资源元素接受 `res://`/`memory://` 字符串或 `{"path": ...}`/`{"resource": ...}`，`null` 留空槽；③ 元素类型无法确定而元素是对象/数组时拒绝写入；④ 依声明类型构建 typed array，元素不匹配时报错。无法安全表达的元素一律报错，不静默丢弃。
- 写后 readback 校验：`util::check_readback` 新增 `type_sensitive` 参数（`readback_util.hpp`，Object/数组属性写入时启用）——回读为 nil、数组被清空或元素为 null 判 REJECTED，返回 `value not applied` 错误并尝试恢复旧值（恢复结果写入错误文案）；普通 REJECTED 报错（可能只读/不存在/需 type_hint），CONVERTED 返回 `warning`。
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

- 路径解析 `find_node`：本地薄包装，转发 `util::resolve_scene_node`（`util/scene_path.hpp`，全仓 7 处 `find_node` 的归一实现）——依次剥除前导 `/`、`root`、场景根名三段，`get_node_or_null` 解析，并沿父链校验节点确实挂在编辑场景根之下（防跨场景误操作）。
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
- `save_input_map` 是**持久化副作用**：把动作写入 `ProjectSettings` 的 `input/<action>` 并 `save()`；`add_input_map_action_event` 同理。二者均以 `GDA_TOOL_CLASS_SIDE(` 声明为副作用工具（`side_effect` 非空），遍历经 `get_tool_detail` 的 `side_effect` 字段自动排除，不再硬编码清单（与 AGENTS.md 遍历排除约定一致）。

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

- **RID 生命周期**：`RidStore`（`std::unordered_map<int64_t, godot::RID>`，定义于 `util/rid_registry.hpp` 的 `rid_store<Physics>()` 模板标签实例——08-22 起跨域共享，text_ops 亦复用）把 int64 句柄↔RID 双向映射并保活；跨请求传参一律用整数 id。注意 `create_physics_2d_circle_shape` 的工具描述明确提示：返回的是 `PhysicsServer2D` RID 句柄，**不可直接赋给 `CollisionShape2D.shape`**（其类型为 `Shape2D` 资源），需要挂载时用资源类工具或 `code_execute`。
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

- **三路解析**（`resolve_resource`）：`object_id_str`/`object_id` → `ObjectDB` + `resource_registry`（`core/resource_registry.cpp` 的 name:/oid 双键内存缓存）→ `path` 兜底 `ResourceLoader.load`；`path` 加载成功会把实例注册进 `resource_registry`（09-13 起），使后续调用经 `object_id`/`name:` 定位到同一实例、跨调用修改存活。磁盘加载路径返回 `warning`：磁盘副本**不含**内存中的未保存修改。
- 序列化统一返回 `class/path/object_id/object_id_str/name` 五字段（`serialize_ref`）。
- **`save_resource` copy-on-write（09-13 起）**：`path` 为定位路径与默认目标，`dest_path≠path` 且资源经 `path` 定位时，先 `duplicate(true)` 深拷贝再写盘——源缓存实例不被改写；响应增 `copy_on_write: true` 与 `source_path`，且验证加载时排除了"回读到源实例"的假阳性。
- **`reload_resource`（09-13 新增）**：对 `path` 以 `CACHE_MODE_REPLACE` 强制重新加载，替换编辑器缓存实例；响应 `{result:"reloaded", path, replaced:true, reused_cached_instance}`——`replaced` 表示缓存已随 reload 刷新（当前实现恒为 true），`reused_cached_instance:true` 表示引擎复用了原实例而非新建。文件缺失/无法加载时报错。
- `handle_reimport` 仅编辑器模式可用（`is_editor_hint` 校验）；空参时 `count=0` 返回 `reimport queued for 0 file(s)` 静默成功（AGENTS.md 契约缺口，确认属实）。**reimport 只对带 `.import` 伴生的文件有效**：对 `.tres/.gd` 等直接文本文件引擎会记 BUG 且无导入效果，工具仍报 queued——刷新这类文件用 `reload_resource`。
- `handle_get_extensions`：缺 `type` 时调 `get_recognized_extensions_for_type("")` 返回全类型（AGENTS.md 契约缺口，确认属实）。
- `remove_resource_file` 有磁盘副作用；`set_resource_uid` 修改 `.uid` 文件。
- **创建/复制支持脚本全局类（09-13 起）**：`create_resource` 的 `type` 接受 ClassDB 引擎类（原有）与全局脚本类（GDScript `class_name` / C# `[GlobalClass]`，经 `ProjectSettings.get_global_class_list()` 定位脚本，脚本不可实例化或基类非 Resource 时报错）；`get_resource_types` 列表同样并入可实例化的全局脚本类（排序后追加）。
- **`duplicate_resource` 新增可选 `name`（09-13 起）**：提供时设置资源名；无论是否提供都会注册进 `resource_registry`（oid 键保活副本，提供 `name` 时另有 name 键，可经 `memory://name` 再次定位）。
- **`copy_resource_file`（09-13 新增）**：`path`→`dest_path` 逐字节复制（两者必填，父目录递归创建、目标已存在则覆盖），随后 `update_file` + 编辑器文件系统扫描；**不复制 `.import`/`.uid` sidecar、不改写引用**（导入资产需 reimport），响应 `{result:"copied", from, to}`。需要保留引用关系时改用 rename/move。
- **`rename_resource_file` 事务化（08-20 起）**：改名不再只是 Move+索引重建——① 先搬运伴生 `.uid` 文件（保 uid 不重生成，P0-2 修复）；② rename 前扫描 res:// 全部 `.tscn/.tres` 收集依赖，rename 后对命中 `path="<旧>"` 的依赖做文本回写（P0-1 修复），写前关闭读句柄、写后 flush/close 并回读验证，验证不过入 `stale_references` 而非乐观上报；③ 返回结构化影响报告 `{result, updated_files:[{file,changes}], stale_references:[{file,reason}], uid_preserved}`（P2-1）；④ `script_class` 因无法可靠确认全局类名（曾依赖 `ResourceLoader.load` 重入编辑器文件系统，有崩溃风险）一律列入 `stale_references` 由调用方人工确认。**目录参数 fail fast（09-13 起）**：`path` 是目录时报"directory rename not supported"并指引改用 `move_resource_file`（目录改名不会改写依赖引用）。`get_resource_references` 复用同一扫描器反查引用（path/uid/class_name 三选一）。
- **目录 move 排除 sidecar（09-13 起）**：`move_resource_file` 目录遍历时跳过 `.uid`/`.import` sidecar，主文件各自走引用改写事务、sidecar 随主文件迁移再统一清理，避免 sidecar 被当独立文件重复搬运。
- **移动/删除后自动关闭旧路径标签（09-13 起）**：rename/move/remove 后仍指向旧路径的已打开场景标签会被自动关闭，响应附 `closed_tabs`；当前正在编辑的标签或无法关闭的标签留开并在 `closed_tabs_warning` 中说明；`remove_resource_file` dry-run 另在 `open_tabs` 中预告受影响标签。
- 工程约束：`collect_text_file_paths` 用 `join_path` 拼接（`res://` 根免产生 `res:///` 三重斜杠，否则 WRITE 提交会失败并残留 `.tmp`）。

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
| `create_editor_undo_redo_action` / `commit` / `add_do` / `add_undo` | 撤销栈四件套 |
| `get_editor_file_system_tree` / `scan_editor_file_system` / `get_editor_file_system_status` | 文件系统（原 `editor_import_resource` 已删除） |
| `play_editor_current_scene` / `stop_editor_playing` | 播放 |
| `set_editor_main_scene` / `inspect_editor_resource` / `set_editor_plugin_enabled` | 杂项（原 `editor_get_plugin_list` 已删除） |
| `build_csharp_assembly` | C# 工程编译（`dotnet build` 子进程，见下） |

关键实现事实：

- `handle_save_all_scenes` 经 `editor->call("get_unsaved_scenes")` 拿未保存场景列表（godot-cpp 无直接 API）。
- **`create_editor_scene` 可靠性（09-13 起）**：新增 `timeout_ms`（默认 `GDA_NEW_SCENE_SWITCH_WAIT_MS=2000`，允许 50-30000，非整数或越界直接报错）；超时失败时释放未被编辑器接管的临时根节点（`node_released`），并返回 `waited_ms`/`timeout_ms`/`node_released`/`editor_state`（`edited_root`/`unsaved_scenes`/`file_system{scanning,progress}`）诊断；`close_current` 各失败路径先释放临时节点再报错。
- **`save_editor_scene` 失败返回 `error` 字段**（含 error code 与路径，不再只回原始枚举值）；**`save_editor_scene_as` 成功返回 `{result:"saved"}`** 并附 `tab_path_registered`/`tab_rebuilt`——Godot 4.7 的 `EditorInterface::save_scene_as` 不更新编辑器内部标签路径，插件在路径未注册时 close+reopen 重建标签以保持路径一致，重建失败附 `warning`（09-13 起）。
- **`reload_editor_scene` 未打开的场景返回 error**（"scene is not open"）而非假成功；成功返回 `{result:"reloaded", scene_path, observed?}`（`observed:true` 表示已确认编辑根路径匹配）（09-13 起）。
- **`open_editor_scene` 的 unsaved 错误补恢复指引**：除"先保存"外，提示可对列出的路径调 `reload_editor_scene` 丢弃改动后重试（09-13 起）。
- `handle_play_current_scene`：`is_playing_scene()` 防重（返回 `already_playing`），play 后再次校验，失败报 `failed to start scene playback: no game process started`；与 `entry_runtime`（game bridge）配合（见 [../modules/entry_runtime.md](../modules/entry_runtime.md)）。
- 文件系统树导出 `dir_to_json` 递归上限 `MAX_TREE_DEPTH=12`，截断时返回 `truncated: true`。
- `set_editor_main_scene` 写 `application/run/main_scene` 并 `ProjectSettings::save()`；`set_editor_plugin_enabled`、`save_editor_scene` 等均以 `GDA_TOOL_CLASS_SIDE(` 声明（`side_effect` 非空），遍历自动排除。
- `create_editor_undo_redo_action` 支持动作分组名称，`add_do`/`add_undo` 以 node_path + 方法 + 参数形式入栈。
- **`build_csharp_assembly`（08-20 新增）**：在 res:// 根定位 `.csproj/.sln`（无则报非 C# 工程），经 `OS::create_process` 异步启动 `dotnet build --nologo <project>`（非阻塞，沿用 os_ops 模式，避免冻结编辑器），返回 `{project_file, command, started, pid, note}`；`started` 为假时附 `error` 说明 SDK 缺失。`note` 明示能力边界：GDExtension(C++) 无公共 API 触发编辑器内 C# 程序集热重载（该能力在引擎 `modules/mono` 内部），仅能在 CI/命令行式编译验证；编辑器内类/签名变更后的验证仍须用户在编辑器点 Build 或重启。以 `GDA_TOOL_CLASS_SIDE(` 声明（`side_effect` 非空），遍历自动排除。

## 与现有文档的不一致点

| 文档 | 声称 | 代码事实 | 判定 |
|---|---|---|---|
| AGENTS.md（命名约定） | 工具命名 `<动词>_<类别>_<维度>_<对象>_<修饰>`，动词置首，如 `intersect_physics_2d_ray` | 本组符合；`signal_connect`/`signal_disconnect`（动词置首，无类别段）、config_ops 的 `get_project_settings`/`set_engine_*`/`get_editor_settings`（类别段为 project/engine/editor，与文件名 `config_` 不一致）属规范内的长短变化 | 一致 ✓（动词置首） |
| AGENTS.md（工具总数） | 365 领域工具 | 30 个 `*_tools.hpp` 恰为 365 个 `GDA_TOOL_CLASS(`/`GDA_TOOL_CLASS_SIDE(`；本页 13 域 180 个 | 一致 ✓ |
| AGENTS.md（错误模式） | 领域工具返回 `{"error": "消息"}` | 一致；另有 `error_detail` 扩展格式与 dispatch `catch(...)` 兜底、导出期固定错误 | 一致 ✓（有扩展） |
| AGENTS.md（契约缺口 3 项） | `create_scene_node` 不校验必填；`get_resource_extensions` 缺 type 返回全类型；`reimport_resource_files` 空参静默成功 | 三项均在代码中逐一确认（默认 "NewNode"/"Node"；空 type 传 `""`；count=0 返回 queued） | 一致 ✓ |
| AGENTS.md（遍历排除） | 49 个副作用工具排除 | 49 个副作用工具以 `GDA_TOOL_CLASS_SIDE(` 声明并经 `ISideEffect` 暴露 `side_effect`；L2 遍历解析器仅匹配 `GDA_TOOL_CLASS(`（316 个入枚举），经 `get_tool_detail` 返回的 `tool.side_effect` 非空再兜底排除，不再硬编码清单 | 一致 ✓ |
| 命名归属 | 类别前缀应反映模块 | `get_scene_tree` 注册在 scene_ops（非 scene_tree_ops）——旧 `scene_tree_get`/`scene_get_tree` 双名已合并，不再语义重叠 | 已消解 ✓ |
| AGENTS.md（架构） | 工具经 `call_tool` 代理、`register_all.cpp` 的 `g_handlers` 映射分发 | 工具由 `<域>_tools::make_tools()` 汇入 `ToolRegistry`，`register_all.cpp` 由 registry 派生填充 `g_handlers` 与 catalog；`dispatch.cpp` 非主线程走 `queue.submit()` | 一致 ✓ |
| `input_ops` | （无文档声明） | `parse_key`/`parse_mouse_button` 对未识别名称返回 `KEY_NONE`/`MOUSE_BUTTON_NONE` 而非报错 | 文档空缺，建议补充 |

## 相关页面

- 注册管线与分类：[../modules/tools_registry.md](../modules/tools_registry.md)
- 基础设施与线程模型：[../modules/core.md](../modules/core.md)
- 运行时桥接：[../modules/entry_runtime.md](../modules/entry_runtime.md)
- 入口：[../index.md](../index.md) ｜ 总览：[../overview.md](../overview.md) ｜ 测试：[../tests.md](../tests.md)
