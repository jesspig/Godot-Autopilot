---
type: 模块文档
title: 工具实现模块（B 组）
description: B 组 20 个域、113 个 canonical 工具（调试/显示/OS/运行时/音频/渲染/文本/瓦片/动画/主题/图像/网格等）的 action 表与别名归属
tags:
  - 模块
  - 领域工具
  - B组
  - action
timestamp: "2026-11-23T17:28:29+08:00"
resource: src/tools/domains/
---

# 工具实现模块 B（20 域 113 工具）

> 审计日期：2026-11-23（增量同步）。数值与 A 组同源：catalog 252 = 246 域 + 5 元 + `system_status` 1（35 个域文件），生产别名 325、schema 235/17、能力矩阵 561 行缺失 0，见 [tool_surface_report.md](../tool_surface_report.md)。本轮工具名、域数与 action 表未变；`run_gdscript_tests` 副作用由 None 改为 CodeExecute（catalog None 151→150、CodeExecute 5→6，高风险档 26→27）。本页覆盖 20 个域、113 个 canonical 工具；A 组见 [tools_ops_a.md](tools_ops_a.md)（133 个），133 + 113 = 246。

## 口径与别名窗口

- 第一列是 canonical 工具名；旧名只出现在“旧名（别名）”列，不占 catalog、不进 `search_tools`，仅经 `call_tool` / `batch_execute` 解析。
- 别名统一 `deprecated_since="0.4.0"`、`removal_version="0.6.0"`；命中时结果顶层附 `deprecated_alias`、`canonical_name`、`alias_removal_version`、`deprecated_since`，契约见 [tool_surface_conventions.md](../tool_surface_conventions.md)。
- 审计的 325 条别名定义含 8 条与实名工具重名（`create_render_decal`、`create_render_material`、`create_render_reflection_probe`、`create_render_shader`、`create_render_sky`、`get_particles_state`、`restart_particles`、`set_particles_params`），按冲突策略实名工具优先、别名被遮蔽；这些名字是本页 Render 的 canonical 工具，不再列入别名列，故表内别名条目数少于 325。
- action 工具的动作枚举由 schema 给出，action 专用参数为可选、由 handler 按动作校验；每个工具至少一条示例在 `ToolSpec.examples`，经 `get_tool_detail` 读取。
- `game_*` / `game_scene_tree` / `game_tween` 等运行时工具需游戏由编辑器启动且加载 godot-autopilot 扩展，并受 `game_runtime` 高风险档 `confirm=true` 约束；Renderer 工具全程使用 RID 整数句柄；Image/Mesh 产物默认留在内存并可注册为 `memory://<name>`。

## Debug（6）

| 工具 | action | 说明 | 副作用 | 旧名（别名，0.4.0 弃用 → 0.6.0 移除） |
|---|---|---|---|---|
| `print_debug_log` | — | 经插件日志系统写 Info 级调试消息 | None | — |
| `get_debug_stack` | — | 捕获编辑器进程全部运行脚本的回溯 | None | — |
| `set_debug_physics_fps` | — | 全局设置物理帧率 | ModifiesWindow | — |
| `set_debug_visuals` | collision / navigation / performance | 开关编辑器运行游戏的调试叠加 | WritesConfig | `set_debug_collision_visual`、`set_debug_navigation_visual`、`set_debug_performance_visual` |
| `get_debug_info` | monitors / catalog / custom / object / node / memory | 读性能监控、监控目录、自定义监控、对象/节点/内存计数 | None | `get_debug_monitors`、`get_debug_monitor`、`get_debug_monitor_catalog`、`get_debug_custom_monitor`、`get_debug_custom_monitor_names`、`get_debug_object_count`、`get_debug_node_count`、`get_debug_memory_usage` |
| `remove_debug_custom_monitor` | — | 按字符串 id 删除自定义性能监控 | Destructive | — |

## Debugger（4）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `get_debugger_stream` | log / output / errors / session | 读调试器日志/输出/错误/会话信息 | None | `get_debugger_log`、`get_debugger_output`、`get_debugger_errors`、`get_debugger_session_info` |
| `debugger_breakpoints` | list / set / clear | 列出、设置、清除脚本断点 | ModifiesWindow | — |
| `get_plugin_log` | — | 读插件进程内 LogSystem 环形缓冲（授权拒绝、超时、迟到响应等诊断） | None | — |
| `get_debugger_scene_tree` | — | 经运行时通道读运行中游戏的场景树 | None | — |

## Display（8）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `get_display_info` | screens / clipboard / mouse / voices / window | 读屏幕、剪贴板、鼠标、TTS 音色、窗口矩形 | None | `get_display_screen_count`、`get_display_screen_dpi`、`get_display_screen_position`、`get_display_screen_refresh_rate`、`get_display_screen_size`、`get_display_clipboard`、`get_display_mouse_position`、`get_display_tts_voices`、`get_display_window_rect` |
| `set_display_clipboard` | — | 写系统剪贴板 | ModifiesWindow | — |
| `show_display_dialog` | — | 弹系统原生模态对话框 | ShowsAlert | — |
| `capture_display_screen` | — | 截取物理屏幕 PNG | None | — |
| `display_window` | create / foreground / attention / flag / mode / position / size / title / popup / popup_centered / transient / exclusive / unparent_when_invisible / close | 创建或修改子窗口（弹出、置前、标志、模式、位置、尺寸、标题） | ModifiesWindow | `create_display_window`、`move_display_window_to_foreground`、`request_display_window_attention`、`set_display_window_flag`、`set_display_window_mode`、`set_display_window_position`、`set_display_window_size`、`set_display_window_title` |
| `display_mouse` | mode / warp | 设置鼠标模式或把光标移动到客户区坐标 | ModifiesWindow | `set_display_mouse_mode`、`warp_display_mouse` |
| `display_tts` | speak / stop / pause / resume / is_speaking | 系统 TTS 朗读控制 | ShowsAlert | `speak_display_tts`、`stop_display_tts` |
| `delete_display_window` | — | 关闭并释放 `display_window action=create` 创建的窗口 | Destructive | — |

## OS（12）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `os_process` | create / execute / kill / status / exit_code / is_running | 外部进程创建、执行、终止与状态；超时未确认终止时回 `termination_confirmed=false` 并提示进程可能仍在运行 | Process | `create_os_process`、`execute_os_process`、`kill_os_process` |
| `os_environment` | get / set / has | 读/写/检查编辑器进程环境变量（set 仅会话内） | Process | `get_os_environment`、`set_os_environment` |
| `os_time` | datetime / unix_time | 系统日期时间与 Unix 时间戳 | None | `get_os_datetime`、`get_os_unix_time` |
| `os_system_info` | info / locale / fonts / memory_info / static_memory / video_adapter / cmdline_args | 宿主系统、语言、字体、内存、显卡与命令行信息 | None | `get_os_system_info`、`get_os_locale`、`get_os_system_fonts` |
| `get_os_unique_id` | — | 读机器唯一标识 | Process | — |
| `get_os_user_data_dir` | — | 读用户数据目录 | Process | — |
| `os_file_text` | read / find | 纯文件 I/O 读文本与递归搜索（>2 MiB 报 `file_too_large`） | None | `read_file`、`find_in_files` |
| `os_file_write` | write / copy / ensure / trash | 写/追加文本、字节复制、幂等建目录、移入回收站；脚本写入附诊断；trash 需 `confirm=true`，否则回 `confirmation_required` | WritesFile | `write_file`、`copy_file`、`ensure_directory`、`move_os_file_to_trash` |
| `os_file_manager` | list / info | 只读目录列举与文件元信息（大小、mtime、sha256/md5） | None | `list_directory`、`get_file_info` |
| `create_csharp_script` | — | 预校验后创建 C# 脚本文件 | WritesFile | — |
| `open_os_path` | — | 用默认应用打开 URL 或路径 | Process | — |
| `show_os_alert` | — | 让编辑器向用户弹提示 | ShowsAlert | — |

## Game（9）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `get_game_state` | status / session / input / job | 读游戏状态、调试会话、输入状态与异步 job | None | `get_game_status`、`get_game_input_status`、`get_game_job` |
| `get_game_ui` | elements | 枚举运行中游戏的 Control 树（含窗口坐标与 `mouse_filter`） | None | `get_game_ui_elements` |
| `get_game_evidence` | sample / collect / validate | 采样属性、收集证据、校验 UI 布局（只读） | None | `sample_game_property`、`collect_game_evidence`、`validate_game_ui_layout` |
| `game_script` | execute / start_job | 在游戏进程内执行 GDScript（可断言）或提交长脚本 job | GameRuntime | `execute_game_script`、`start_game_job` |
| `game_input` | queue / wait / sequence | 游戏内排队输入、等待输入状态、按物理帧注入序列 | GameRuntime | `queue_game_input`、`wait_game_input`、`sequence_game_inputs` |
| `click_game_ui_element` | — | 按节点路径或可见文本在游戏内枚举控件并点击（含 hover 刷新与命中回传） | GameRuntime | — |
| `reload_game_scripts` | — | 软重载游戏脚本（保持实例状态） | GameRuntime | — |
| `set_game_locale` | — | 设置游戏 TranslationServer 语言 | GameRuntime | — |
| `get_game_localization` | locale / list / translate | 读游戏语言、翻译表与翻译结果 | None | `get_game_locale`、`get_translation_list`、`translate_text` |

## Audio（5）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `audio_bus` | get_layout / set_layout / get_info / info / set_volume / set_mute / set_solo / set_bypass / add_effect | 总线布局、信息、音量/静音/独奏/旁路与效果添加 | None | `get_audio_bus_layout`、`set_audio_bus_layout`、`get_audio_bus_count`、`get_audio_bus_name`、`set_audio_bus_volume_db`、`set_audio_bus_mute`、`set_audio_bus_solo`、`set_audio_bus_bypass_effects`、`add_audio_bus_effect` |
| `audio_player` | play / stop / set_volume / set_pitch / seek / get_position | 三种 AudioStreamPlayer 的播放、停止、音量、音高、定位；`stream_path` 仅接受项目内 `res://` 文件 | None | `play_audio_player`、`stop_audio_player`、`set_audio_player_volume_db`、`set_audio_player_pitch_scale`、`seek_audio_player`、`get_audio_player_playback_position` |
| `audio_device` | list_outputs / set_output / list_inputs / set_input / get_input_status | 输出/输入设备列举与切换 | None | `get_audio_device_outputs`、`set_audio_device_output`、`get_audio_device_inputs`、`get_audio_input_devices`、`set_audio_device_input` |
| `audio_capture` | start / stop / create_wav | 输入捕获启停并落为 `memory://` WAV；或由内联样本建 WAV | None | `start_audio_input_capture`、`stop_audio_input_capture`、`create_audiostream_wav` |
| `remove_audio_bus_effect` | — | 按总线内效果下标删除效果 | Destructive | — |

## Render（29）

| 工具 | action | 说明 | 旧名（别名） |
|---|---|---|---|
| `render_environment` | create / set_params / set_sky / get | 创建并配置 Environment RID（bg/ambient/glow/ssr/tonemap/sdfgi/volumetric_fog 分组） | `set_render_environment_bg_color`、`set_render_environment_ambient_light`、`set_render_environment_glow`、`set_render_environment_ssr`、`set_render_environment_tonemap`、`set_render_environment_sdfgi`、`set_render_environment_volumetric_fog` |
| `render_instance` | create / attach / set_transform / set_visible / set_layer_mask / free | RenderingServer 实例创建、挂场景、变换、可见与层掩码 | `set_render_instance_visible`、`set_render_instance_layer_mask` |
| `render_viewport` | create / attach_camera / set_scenario / attach_canvas / attach_to_screen / set_size / set_clear_mode / capture_texture | 服务端视口创建、接线、尺寸/清屏与纹理截取 | `create_render_viewport`、`set_render_viewport_size`、`set_render_viewport_clear_mode` |
| `render_camera` | create / transform / perspective / orthogonal | 服务端 3D 相机创建与配置 | `create_render_camera`、`set_render_camera_transform`、`set_render_camera_perspective`、`set_render_camera_orthogonal` |
| `render_canvas_item` | create / add_rect / add_circle / add_line / add_texture_rect / add_polygon / set_transform / set_visible | 服务端 2D canvas item 绘制 | `create_render_canvas_item`、`add_render_canvas_item_rect`、`add_render_canvas_item_circle`、`add_render_canvas_item_line`、`add_render_canvas_item_texture_rect`、`set_render_canvas_item_transform`、`set_render_canvas_item_visible` |
| `render_light` | create / set_param / set_color | 服务端灯光创建与参数/颜色 | `create_render_light`、`set_render_light_param`、`set_render_light_color` |
| `render_particles` | create / set_emitting / restart / set_lifetime / set_params / get_state | 服务端粒子或编辑场景粒子节点的创建与控制 | `create_render_particles`、`set_render_particles_emitting`、`restart_render_particles`、`set_render_particles_lifetime` |
| `render_material` | create / set_param | 服务端材质创建与参数 | `set_render_material_param` |
| `render_shader` | create / set_code / set_global_param | 服务端着色器创建、改代码、全局参数 | `set_render_shader_code`、`set_render_shader_parameter_global` |
| `render_sky` | create / set_material | 服务端天空创建与材质 | `set_render_sky_material` |
| `render_fog_volume` | create / set_shape | 服务端雾体积创建与形状 | `create_render_fog_volume`、`set_render_fog_volume_shape` |
| `render_decal` | create | 服务端贴花创建 | — |
| `render_reflection_probe` | create | 服务端反射探针创建 | — |
| `render_mesh` | create / add_surface / set_surface_material | 服务端网格创建、加表面、表面材质 | `create_render_mesh`、`add_render_mesh_surface`、`set_render_mesh_surface_material` |
| `render_texture` | create_from_image | 从图像文件建服务端纹理 | `create_render_texture_from_image` |
| `render_inspect` | material_get_param / shader_get_code / shader_get_param_list / mesh_surface_get_arrays / texture_2d_get / global_shader_parameter_get / global_shader_parameter_list / global_shader_parameter_remove | 只读回读渲染对象 | — |
| `free_render_rid` | — | 释放任意 RenderingServer RID（需 `confirm=true`） | — |
| `create_render_scenario` | — | 创建 3D 场景 RID | — |
| `set_render_scenario_environment` | — | 把 Environment 挂到 3D 场景 | — |
| `get_render_canvas_item_rid` | — | CanvasItem 节点 → RID | — |
| `find_render_node_from_rid` | — | RID → 使用它的编辑场景节点 | — |
| `create_render_material` / `create_render_shader` / `create_render_sky` / `create_render_reflection_probe` / `create_render_decal` | — | 单步创建对应服务端 RID（action 工具的补充入口） | — |
| `restart_particles` / `set_particles_params` / `get_particles_state` | — | 编辑场景粒子节点重启/参数/状态 | — |

> 副作用：`render_*` 的大部分为 None；`free_render_rid` 为 Destructive，`restart_particles` / `set_particles_params` 为 WritesScene。名称为 `create_*` 的补充入口与 action 工具并存，便于单步调用与旧客户端迁移。

## Text（3）

| 工具 | action | 说明 | 旧名（别名） |
|---|---|---|---|
| `font` | create / set_data / set_antialiasing / set_hinting / system_path | 创建/配置 TextServer 字体对象、取系统字体路径 | `create_text_font`、`set_text_font_data`、`set_text_font_antialiasing`、`set_text_font_hinting`、`get_text_font_system_path` |
| `shaped_text` | create / add_string / get_size | 创建/追加/测量 shaped text | `create_shaped_text`、`add_shaped_text_string`、`get_shaped_text_size` |
| `text_query` | has_feature / locale_right_to_left | 查询 TextServer 能力与 RTL | `has_text_feature`、`is_text_locale_right_to_left` |

## TileMap（3）

| 工具 | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|
| `create_tilemap` | 建 TileMap 节点并附默认 TileSet | WritesScene | — |
| `set_tilemap_cells` | 单格/批量/矩形填充三种形式写 TileMap 或 TileMapLayer | WritesScene | `set_tilemap_cell`、`fill_tilemap_rect` |
| `create_tilemap_tileset` | 建内存 TileSet 并注册 `memory://<name>` | None | — |

## Tileset（1）

| 工具 | action | 说明 | 旧名（别名） |
|---|---|---|---|
| `update_tileset` | add_atlas_source / add_physics_layer / set_tile_collision | 给 `memory://` TileSet 加 atlas 源、物理层与格子碰撞；写碰撞前校验物理层存在，atlas 纹理路径 containment 与网格上限 | `add_tilemap_atlas_source`、`add_tilemap_physics_layer`、`set_tilemap_tile_collision` |

## GridMap（4）

| 工具 | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|
| `create_gridmap` | 建 GridMap 节点（可选 cell_size） | WritesScene | — |
| `create_meshlibrary_item` | 向 `memory://` MeshLibrary 加条目 | None | — |
| `set_gridmap_cells` | 单格/轴对齐盒填充/批量写格子 | WritesScene | `set_gridmap_cell`、`fill_gridmap_box` |
| `get_gridmap_used_cells` | 列出已用格子 | None | — |

## SpriteFrames（1）

| 工具 | action | 说明 | 旧名（别名） |
|---|---|---|---|
| `update_spriteframes` | create / add_animation / add_frame | 建 `memory://` SpriteFrames、加动画、加帧（支持 hframes/vframes 切 AtlasTexture）；hframes×vframes 上限 10000（防 int64 溢出），纹理路径 containment | `create_spriteframes`、`add_spriteframes_animation`、`add_spriteframes_frame` |

## Animation（10）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `create_scene_animation_player` | — | 在编辑场景建 AnimationPlayer 节点 | WritesScene | — |
| `create_animation` | — | 在默认动画库创建 Animation | WritesScene | — |
| `remove_animation` | — | 删除指定动画（跨库搜索） | Destructive | — |
| `get_animation_list` | list / inspect / library | 列/检视动画与动画库 | WritesScene | — |
| `create_animation_track` | — | 加 value/method 轨道 | WritesScene | — |
| `insert_animation_keyframe` | — | 在轨道插入关键帧 | WritesScene | — |
| `remove_animation_track` | — | 删除整条轨道 | Destructive | — |
| `set_animation_track_bundle` | — | 一轨及其全部键一次原子提交 | WritesScene | — |
| `animation_runtime` | play / pause / stop / seek / play_backwards / set_speed / queue / current / get_parameter / set_parameter | 编辑器时驱动 AnimationPlayer 或读写 AnimationTree 参数（不落场景） | ModifiesWindow | — |
| `animation_tree` | create / inspect / add_node / remove_node / rename_node / connect / set_transition / remove_transition | 建 AnimationTree、状态机节点与过渡 | WritesScene | `create_scene_animation_tree`、`add_animation_machine_state`、`connect_animation_states` |

## Theme（3）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `get_theme_info` | — | 汇总 `.tres` 主题的类型与条目名单 | None | — |
| `update_theme` | create / bundle | 创建或更新磁盘 Theme（写出后 IGNORE 重载校验）；加载路径 containment 校验 | WritesFile | `create_theme_resource`、`set_theme_item_bundle`、`set_theme_color`、`set_theme_constant`、`set_theme_font_size`、`set_theme_stylebox_flat` |
| `update_control_layout` | apply / layout | 对 Control 应用主题、锚点预设或组合布局（单 undo 步） | WritesScene | `apply_theme_to_control`、`set_control_anchor_preset`、`set_control_layout` |

## Tween（3）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `game_tween` | create / control / callback / method / interval | 游戏内创建/控制补间步骤 | GameRuntime | `create_game_tween`、`control_game_tween` |
| `get_game_tween_list` | — | 列出游戏内活跃补间（只读） | None | — |
| `tween_editor_property` | property / callback / method / interval | 在编辑场景预览补间步骤；`target_value` 按属性当前类型反序列化 | WritesScene | — |

## Log（1）

| 工具 | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|
| `get_game_log_entries` | 读 `user://logs/godot.log` 尾部（可按 `filter` 扩大扫描窗） | None | — |

## Image（4，新增域）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `image_edit` | create / load / info / resize / crop / flip / rotate / fill | 内存 Image 创建、加载与编辑 | None | — |
| `image_pixel` | blit / blend / get_pixel / set_pixel / compress / decompress / to_texture | 像素读写、混合、压缩与转纹理 | None | — |
| `image_save` | save | 把内存 Image 存为 res:// 下 PNG | WritesFile | — |
| `texture_gen` | noise / gradient / noise_texture / gradient_texture | 生成噪声/渐变图像与纹理 | None | — |

## Mesh（4，新增域）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `mesh_surface` | surface_begin / add_vertex / add_triangle_fan / set_uv / set_normal / set_color / set_material / commit | SurfaceTool 会话构建 ArrayMesh，提交后注册 `memory://name` | None | — |
| `mesh_inspect` | inspect / surface_arrays / faces / create_trimesh_shape / create_convex_shape / csg_bake / free | 检视网格、派生碰撞形状、烘焙 CSG | None | — |
| `multimesh` | create / set_transforms / set_colors / set_buffer / info | 创建/填充内存 MultiMesh | None | — |
| `multimesh_instance` | assign_to_instance | 把内存 MultiMesh 挂到 MultiMeshInstance3D | WritesScene | — |

## Testing（1）

| 工具 | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|
| `run_gdscript_tests` | 运行内置断言框架的 GDScript 测试并返回结构化报告；`directory` 仅接受项目内 `res://` 目录 | CodeExecute | `run_gdscript_test_files` |

## C#（2）

| 工具 | action | 说明 | 副作用 | 旧名（别名） |
|---|---|---|---|---|
| `csharp_member` | get / set / call / list | 经冻结的 GdaCSharpBridge ABI 读写/调用/列出 C# 成员；调用前校验 holder 的 `bridge` meta 存在且非 null | CodeExecute | `csharp_get_property`、`csharp_set_property`、`csharp_call_method`、`csharp_list_members` |
| `csharp_event` | subscribe / unsubscribe | 订阅/退订 C# 事件 | CodeExecute | `csharp_subscribe_event`、`csharp_unsubscribe_event` |

## 调用示例

```json
{"name":"audio_bus","arguments":{"action":"set_volume","bus_index":1,"volume_db":-6.0}}
{"name":"render_environment","arguments":{"action":"set_params","environment_rid":7,"params":{"glow":{"enabled":true}}}}
{"name":"image_edit","arguments":{"action":"load","path":"res://icon.svg","name":"icon"}}
{"name":"mesh_surface","arguments":{"action":"surface_begin","name":"quad"}}
{"name":"game_script","arguments":{"action":"start_job","source_code":"return get_tree().get_node_count()"}}
{"name":"debugger_breakpoints","arguments":{"action":"list"}}
```

旧名迁移示例：`play_audio_player` → `audio_player` + `action=play`；`create_render_camera` → `render_camera` + `action=create`；`create_game_tween` → `game_tween` + `action=create`；`get_debug_monitor` → `get_debug_info` + `action=monitors`；`get_game_status` → `get_game_state` + `action=status`；`write_file` → `os_file_write` + `action=write`。别名窗口内旧调用仍可用，结果附弃用字段。

## 相关页面

- 别名契约与窗口：[tool_surface_conventions.md](../tool_surface_conventions.md)
- 最终审计：[tool_surface_report.md](../tool_surface_report.md)
- A 组工具表：[tools_ops_a.md](tools_ops_a.md)
- 注册管线：[tools_registry.md](tools_registry.md)
- 运行时桥接：[entry_runtime.md](entry_runtime.md)
- 入口：[../index.md](../index.md) ｜ 总览：[../overview.md](../overview.md) ｜ 测试：[../tests.md](../tests.md)
