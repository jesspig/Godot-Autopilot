---
type: 模块文档
title: 领域工具模块（A 组）
description: A 组 15 个域、133 个 canonical 工具（场景/属性/输入/物理/导航/资源/脚本/配置/编辑器/分析/截图）的 action 表与别名归属
tags:
  - 模块
  - 领域工具
  - A组
  - action
timestamp: "2026-11-25T23:22:40+08:00"
resource: src/tools/domains/
---

# 领域工具模块（src/tools/domains/，A 组 15 域 133 工具）

> 审计日期：2026-11-23（增量同步）。数值取自 [tool_surface_report.md](../tool_surface_report.md) 与当前源码 catalog 基线逐项统计：catalog 252 = 246 个域工具 + 5 个元工具 + `system_status` 1，35 个域文件；生产别名 325、schema 235 非空 / 17 空、能力矩阵 561 行缺失 0。本轮工具名、域数与 action 表未变，改动集中在 handler 校验与事务增强；副作用口径变化见报告页。本页覆盖其中 15 个域、133 个 canonical 工具。

## 口径与别名窗口

- **canonical 工具**是本页表格第一列的真实工具名，也是 `search_tools`、`get_tool_detail`、`ToolCatalog` 唯一呈现的名字。
- **旧工具名**只存在于“旧名（别名）”列。它们不占 catalog、不进 BM25 索引，只经 `call_tool` / `batch_execute` 解析到 canonical；调用成功时结果顶层附 `deprecated_alias`、`canonical_name`、`alias_removal_version`、`deprecated_since`。契约见 [tool_surface_conventions.md](../tool_surface_conventions.md)。
- 全部 325 个生产别名统一 `deprecated_since="0.4.0"`、`removal_version="0.6.0"`；`VERSION` 当前为正式版 0.3.5。到期删除别名，不恢复为正式工具。
- action 工具的单工具 action 上限 8 个；action 专用参数在 schema 中为可选，handler 按 action 校验必填。每个工具至少一条示例存于 `ToolSpec.examples`，经 `get_tool_detail` 的 `tool.examples` 查看。
- 本页只列 canonical 行为；handler 级历史细节已由 [tool_surface_report.md](../tool_surface_report.md) 与 [changelog/2026-11-16-log.md](../changelog/2026-11-16-log.md) 承接，不再复述已删除的工具。

## Scene（7）

| 工具 | action / mode | 说明 | 副作用 | 旧名（别名，0.4.0 弃用 → 0.6.0 移除） |
|---|---|---|---|---|
| `create_scene_node` | mode: node / scene / spec / bundle / path | 一次完成建节点、实例化场景、声明式建树、节点+属性+脚本+分组+信号原子捆绑、建路径节点 | WritesScene | `instantiate_scene`（scene）、`build_nodes_from_spec`（spec）、`spawn_node_bundle`（bundle）、`create_path_node`（path） |
| `edit_scene_node` | rename / reparent / advance_path / duplicate / reorder / replace | 改名、改父级、推进 PathFollow、复制子树、替换节点、调整同级顺序（单 undo 步）；reparent 撤销按固定顺序登记（脱离新父 → 挂回旧父 → 复位索引 → 按需还原变换 → 还原 owner） | WritesScene | `rename_scene_node`、`reparent_node`、`advance_path_follow` |
| `delete_scene_node` | — | 删除节点；拒删场景根，写入编辑器 undo 历史 | Destructive | — |
| `pack_scene` | — | 把子树打包为 res:// 下的 PackedScene 文件，落盘后重载校验 | WritesFile | — |
| `get_scene_tree` | — | 遍历编辑场景树；`pattern`/`type`/`group` 切换为过滤模式，`verify`/`paths`/`observe` 附核验、屏坐标与截图 | None | `get_scene_snapshot`、`get_scene_node_screen_rect`、`verify_scene_saved`、`has_group_node` |
| `scene_camera` | focus / set_params / project / unproject / project_ray / behind | 对准相机、改参数、在客户区/画布/3D 坐标间换算 | WritesScene | `focus_camera_on_node`、`set_camera_params` |
| `get_camera_view_rect` | — | 相机视野在编辑器客户区的矩形描述（`client = offset + pixel * scale`） | None | — |

## SceneTree（1）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `game_scene_tree` | call_group / notify_group / get_nodes_in_group / create_timer / is_paused / set_pause / set_debug_collisions_hint / reload_current_scene | 对运行中游戏的 SceneTree 操作，经运行时通道转发，永不触碰编辑器树；需 `game_runtime` 授权与 `confirm=true` | GameRuntime | `call_scene_tree_group`、`notify_scene_tree_group`、`get_scene_tree_nodes_in_group`、`create_scene_tree_timer`、`is_scene_tree_paused`、`set_scene_tree_pause`、`set_scene_tree_debug_collisions_hint`、`reload_scene_tree_current_scene` |

## Properties（6）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `property_get` | — | 读单节点属性（支持批量 `properties` 与 `metadata/` 通道） | None | — |
| `property_set` | — | 写节点属性：严格形状转换、对象引用、属性族展开、写后 readback 与 undo；批量失败逐项校验回滚，附 `rolled_back` / `unrestored` 收据 | WritesScene | `set_properties_batch` |
| `property_get_list` | — | 列出节点属性及元数据（`only_script_variables`、`property_filter`） | None | — |
| `signal_connect` | — | 连接信号到目标方法，默认持久化 | WritesScene | — |
| `signal_disconnect` | — | 断开由 `signal_connect` 建立的连接（幂等） | WritesScene | — |
| `set_node_transform` | — | 单节点位移/旋转/缩放单 undo 步 | WritesScene | — |

## Group（1）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `node_group` | add / remove | 增删节点分组，注册编辑器 undo/redo | WritesScene | `add_group_node`、`remove_group_node` |

## Input（6）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `get_input_state` | action_pressed / action_just_pressed | 读编辑器进程动作按下/本帧刚按下状态 | None | `is_input_action_pressed`、`is_input_action_just_pressed` |
| `input_action` | press / release | 按/放动作（`strength` 模拟模拟量） | ModifiesWindow | `press_input_action`、`release_input_action` |
| `input_key` | press / release | 注入原始按键事件 | ModifiesWindow | `press_input_key`、`release_input_key` |
| `input_mouse` | move / press / release / click / scroll / drag | 注入鼠标移动、按键、一次点击、滚轮、插值拖拽 | ModifiesWindow | `move_input_mouse`、`press_input_mouse_button`、`release_input_mouse_button`、`click_input_mouse`、`scroll_input_mouse`、`drag_input_mouse` |
| `input_text` | type | 向当前焦点控件整体注入文本（非逐键），`submit` 追加回车 | ModifiesWindow | `type_input_text` |
| `input_gamepad` | start_vibration / stop_vibration | 手柄双马达振动启停 | ModifiesWindow | `start_input_gamepad_vibration`、`stop_input_gamepad_vibration` |

## InputMap（4）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `get_input_map` | list / has | 列动作名或检查动作存在 | None | `get_input_map_actions`、`has_input_map_action` |
| `set_input_map_action` | set | 确保动作存在并一次应用死区与事件；`persist` 决定是否写 ProjectSettings；键名解析安全化（数字键安全转换、F1–F12 识别修复） | WritesConfig | `add_input_map_action`、`add_input_map_action_event`、`set_input_map_action_deadzone`、`set_input_map_action_bundle` |
| `erase_input_map` | action / event | 删除整个动作或其中一条事件并持久化 | WritesConfig | `erase_input_map_action`、`erase_input_map_action_event` |
| `save_input_map` | — | 把当前 InputMap 写入 ProjectSettings 并保存 | WritesConfig | — |

## Physics（46）

| 工具 | 说明 | 旧名（别名） |
|---|---|---|
| `get_physics_2d_space_direct_state` / `get_physics_3d_space_direct_state` | 取物理空间直查状态（空间查询入口） | — |
| `physics_query` | `dimension=2/3` 的统一查询：intersect_point / intersect_ray / intersect_shape / cast_motion / collide_shape / rest_info / test_motion / contacts / area_overlaps | `intersect_physics_2d_ray`、`intersect_physics_2d_shape`、`intersect_physics_2d_point`、`intersect_physics_3d_ray`、`intersect_physics_3d_shape`、`intersect_physics_3d_point` |
| `create_physics_2d_body` / `create_physics_3d_body` | 创建物理体并返回 RID | — |
| `set_physics_2d_body_mode` / `set_physics_3d_body_mode` | 设置模拟模式 | — |
| `apply_physics_2d_body_force` / `apply_physics_3d_body_force` | 施加持续力 | — |
| `apply_physics_2d_body_impulse` / `apply_physics_3d_body_impulse` | 施加速度冲量 | — |
| `set_physics_2d_body_state` / `get_physics_2d_body_state` | 2D 物理体状态写/读 | — |
| `set_physics_3d_body_state` / `get_physics_3d_body_state` | 3D 物理体状态写/读 | — |
| `create_physics_2d_joint` / `create_physics_3d_joint` | 创建关节 | — |
| `create_physics_2d_area` / `create_physics_3d_area` | 创建区域 | — |
| `set_physics_2d_area_monitorable` / `set_physics_3d_area_monitorable` | 设置区域可被监测 | — |
| `apply_physics_3d_body_torque` | 施加扭矩 | — |
| `set_physics_3d_body_axis_lock` | 锁定/解锁单轴 | — |
| `add_physics_3d_body_collision_exception` / `remove_physics_3d_body_collision_exception` | 增删碰撞例外 | — |
| `set_physics_3d_joint_param` | 设置 3D 关节参数 | — |
| `set_physics_3d_area_space` | 把 3D 区域挂到空间 | — |
| `set_physics_3d_space_solver_iterations` / `set_physics_3d_space_solver_params` | 设置空间求解器迭代与参数 | — |
| `create_physics_3d_soft_body` / `set_physics_3d_soft_body_mesh` | 创建软体并设置网格 | — |
| `create_physics_2d_circle_shape` / `create_physics_3d_sphere_shape` | 创建 2D 圆 / 3D 球形状 RID | — |
| `set_physics_2d_shape_data` / `set_physics_3d_shape_data` | 设置形状几何数据 | — |
| `add_physics_3d_body_shape` | 给 3D 物理体挂形状 | — |
| `set_physics_3d_body_param` / `set_physics_3d_area_param` / `set_physics_3d_space_param` | 按枚举设置体/区域/空间参数 | — |
| `set_physics_3d_area_transform` / `set_physics_3d_body_transform` | 设置 3D 区域/物理体全局变换 | — |
| `get_physics_node_rid` | 场景节点 → 物理 RID | — |
| `get_debug_object_info` | ObjectID → 类名与身份（Category 为 Debug） | — |
| `set_physics_body_bundle` | 体+形状+参数一次创建 | — |
| `create_collision_shape` / `assign_collision_shape` | 内存 Shape2D/3D 创建与挂到 CollisionShape 节点 | — |

## Nav（14）

| 工具 | 说明 | 旧名（别名） |
|---|---|---|
| `create_nav_2d_map` / `create_nav_3d_map` | 创建 2D/3D 导航地图 | — |
| `set_nav_3d_map_cell_size` | 设置 3D 地图单元格尺寸 | — |
| `create_nav_2d_region` / `create_nav_3d_region` | 创建导航区域 | — |
| `set_nav_3d_region_navigation_mesh` | 给 3D 区域指定 NavigationMesh | — |
| `create_nav_2d_agent` / `create_nav_3d_agent` | 创建导航代理 | — |
| `set_nav_2d_agent_velocity` / `set_nav_3d_agent_velocity` | 设置代理想速度 | — |
| `create_nav_3d_obstacle` | 创建 3D 导航障碍 | — |
| `nav_query` | 2D/3D 导航路径、最近点与代理状态查询（mode 区分） | `get_nav_2d_map_path`、`get_nav_3d_map_path`、`get_nav_3d_map_closest_point_to_segment`、`get_nav_3d_agent_state` |
| `bake_navigation_mesh` | 从场景节点下几何烘焙 NavigationMesh | — |
| `astar_path` | 内存 AStar 图创建、加点、连边与寻路 | — |

## Resources（10）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `resource_load` | load / reload / threaded_start / threaded_status / threaded_get | 资源加载、强制重载与线程化加载三件套 | None | `load_resource`、`reload_resource`、`load_script`、`load_resource_threaded`、`get_resource_load_threaded_status`、`get_resource_load_threaded` |
| `resource_query` | type / exists / types / extensions / dir / uid / deps / references / property / curve_baked | 只读资源查询：类型、存在性、目录、UID、依赖/反查、属性、曲线烘焙点 | None | `get_resource_type`、`has_resource`、`get_resource_types`、`get_resource_extensions`、`get_resource_dir_files`、`get_resource_uid`、`get_resource_dependencies`、`has_resource_dependency`、`get_resource_references`、`get_curve_baked_points` |
| `resource_create` | create / duplicate / curve / curve_points / primitive / array | 创建内存资源、复制、曲线/控制点、PrimitiveMesh、ArrayMesh | None | `create_resource`、`duplicate_resource`、`create_curve_resource`、`set_curve_points`、`create_primitive_mesh`、`create_array_mesh` |
| `resource_mesh` | surface_material | 给网格表面指派材质 | None | `set_mesh_surface_material` |
| `resource_property` | get / set | 读写内存资源属性（写后 readback） | None | `get_resource_property`、`set_resource_property` |
| `resource_file` | copy / rename / move / remove / create_dir | 资源文件复制、改名、移动、删除、建目录；rename/move 事务化改写引用，sidecar 逆序回滚、重导入/重载并把引用绑回受影响场景属性 | WritesFile | `copy_resource_file`、`rename_resource_file`、`move_resource_file`、`remove_resource_file`、`create_directory` |
| `resource_save` | save | 保存资源到磁盘（支持 copy-on-write 与落盘校验）；按 object_id 保存时保留 `memory://` 别名 | WritesFile | `save_resource` |
| `resource_uid_set` | set_uid | 写入资源 UID：显式非正数拒绝、已占用需 `overwrite=true`，写入后复核注册状态 | WritesFile | `set_resource_uid` |
| `resource_import` | reimport / import_options | 排队重导入、写 `.import` 白名单选项；reimport 参数畸形（缺 files/path、空数组或非字符串）直接报错不假成功 | WritesFile | `reimport_resource_files`、`set_resource_import_options` |
| `assign_mesh_to_instance` | — | 把网格挂到 MeshInstance2D/3D 节点 | WritesScene | — |

## Scripts（10）

| 工具 | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|
| `execute_script` | 在编辑器进程执行 GDScript（`expression`/`source_code` 互斥别名、行号映射） | CodeExecute | — |
| `create_script` | 创建并写盘 GDScript，附磁盘字节/哈希与诊断 | WritesFile | — |
| `patch_script` | 锚点补丁一处脚本并以编译门/回滚校验 | WritesFile | — |
| `attach_script_to_node` / `detach_script_from_node` | 附加/分离脚本（fresh 装载） | WritesScene | — |
| `get_script_property` / `set_script_property` | 读脚本/节点属性；轻量无 undo 写入 | None / WritesScene | — |
| `call_script_node` | 调用编辑场景节点方法 | None | — |
| `reload_script` | 从磁盘重读并重编译（`CACHE_MODE_IGNORE`） | None | — |
| `get_script_property_list` | 枚举脚本声明变量 | None | — |

## Config（8）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `get_project_settings` | get / has | 读 project.godot 设置或检查存在 | None | `has_project_settings` |
| `update_project_settings` | set / save | 改内存设置并保存到 project.godot；`autoload/` 前缀键（大小写不敏感）拒写，改用 `update_autoloads` | WritesConfig | `set_project_settings`、`save_project_settings` |
| `get_editor_settings` | get / has | 读编辑器偏好（附 hint / enum_options） | None | `has_editor_settings` |
| `update_editor_settings` | set | 改编辑器偏好并落盘 | WritesConfig | `set_editor_settings` |
| `get_engine_state` | version / fps / frames_drawn / time_scale | 读引擎版本、FPS、累计帧、时间缩放 | None | `get_engine_version`、`get_engine_fps`、`get_engine_frames_drawn`、`get_engine_time_scale` |
| `update_engine_state` | time_scale / max_fps | 改时间缩放与帧率上限；time_scale 限有限数且 0..上限、max_fps 限合法区间 | ModifiesWindow | `set_engine_time_scale`、`set_engine_max_fps` |
| `get_autoloads` | list | 按加载序列出自启动单例 | None | `get_autoload_list` |
| `update_autoloads` | add / remove / move | 增删/排序自启动单例并即时保存；add 使用共享脚本能力判定，接受非 Tool 的 C# 脚本，返回 `runtime_mountable` / `editor_runnable` / `code`，已存在同路径返回 `already_present` | WritesConfig | `add_autoload`、`remove_autoload`、`move_autoload` |

## Docs（1）

| 工具 | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|
| `get_docs_class` | 从 ClassDB 取类的反射签名（无 docstring；给 `query` 时搜索类名） | None | `find_docs_class`、`get_docs_method`、`get_docs_property` |

## Editor（16）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `get_editor_state` | selection / root / open / unsaved / playing / info | 读选择、编辑根、打开/未保存场景、播放状态 | None | `get_editor_selection`、`get_editor_edited_scene_root` |
| `get_editor_ui` | elements / hit / geometry / scene_tree_items | 枚举编辑器控件、命中链、视口几何、Scene 树行 | None | `get_editor_ui_elements`、`hit_test_editor_point`、`get_editor_viewport_geometry`、`scene_tree_items` |
| `get_editor_files` | tree / status / scan | 编辑器文件系统树、状态与扫描 | None | `get_editor_file_system_tree`、`get_editor_file_system_status`、`scan_editor_file_system` |
| `editor_scene_control` | create / open / close / reload / play / stop | 场景新建、打开、关闭、重载、播放、停止；play 返回 `autosaved` / `disk_changed` / `autosave_setting`，显式说明 Godot 默认的运行前保存；场景写工具返回 `disk_persisted=false`，save 返回 `disk_persisted=true` | ModifiesWindow | `create_editor_scene`、`open_editor_scene`、`close_editor_scene`、`reload_editor_scene`、`play_editor_current_scene`、`stop_editor_playing` |
| `save_editor_scene` | save / save_as / save_all | 保存当前/另存/全部保存，附落盘回执；另存为文件未变化或不可读时不再报成功 | WritesFile | `save_editor_scene_as`、`save_editor_scenes` |
| `editor_selection_set` | set / select_tree | 设置选择或按场景树行选中 | ModifiesWindow | `set_editor_selection`、`select_scene_tree_node` |
| `editor_undo` | create / add_do / add_undo / commit / undo / redo / history / clear | 撤销栈四件套与全局 undo/redo/历史 | ModifiesWindow | `create_editor_undo_redo_action`、`add_editor_undo_redo_do`、`add_editor_undo_redo_undo`、`commit_editor_undo_redo` |
| `script_editor` | open / goto / list_open / current / unsaved / save_all / reload_open / close | 脚本编辑器打开、跳行、列表、保存、重载、关闭 | ModifiesWindow | — |
| `editor_session` | get / mark_unsaved / play_main / play_custom | 读多签会话、标脏、播放主场景/指定场景 | ModifiesWindow | — |
| `editor_inspect` | inspect_object / edit_resource / edit_node / selected_path / expand_all / collapse_all | Inspector 检查对象/资源/节点与折叠 | ModifiesWindow | `inspect_editor_resource` |
| `editor_files` | select / selected_paths / current_path / current_dir / navigate / file_meta | FileSystem 停靠面板定位与元信息 | ModifiesWindow | — |
| `editor_info` | theme / scale / language / paths / multi_window / distraction_free / toast | 读编辑器主题/缩放/语言/路径并推 toast | ShowsAlert | — |
| `editor_ui_action` | click / type / shortcut | 按元素 path 点击、输入文本、注入快捷键 | ModifiesWindow | `click_editor_element`、`type_editor_element_text`、`run_editor_shortcut` |
| `editor_config` | main_scene / plugin_enable | 设置主场景、启停插件 | WritesConfig | `set_editor_main_scene`、`set_editor_plugin_enabled` |
| `build_csharp_assembly` | — | `dotnet build` 构建 C# 工程，返回构建结果与后续动作；改为有界 deferred，先回 `__gda_wait` 再解析结果，digest 与错误行截断上报 | Process | — |
| `dotnet_environment` | — | 探测 .NET/C# 环境，不触发构建 | Process | — |

## Analysis（1）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `analyze_project` | validate_scene / unused_resources / signal_flow | 只读分析：场景可加载性/依赖、未使用资源、信号流 | None | `validate_scene_file`、`find_unused_resources`、`trace_signal_flow` |

## Capture（2）

| 工具 | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|
| `capture_editor_viewport` | 截图（编辑器视口或 `target=game` 游戏视口），支持区域/缩放/标注/差分/落盘 | WritesFile | `capture_game_viewport` |
| `review_scene_visually` | 一次返回编辑器截图、游戏截图、节点屏坐标表与视口映射；game 子调用返回 pending 时自动等待并回传，异常响应附截断诊断 | WritesFile | — |

## 调用示例

```json
{"name":"edit_scene_node","arguments":{"action":"rename","path":"Player/Sprite2D","new_name":"Hero"}}
{"name":"create_scene_node","arguments":{"mode":"scene","path":"res://scenes/Enemy.tscn","editable_instance":true}}
{"name":"physics_query","arguments":{"dimension":3,"mode":"intersect_ray","space_rid":7,"from":{"x":0,"y":1,"z":0},"to":{"x":0,"y":1,"z":-10}}}
{"name":"resource_create","arguments":{"action":"curve","name":"enemy_curve"}}
{"name":"editor_undo","arguments":{"action":"history"}}
```

旧名迁移示例：`rename_scene_node` → `edit_scene_node` + `action=rename`；`get_scene_node_screen_rect` → `get_scene_tree` + `paths`；`intersect_physics_3d_ray` → `physics_query` + `dimension=3, mode=intersect_ray`；`load_resource` → `resource_load` + `action=load`；`play_editor_current_scene` → `editor_scene_control` + `action=play`。所有旧名调用在别名窗口内仍可用，返回顶层附弃用字段。

## 相关页面

- 别名契约与发展窗口：[tool_surface_conventions.md](../tool_surface_conventions.md)
- 最终审计与按域/副作用分布：[tool_surface_report.md](../tool_surface_report.md)
- 注册管线：[tools_registry.md](tools_registry.md)
- 运行时桥接：[entry_runtime.md](entry_runtime.md)
- 入口：[../index.md](../index.md) ｜ 总览：[../overview.md](../overview.md) ｜ 测试：[../tests.md](../tests.md)
