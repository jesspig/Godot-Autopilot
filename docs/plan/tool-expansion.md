# 工具扩展计划

> **版本**: 0.1.0 | **更新**: 2026-07-29
>
> **摘要**: 基于 Godot 引擎源码分析（`C:\Users\jessp\Documents\Code\GitHub\godot`），发现当前工具集仅覆盖约 30% 的 Server 层 API。本节列出全部 ~108 个计划新增工具的扩展方案，按优先级和引擎源码映射组织。
>
> **关联文档**:
> - [tool-catalog.md](tool-catalog.md) — 当前已有工具清单
> - [tool-discovery.md](tool-discovery.md) — 新工具如何被发现
> - [execution-engine.md](execution-engine.md) — 执行引擎如何编排新工具
> - [implementation-plan.md](implementation-plan.md) — 实施任务分解

---

## 1. 覆盖度分析

| 引擎子系统 | 当前工具数 | 计划工具数 | 引擎源码路径 | 优先级 |
|-----------|-----------|-----------|-------------|--------|
| DisplayServer | 0 | 24 | `servers/display/display_server.h` | P0 |
| OS | 0 | 15 | `core/os/os.h` | P1 |
| RenderingServer | 29 | 50 | `servers/rendering/rendering_server.h` | P1 |
| PhysicsServer | 36 | 46 | `servers/physics_*d/` | P1 |
| SceneTree | 3 | 11 | `scene/main/scene_tree.h` | P2 |
| InputMap | 0 | 8 | `core/input/input_map.h` | P2 |
| AudioServer | 15 | 20 | `servers/audio/audio_server.h` | P2 |
| TextServer | 0 | 10 | `servers/text/text_server.h` | P2 |
| Debug/Performance | 15 | 23 | `main/performance.h` | P2 |

所有新工具的处理函数遵循与现有工具相同的签名：`mcp::JsonValue(const mcp::JsonValue& args)`。

---

## 2. DisplayServer 扩展（P0，24 个）

源码：`godot/servers/display/display_server.h`（~240 个公有方法，选择常用非废弃 API）

| 工具名 | 引擎方法 | 描述 | 参数 |
|--------|---------|------|------|
| `display_window_create` | `create_sub_window()` | 创建子窗口 | mode, vsync_mode, rect |
| `display_window_delete` | `delete_sub_window()` | 删除子窗口 | window_id |
| `display_window_set_title` | `window_set_title()` | 设置窗口标题 | window_id, title |
| `display_window_set_size` | `window_set_size()` | 设置窗口尺寸 | window_id, width, height |
| `display_window_set_position` | `window_set_position()` | 设置窗口位置 | window_id, x, y |
| `display_window_set_mode` | `window_set_mode()` | 设置窗口模式（全屏/窗口/最小化） | window_id, mode |
| `display_window_set_flag` | `window_set_flag()` | 设置窗口标志（无边框/置顶/透明） | window_id, flag, enabled |
| `display_window_move_to_foreground` | `window_move_to_foreground()` | 窗口置前 | window_id |
| `display_window_request_attention` | `window_request_attention()` | 闪烁任务栏 | window_id |
| `display_screen_get_count` | `get_screen_count()` | 获取屏幕数量 | 无 |
| `display_screen_get_size` | `screen_get_size()` | 获取屏幕分辨率 | screen_id |
| `display_screen_get_position` | `screen_get_position()` | 获取屏幕位置 | screen_id |
| `display_screen_get_dpi` | `screen_get_dpi()` | 获取屏幕 DPI | screen_id |
| `display_screen_get_refresh_rate` | `screen_get_refresh_rate()` | 获取刷新率 | screen_id |
| `display_mouse_get_position` | `mouse_get_position()` | 获取鼠标位置 | 无 |
| `display_mouse_set_mode` | `mouse_set_mode()` | 设置鼠标模式（捕获/隐藏/可见） | mode |
| `display_mouse_warp` | `warp_mouse()` | 鼠标跳转 | x, y |
| `display_clipboard_set` | `clipboard_set()` | 设置剪贴板内容 | text |
| `display_clipboard_get` | `clipboard_get()` | 读取剪贴板内容 | 无 |
| `display_tts_speak` | `tts_speak()` | 文本转语音 | text, voice, volume, pitch, rate |
| `display_tts_stop` | `tts_stop()` | 停止 TTS | 无 |
| `display_tts_get_voices` | `tts_get_voices()` | 获取语音列表 | 无 |
| `display_dialog_show` | `dialog_show()` | 显示原生对话框 | title, description, buttons |
| `display_screen_capture` | `screen_get_image()` | 屏幕截图 | screen_id |

---

## 3. OS 扩展（P1，15 个）

源码：`godot/core/os/os.h`（~50 个脚本可调用方法）

| 工具名 | 引擎方法 | 描述 | 参数 |
|--------|---------|------|------|
| `os_execute` | `execute()` | 阻塞执行外部命令 | path, arguments, output |
| `os_create_process` | `create_process()` | 非阻塞启动进程 | path, arguments |
| `os_kill` | `kill()` | 终止进程 | pid |
| `os_get_environment` | `get_environment()` | 获取环境变量 | variable |
| `os_set_environment` | `set_environment()` | 设置环境变量 | variable, value |
| `os_get_system_info` | 组合 `get_name/version/processor_count` | 获取系统信息 | 无 |
| `os_get_locale` | `get_locale()` | 获取语言区域 | 无 |
| `os_get_datetime` | `get_datetime()` | 获取系统时间 | utc |
| `os_get_unix_time` | `get_unix_time()` | 获取 Unix 时间戳 | 无 |
| `os_get_unique_id` | `get_unique_id()` | 获取机器唯一标识 | 无 |
| `os_get_user_data_dir` | `get_user_data_dir()` | 获取用户数据目录 | 无 |
| `os_shell_open` | `shell_open()` | 在默认应用中打开 URL/文件 | uri |
| `os_alert` | `alert()` | 模态弹窗 | text, title |
| `os_move_to_trash` | `move_to_trash()` | 移到回收站 | path |
| `os_get_system_fonts` | `get_system_fonts()` | 获取系统字体列表 | 无 |

---

## 4. RenderingServer 扩展（P1，20 个）

源码：`godot/servers/rendering/rendering_server.h`

| 工具名 | 引擎方法 | 描述 |
|--------|---------|------|
| `render_texture_create_2d` | `texture_2d_create()` | 创建 2D 纹理 |
| `render_shader_set_code` | `shader_set_code()` | 设置着色器源码 |
| `render_shader_get_parameter_list` | `shader_get_parameter_list()` | 获取着色器参数列表 |
| `render_environment_set_glow` | `environment_set_glow()` | 设置辉光效果 |
| `render_environment_set_ssr` | `environment_set_ssr()` | 屏幕空间反射 |
| `render_environment_set_tonemap` | `environment_set_tonemap()` | 色调映射 |
| `render_environment_set_sdfgi` | `environment_set_sdfgi()` | SDFGI 全局光照 |
| `render_environment_set_volumetric_fog` | `environment_set_volumetric_fog()` | 体积雾 |
| `render_sky_create` | `sky_create()` | 创建天空 |
| `render_sky_set_material` | `sky_set_material()` | 设置天空材质 |
| `render_particles_set_emitting` | `particles_set_emitting()` | 粒子发射控制 |
| `render_particles_restart` | `particles_restart()` | 重启粒子系统 |
| `render_particles_set_lifetime` | `particles_set_lifetime()` | 设置粒子生命周期 |
| `render_reflection_probe_create` | `reflection_probe_create()` | 创建反射探针 |
| `render_decal_create` | `decal_create()` | 创建贴花 |
| `render_fog_volume_set_shape` | `fog_volume_set_shape()` | 设置雾体积形状 |
| `render_instance_set_visible` | `instance_set_visible()` | 设置实例可见性 |
| `render_instance_set_layer_mask` | `instance_set_layer_mask()` | 设置实例层遮罩 |
| `render_global_shader_parameter_set` | `global_shader_parameter_set()` | 设置全局着色器参数 |

---

## 5. 其他 Server 扩展（P2）

### 5.1 SceneTree（8 个）

源码：`godot/scene/main/scene_tree.h`

| 工具名 | 引擎方法 | 描述 |
|--------|---------|------|
| `scene_tree_set_pause` | `set_pause()` | 暂停/恢复场景 |
| `scene_tree_is_paused` | `is_paused()` | 检查暂停状态 |
| `scene_tree_call_group` | `call_group()` | 调用群组方法 |
| `scene_tree_notify_group` | `notify_group()` | 通知群组 |
| `scene_tree_get_nodes_in_group` | `get_nodes_in_group()` | 获取群组节点列表 |
| `scene_tree_create_timer` | `create_timer()` | 创建计时器 |
| `scene_tree_reload_current_scene` | `reload_current_scene()` | 重新加载当前场景 |
| `scene_tree_set_debug_collisions` | `set_debug_collisions_hint()` | 调试碰撞可视化 |

### 5.2 InputMap（8 个）

源码：`godot/core/input/input_map.h`

| 工具名 | 引擎方法 | 描述 |
|--------|---------|------|
| `input_map_has_action` | `has_action()` | 检查动作是否存在 |
| `input_map_get_actions` | `get_actions()` | 获取全部动作 |
| `input_map_add_action` | `add_action()` | 添加新动作 |
| `input_map_erase_action` | `erase_action()` | 删除动作 |
| `input_map_action_add_event` | `action_add_event()` | 绑定按键到动作 |
| `input_map_action_erase_event` | `action_erase_event()` | 移除动作的按键绑定 |
| `input_map_action_set_deadzone` | `action_set_deadzone()` | 设置动作死区 |

### 5.3 AudioServer 补充（5 个）

| 工具名 | 引擎方法 | 描述 |
|--------|---------|------|
| `audio_bus_set_solo` | `set_bus_solo()` | 设置总线独奏 |
| `audio_get_output_device_list` | `get_output_device_list()` | 获取输出设备列表 |
| `audio_set_output_device` | `set_output_device()` | 设置输出设备 |
| `audio_get_input_device_list` | `get_input_device_list()` | 获取输入设备列表 |
| `audio_set_input_device` | `set_input_device()` | 设置输入设备 |

### 5.4 TextServer（10 个）

源码：`godot/servers/text/text_server.h`

| 工具名 | 引擎方法 | 描述 |
|--------|---------|------|
| `text_has_feature` | `has_feature()` | 检查文本功能支持 |
| `text_get_system_font_path` | `get_system_font_path()` | 获取系统字体路径 |
| `text_create_font` | `create_font()` | 创建字体对象 |
| `text_font_set_data` | `font_set_data()` | 设置字体数据 |
| `text_font_set_antialiasing` | `font_set_antialiasing()` | 抗锯齿设置 |
| `text_font_set_hinting` | `font_set_hinting()` | 字体微调设置 |
| `text_create_shaped_text` | `create_shaped_text()` | 创建塑形文本 |
| `text_shaped_text_add_string` | `shaped_text_add_string()` | 添加字符串到塑形文本 |
| `text_shaped_text_get_size` | `shaped_text_get_size()` | 获取塑形文本尺寸 |
| `text_is_locale_right_to_left` | `is_locale_right_to_left()` | 检查语言是否从右到左 |

### 5.5 Debug / Performance 补充（8 个）

源码：`godot/main/performance.h`

| 工具名 | 引擎方法 | 描述 |
|--------|---------|------|
| `debug_get_all_monitors` | `get_monitor()` + 遍历 | 获取全部监控指标 |
| `debug_add_custom_monitor` | `add_custom_monitor()` | 添加自定义监控 |
| `debug_remove_custom_monitor` | `remove_custom_monitor()` | 移除自定义监控 |
| `debug_get_custom_monitor` | `get_custom_monitor()` | 获取自定义监控值 |
| `debug_list_custom_monitors` | `get_custom_monitor_names()` | 列出自定义监控 |

### 5.6 PhysicsServer 补充（10 个）

| 工具名 | 引擎方法 | 描述 |
|--------|---------|------|
| `physics_3d_shape_create` | `shape_create()` | 创建碰撞形状 |
| `physics_3d_shape_set_data` | `shape_set_data()` | 设置形状数据 |
| `physics_3d_body_add_shape` | `body_add_shape()` | 添加形状到物理体 |
| `physics_3d_body_set_param` | `body_set_param()` | 设置物理体参数（摩擦/弹性/质量） |
| `physics_3d_area_set_param` | `area_set_param()` | 设置区域参数（重力/阻尼） |
| `physics_3d_space_set_param` | `space_set_param()` | 设置空间参数 |
| `physics_3d_joint_set_param` | `joint_set_param()` | 设置关节参数 |
| `physics_3d_soft_body_set_param` | `soft_body_set_param()` | 设置软体参数 |
| `physics_3d_area_set_transform` | `area_set_transform()` | 设置区域变换 |
| `physics_3d_body_set_transform` | `body_set_state()` | 设置物理体变换 |

---

## 6. 新文件清单

| 文件 | 包含工具数 | 引擎源 |
|------|-----------|--------|
| `src/tools/display_ops.hpp/.cpp` | 24 | `servers/display/display_server.h` |
| `src/tools/os_ops.hpp/.cpp` | 15 | `core/os/os.h` |
| `src/tools/scene_tree_ops.hpp/.cpp` | 8 | `scene/main/scene_tree.h` |
| `src/tools/input_map_ops.hpp/.cpp` | 8 | `core/input/input_map.h` |
| `src/tools/text_ops.hpp/.cpp` | 10 | `servers/text/text_server.h` |
| `src/tools/render_ops.hpp/.cpp`（扩充） | +20 | `servers/rendering/rendering_server.h` |
| `src/tools/debug_ops.hpp/.cpp`（扩充） | +8 | `main/performance.h` |
| `src/tools/physics_ops.hpp/.cpp`（扩充） | +10 | `servers/physics_*d/` |
| `src/tools/audio_ops.hpp/.cpp`（扩充） | +5 | `servers/audio/audio_server.h` |
