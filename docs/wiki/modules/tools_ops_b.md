# 工具实现模块 B（调试/显示/OS/运行时/渲染/资源/执行）

> 覆盖 `src/tools/` 下 18 个模块：debug_ops、debugger_ops、debugger_access、display_ops、display_window_ops、os_ops、runtime_ops、runtime_game_ops、audio_ops、render_ops、environment_ops、text_ops、tilemap_ops、tileset_ops、spriteframes_ops、code_exec_ops、log_ops、capture_ops。
>
> 工具总数：**170** = 168 个领域工具 + 2 个元工具（`batch_execute`、`code_execute`，经 `server.RegisterTool()` 直连，不经 `call_tool`）。数量以 `src/tools/tool_defs.def` 的 `TOOL_ENTRY` 与 `register_all.cpp` 为准。
>
> 相关页面：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## 结构说明

| 文件 | 命名空间 | 声明位置 | 工具数 |
|---|---|---|---|
| `debug_ops.cpp` | `godot_autopilot::debug_ops` | `debug_ops.hpp` | 23 |
| `debugger_ops.cpp` | `godot_autopilot::debugger_ops` | `debugger_ops.hpp` | 7 |
| `debugger_access.cpp` | `godot_autopilot`（自由函数） | `debugger_access.hpp` | 0 |
| `display_ops.cpp` | `godot_autopilot::display_ops` | `display_ops.hpp` | 15 |
| `display_window_ops.cpp` | `godot_autopilot::display_ops` | `display_ops.hpp`（无独立 hpp） | 9 |
| `os_ops.cpp` | `godot_autopilot::os_ops` | `os_ops.hpp` | 15 |
| `runtime_ops.cpp` | `godot_autopilot::runtime_ops` | `runtime_ops.hpp` | 0（基础设施） |
| `runtime_game_ops.cpp` | `godot_autopilot::runtime_ops` | `runtime_ops.hpp`（无独立 hpp） | 6 |
| `audio_ops.cpp` | `godot_autopilot::audio_ops` | `audio_ops.hpp` | 20 |
| `render_ops.cpp` | `godot_autopilot::render_ops` | `render_ops.hpp` | 43 |
| `environment_ops.cpp` | `godot_autopilot::render_ops` | `render_ops.hpp`（无独立 hpp） | 7 |
| `text_ops.cpp` | `godot_autopilot::text_ops` | `text_ops.hpp` | 11 |
| `tilemap_ops.cpp` | `godot_autopilot::tilemap_ops` | `tilemap_ops.hpp` | 4 |
| `tileset_ops.cpp` | `godot_autopilot::tileset_ops` | `tileset_ops.hpp` | 3 |
| `spriteframes_ops.cpp` | `godot_autopilot::spriteframes_ops` | `spriteframes_ops.hpp` | 3 |
| `code_exec_ops.cpp` | `godot_autopilot::code_exec_ops` | `code_exec_ops.hpp` | 2（元工具） |
| `log_ops.cpp` | `godot_autopilot::log_ops` | `log_ops.hpp` | 1 |
| `capture_ops.cpp` | `godot_autopilot::capture_ops` | `capture_ops.hpp` | 1 |

注：`environment_ops.cpp`、`display_window_ops.cpp`、`runtime_game_ops.cpp` 没有独立 hpp，函数声明复用 `render_ops.hpp`、`display_ops.hpp`、`runtime_ops.hpp`，与 AGENTS.md「四步添加工具」的「成对 hpp」描述存在偏差（见文末对照）。

## debug_ops — 引擎诊断与性能监控（23 工具）

- **职责**：编辑器侧引擎诊断：日志打印、脚本调用栈回溯、`Performance` 单例监控、FPS/物理帧率调节、编辑器调试可视化开关。
- **代表工具**：`debug_print`、`debug_print_stack`、`debug_get_performance_monitor`、`debug_list_performance_monitors`、`debug_get_all_monitors`、`debug_set_fps_limit`、`debug_set_physics_fps`、`debug_collision_debug`、`debug_navigation_debug`、`debug_performance_debug`、`debug_remove_custom_monitor`、`debug_get_custom_monitor`、`debug_list_custom_monitors`、`debug_query_object_count`、`debug_query_memory_usage`、`debug_query_node_count`、`debug_get_object_count`、`debug_get_memory_usage`。
- **关键事实**：
  - 内置 83 个性能监控项静态表（`s_monitors`：time/memory/quantity 三类），`debug_get_all_monitors` 按表序取 `Performance::get_monitor` 值。
  - `debug_print_stack` 用 `Engine::capture_script_backtraces(include_vars)` 取 `ScriptBacktrace`，可展开帧、全局/局部/成员变量名。
  - 5 个占位工具**必然返回 error**：`debug_get_object_count_by_class`、`debug_profile_start`、`debug_profile_stop`、`debug_profile_get_data`、`debug_add_custom_monitor`（godot-cpp 不可用，描述中明示）。
  - `debug_collision_debug` 等 3 个写 `EditorSettings::set_project_metadata("debug_options", ...)`。
- **错误模式**：缺失参数 → `{"error": "missing required parameter: ..."}`；单例缺失 → `{"error": "... not available"}`；monitor id 越界 → `{"error": "monitor id out of range: N"}`。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## debugger_ops — 游戏进程调试捕获与断点（7 工具 + 2 个注册类）

- **职责**：捕获编辑器/游戏进程的日志、错误、运行输出、调用栈、场景树、性能监视帧，通过编辑器调试会话（`EditorDebuggerPlugin`）与运行中游戏桥接。
- **代表工具**：`output_get_log`、`debugger_get_errors`、`debugger_get_output`、`debugger_get_stack_dump`、`debugger_get_scene_tree`、`debugger_get_monitors`、`debugger_get_session_info`。
- **关键事实**：
  - 注册两个 Godot 类（`debugger_ops::register_classes()` 经 `ClassDB::register_class`，由 `main.cpp` 调用）：`OutputCaptureLogger`（继承 `godot::Logger`，`_log_error`/`_log_message` 写入捕获缓冲）与 `DebugCapturePlugin`（继承 `EditorDebuggerPlugin`，单例存于静态 `s_instance`；`_setup_session` 登记会话、`_capture` 处理 `gda` 协议消息——`GDA_MSG_READY` 标记就绪、`GDA_MSG_RESPONSE` 转发给 `runtime_ops::handle_game_response`）。
  - 内存捕获环形缓冲（`DebuggerCapture` 单例，mutex 保护）：日志 2000 条、错误 500 条、运行输出 2000 条、监视帧 500 条。
  - 会话激活时 `debugger_get_errors`/`get_output`/`get_scene_tree` **切换为经 `runtime_ops::handle_gda_send` 走运行时通道**（get_errors/get_output/get_tree，超时 5000ms）；`get_stack_dump` 仅断点会话本地数据（附 note 说明无法经通道获取）。
  - `output_get_log` 命中 `Invalid access to property or key` 错误时追加 Godot 3→4 重命名提示（`RENAME_HINTS`：frames→sprite_frames、translation→position 等 9 条）。
  - 空结果附加 `capture_note_for_empty_result()`：提示启动 `editor_play_current_scene` 或改用 `log_get_game_entries`。
- **错误模式**：无会话场景返回 `result` 空串 + `note` 字段；会话内错误经通道原样返回 `{"error": ...}`。
- 出站链接：[运行时通道](../modules/entry_runtime.md) · [工具注册表](../modules/tools_registry.md)

## debugger_access — 运行时通道的调试桥（0 工具）

- **职责**：非工具自由函数接口层，供 `runtime_ops` 调用 `DebugCapturePlugin` 的会话能力，依赖方向无环：`runtime_ops.cpp → debugger_access.hpp`，`debugger_ops.cpp → runtime_ops.hpp`。
- **关键函数**：`debugger_capture_initialized()`（等价实例非空）、`debugger_broadcast_request(payload, &session_id)`（向全部 active 且 ready 会话广播 `gda:request`，返回是否至少发出）、`debugger_send_cancel`、`debugger_breaked_session_ids()`、`debugger_continue_session()`。
- 出站链接：[运行时通道](../modules/entry_runtime.md) · [工具注册表](../modules/tools_registry.md)

## display_ops + display_window_ops — 显示服务与窗口（24 工具）

- **职责**：`DisplayServer` 直通封装：剪贴板、对话框、鼠标、屏幕信息与截图、TTS 语音、子窗口创建与管理（`display_window_ops.cpp` 内 9 个窗口工具与 `display_ops.cpp` 共享 `display_ops` 命名空间）。
- **代表工具**：`display_clipboard_get`/`set`、`display_dialog_show`、`display_mouse_get_position`/`set_mode`/`warp`、`display_screen_capture`、`display_screen_get_count`/`dpi`/`position`/`refresh_rate`/`size`、`display_tts_get_voices`/`speak`/`stop`、`display_window_create`/`delete`/`set_flag`/`set_mode`/`set_position`/`set_size`/`set_title`/`move_to_foreground`/`request_attention`。
- **关键事实**：
  - `display_screen_capture` 经 `Image::save_png_to_buffer` → base64（`capture_ops::base64_encode`），PNG 编码失败时降级返回原始序列化 Image + warning。
  - `display_window_create` 用 `memnew(godot::Window)` 创建原生子窗口（需 `FEATURE_SUBWINDOWS`），返回 `window->get_window_id()`；`display_window_delete` 经 `instance_from_id` 取 Window 后 `queue_free`。
  - `display_mouse_set_mode` 钳制 mode 0-4；`display_dialog_show` 检查 `DisplayServer::dialog_show` 的返回 Error。
  - 14 个副作用工具在测试遍历中排除（见文末对照）。
- **错误模式**：`util::error_json`（"missing required parameter" / "DisplayServer not available" / "subwindows not supported on this platform"）。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## os_ops — 操作系统接口（15 工具，高副作用密度）

- **职责**：`OS`/`Time` 单例直通：弹窗、进程创建与执行、环境变量、系统信息、文件回收站、shell 打开。
- **代表工具**：`os_alert`、`os_create_process`、`os_execute`、`os_get_datetime`、`os_get_environment`、`os_get_locale`、`os_get_system_fonts`、`os_get_system_info`、`os_get_unique_id`、`os_get_unix_time`、`os_get_user_data_dir`、`os_kill`、`os_move_to_trash`、`os_set_environment`、`os_shell_open`。
- **关键事实**：
  - **7 个副作用工具全部进入测试排除清单**（`os_alert` 弹系统模态框；`os_execute`/`os_create_process` 拉起进程；`os_kill` 杀进程；`os_shell_open` 打开 URI；`os_move_to_trash` 删文件；`os_set_environment` 改环境）。
  - `os_execute` 可选 `output: true` 捕获 stdout/stderr（`OS::execute` 第四参 true，返回 exit_code + 拼接输出）。
  - 8 个只读查询工具（datetime/环境/系统信息/时间/字体/唯一 ID 等）不在排除清单，遍历会冒烟。
- **错误模式**：必填参数缺失 → error_json；单例缺失 → error_json；`os_kill`/`os_move_to_trash`/`os_shell_open` 返回 Error 枚举数值。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## runtime_ops + runtime_game_ops — 游戏运行时通道（6 工具 + 基础设施）

- **职责**：`game_*` 工具把请求经编辑器调试会话发送到运行中的游戏进程（gda 协议），等待响应；`runtime_ops.cpp` 提供队列注入、请求生命周期、超时取消、断点自动恢复、捕获文件回读。
- **代表工具**：`game_status`、`game_eval`、`game_input`、`game_input_wait`、`game_input_status`、`game_capture`。
- **关键事实**：
  - **`set_editor_queue(CommandQueue*)`**：`main.cpp` 注入 Godot 主线程队列（`get_editor_queue()` 全局访问）。`handle_gda_send` 非主线程时经 `queue.submit(...).get()` 投递；游戏响应 `handle_game_response` 由 `DebugCapturePlugin::_capture` 喂入，以 request_id 匹配 `PendingRequest`（mutex + condition_variable）并唤醒等待线程。
  - 请求 ID 原子自增；`extract_timeout` 钳制到 `GDA_MAX_TIMEOUT_MS=30000`（默认 `GDA_DEFAULT_TIMEOUT_MS=5000`，`src/core/config.hpp`）；`handle_gda_send` 返回中间态 `{"__gda_pending": id, "timeout_ms": ...}`，由 `register_all.cpp::meta_call_tool_wait` 转 `wait_pending_response` 轮询合并；`game_capture` 额外走 `finalize_capture_response`（主线程读文件 → base64）。
  - **超时**：超时后经 `queue.submit` 向会话发 `debugger_send_cancel`（GDA_OP_CANCEL），错误信息含 request_id 与诊断提示。
  - `game_status` 响应注入派生字段：`healthy`（由 `last_activity_ms` 推导）与 `physics_stalled`（physics_frame 连续不变 ≥1000ms 且 fps>0）。
  - **断点自动恢复** `maybe_recover_break`：对全部 breaked 会话按 `GDA_AUTO_CONTINUE` 环境变量决定是否 `debugger_continue_session`，每会话上限 `GDA_AUTO_CONTINUE_MAX` 次。
  - `game_input` 参数白名单（type/keycode/pressed/button_index/position/action/duration_ms/mode/timeout_ms），未知参数被忽略并回传 `ignored_params` + warning；`game_input_status` 成功时附带最近 5 条引擎错误文本。
  - `runtime_game_ops.cpp` 无独立 hpp，声明在 `runtime_ops.hpp`。
- **错误模式**：`error_json("game not ready: ...")`、超时错误（含 op 名/request_id/诊断）、`unexpected game response (missing result)`、late response 丢弃仅记 warning。
- 出站链接：[运行时通道](../modules/entry_runtime.md) · [工具注册表](../modules/tools_registry.md)

## audio_ops — 音频总线与播放（20 工具）

- **职责**：`AudioServer` 总线/效果/设备 + `AudioStreamPlayer`(1D/2D/3D) 播放控制。
- **代表工具**：`audio_bus_get_layout`、`audio_bus_set_layout`、`audio_bus_get_count`、`audio_bus_set_volume`、`audio_bus_set_mute`、`audio_bus_set_solo`、`audio_effect_add`、`audio_effect_remove`、`audio_stream_play`、`audio_stream_stop`、`audio_stream_set_volume`、`audio_stream_set_pitch`、`audio_stream_get_playback_position`、`audio_stream_seek`、`audio_get_output_device_list`、`audio_set_output_device`。
- **关键事实**：
  - 播放类工具经 `resolve_audio_player` 兼容三种节点；`audio_effect_add` 用 `ClassDBSingleton::instantiate` 按 effect_type 字符串建 `AudioEffect`。
  - `audio_bus_set_layout` 用 `VariantJson::deserialize` 回灌 `AudioBusLayout`；`audio_bus_get_layout` 序列化 `generate_bus_layout()`。
  - `audio_stream_play` 可选 stream_path 即时加载（`ResourceLoader` + FileAccess 存在性检查），无 stream 时提示先用资源工具设置。
  - 节点路径解析：`find_node` 相对/绝对/带根名三种形态，且限定在编辑场景内。
- **错误模式**：bus_index 越界 → `{"error": "audio bus not found at index: N"}`；加载失败用 `error_detail`（含原因/期望/建议）。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## render_ops + environment_ops — RenderingServer 直通（50 工具）

- **职责**：低层 `RenderingServer` RID 句柄封装：CanvasItem 绘制、相机/灯光/网格/材质/视口/粒子、环境后处理、雾、着色器、实例与全局参数；RID 解析。
- **代表工具**：`canvas_item_create`、`canvas_item_draw_rect`、`camera_create`、`light_create`、`mesh_create`、`material_create`、`viewport_create`、`particle_create`、`environment_set_bg_color`、`environment_set_ambient`、`render_environment_set_glow`、`render_environment_set_ssr`、`render_environment_set_tonemap`、`render_environment_set_sdfgi`、`render_environment_set_volumetric_fog`、`render_sky_create`、`render_shader_set_code`、`render_global_shader_parameter_set`、`resolve_rid`。
- **关键事实**：
  - 全程 RID 数值句柄（`rid_from_json`/`rid_to_json`，经 `UtilityFunctions::rid_from_int64` 互转），不绑定场景节点。
  - **`environment_ops.cpp` 无独立 hpp**：7 个环境后处理工具与 `render_ops.cpp` 共享 `render_ops` 命名空间，声明在 `render_ops.hpp`；辅助函数 `rid_from_json`/`parse_color` 两文件各持一份（注释明示"保持两处同步"）。
  - `environment_set_*` 参数繁多带默认值（glow 13 参、sdfgi 11 参、volumetric_fog 13 参）；环境工具用 `LogLevel::Debug` 记完成日志，其余为 Info。
  - `resolve_rid` 与 `canvas_item_get_rid` 桥接场景节点与 RID（`EditorInterface` 参与）。
- **错误模式**：必填 `environment_rid`/RID 参数缺失 → error_json；`RenderingServer not available` → error_json；无 `result` 之外的扩展字段。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## text_ops — 文本服务与字形（11 工具，含 file_write）

- **职责**：`TextServer` 直通：字体资源、shaped text、系统字体路径、RTL 检测；另含一个通用文件写入工具。
- **代表工具**：`text_create_font`、`text_create_shaped_text`、`text_font_set_antialiasing`、`text_font_set_data`、`text_font_set_hinting`、`text_get_system_font_path`、`text_has_feature`、`text_is_locale_right_to_left`、`text_shaped_text_add_string`、`text_shaped_text_get_size`、`file_write`。
- **关键事实**：
  - `text_font_set_data` 用 `FileAccess::get_file_as_bytes` 读字体文件；`file_write` 用 `FileAccess` WRITE/READ_WRITE 直写磁盘，**属于测试 35 排除清单**（写文件副作用，历史事故源之一）。
  - 本模块是 `display_get_system_fonts`（os_ops）之外的字体获取路径补充。
- **错误模式**：`TextServer not available` / `missing or invalid parameter: font_rid` / `shaped_rid`；`file_write` 打开失败 → error_json。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## tilemap_ops — 瓦片地图（4 工具）

- **职责**：编辑场景中创建 `TileMap`/`TileSet` 并写入格子；兼容 `TileMapLayer` 节点。
- **代表工具**：`tilemap_create`、`tilemap_set_cell`、`tilemap_set_cells`、`tileset_create`。
- **关键事实**：
  - `tilemap_create` 经 `ClassDBSingleton::instantiate("TileMap")` 建节点，默认挂编辑场景根并 `set_owner`，返回路径（剥掉根前缀）。
  - `tilemap_set_cell`/`set_cells` 同时支持 `TileMap::set_cell(layer, coords, source_id, atlas_coords)` 与 `TileMapLayer::set_cell(coords, ...)` 两条 API 分支。
  - `tileset_create` 产出 **`memory://` 内存资源**（`resource_ops::register_memory_resource`），供 tileset_ops 解析。
  - 写场景后调用 `scene_dirty_tracker::mark_scene_modified()`；`set_cells` 部分无效条目 → error + warnings（"N cells skipped"）。
- **错误模式**：节点类型不符 → error_json（含创建指引）；parent 未找到 → error_json。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## tileset_ops — 瓦片集深化（3 工具）

- **职责**：对 `memory://` TileSet 添加 atlas 源（自动按纹理尺寸/边距/间距生成格子）、物理层与格子碰撞多边形。
- **代表工具**：`tileset_add_atlas_source`、`tileset_add_physics_layer`、`tileset_set_tile_collision`。
- **关键事实**：
  - `resolve_tileset` 经 `resource_ops::resolve_memory_resource` 找内存 TileSet，缺失时报"先 tileset_create"。
  - `tileset_add_atlas_source`：`add_source` 后按 `compute_grid`（1 + (tex_dim − margin − tile_dim) / (tile_dim + spacing)）逐格 `create_tile`，返回 created_tiles/grid_size。
  - `tileset_set_tile_collision` 用 `TileSetAtlasSource::get_tile_data(atlas_coords, alternative)` 取 `TileData`（Godot 4.7 迁移事实之一），`set_collision_polygons_count` + `set_collision_polygon_points` 写回；物理层缺失时经 `error_detail` 提示先 `tileset_add_physics_layer`；空 polygon 数组清空现有碰撞。
- **错误模式**：source_id 冲突 → error_json；纹理文件不存在 → error_detail；引擎拒绝多边形 → error_detail（含回读校验 `get_collision_polygon_points` 判空）。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## spriteframes_ops — 精灵帧（3 工具）

- **职责**：`memory://` SpriteFrames 资源：创建（删除默认动画）、加动画、加帧（支持 hframes/vframes 切 AtlasTexture）。
- **代表工具**：`spriteframes_create`、`spriteframes_add_animation`、`spriteframes_add_frame`。
- **关键事实**：
  - `spriteframes_create` 经 `ClassDBSingleton::instantiate("SpriteFrames")` 并 `remove_animation("default")`，注册 `memory://` 路径。
  - `spriteframes_add_frame`：单帧直接 `add_frame(anim, tex, duration)`；多帧（hframes*vframes>1）按 `AtlasTexture` 区域切分逐帧添加，返回 frames/added_frames。
  - 纹理路径需磁盘存在且为可加载 Texture2D，否则 `error_detail`（含 reimport 建议）。
- **错误模式**：hframes/vframes 非正整数 → error_json；动画未加 → error_json（提示先 add_animation）。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## code_exec_ops — GDScript 动态执行（2 元工具）

- **职责**：`code_execute` 在编辑器进程内动态编译并执行任意 GDScript；`batch_execute` 顺序批调任意工具。
- **代表工具**：`code_execute`、`batch_execute`（均为元工具，`register_all.cpp` 直接注册为 handler，不经 `call_tool`；`batch_execute` 内部经 `dispatch::call_handler` 调领域工具）。
- **关键事实（执行机制）**：
  1. **安全检查** `check_source_safety`：禁止 `close_scene(` 调用（会销毁执行节点导致编辑器崩溃）。
  2. **源码包装** `build_wrapped_source`：`extends` 行注释化，包裹为 `@tool extends Node` 脚本；单函数模式（默认 `_run`）把用户代码逐行缩进进 `func _run():`，自动探测 tab/空格缩进风格（混合则报错）；多函数模式（检测到 `func `）保留用户函数，缺失的 `function_name` 补 `pass` 占位。
  3. **编译** `compile_and_map_errors`：`GDScript::set_source_code` + `reload()`；失败时取 `capture_new_error_text` 并把 wrapper 行号回映射为用户源码行（`map_line_numbers`，偏移 5 行），错误文本截断 8192 字节并附 wrapped source。
  4. **执行** `run_with_timeout`：`memnew(Node)` + `set_script` 挂到 SceneTree 根（失败则挂编辑场景根），`TempNodeGuard` 确保析构回收；超时仅在执行前检查一次（同步调用不可中断）。
  5. **结果回收** `reconcile_leaked_children`：执行后比对编辑场景根的子节点数，`auto_owner=true` 递归 `set_owner`（计入 auto_owner_set），false 则删除无主新子节点。
  - **SceneRoot 暴露**：wrapper 内 `var SceneRoot := EditorInterface.get_edited_scene_root()`；错误提示说明执行节点在 `/root` 下、须经 `SceneRoot.get_node("Child")` 访问编辑场景节点。
  - **timeout_ms**：默认 5000；schema（`register_all.cpp`）声明最大 30000，**但实现直接 `static_cast<int>` 未钳制**（与 runtime_ops 的 `GDA_MAX_TIMEOUT_MS` 钳制不同，见文末不一致点）。
  - 返回字段：result（`VariantJson::serialize`）、execution_time_ms、auto_owner_set、wrapped_source、output（≤8192）、runtime_error/error_details/structured_error（首行解析 file/line/message）。
  - 结果为 Resource 时自动 `resource_registry::register_resource`（registered_resource 字段）。
- **错误模式**：`util::error_json(error_out)` 统一出口；编译失败含行号映射与 wrapped source；超时报错不含渲染状态。
- 出站链接：[运行时通道](../modules/entry_runtime.md) · [工具注册表](../modules/tools_registry.md)

## log_ops — 游戏日志文件读取（1 工具）

- **职责**：读取游戏进程磁盘日志 `user://logs/godot.log` 尾部。
- **代表工具**：`log_get_game_entries`（limit 默认 50、上限 500）。
- **关键事实**：
  - Windows 用 `CreateFileW` 共享读（FILE_SHARE_READ|WRITE|DELETE）+ 256KB 尾窗口；打开失败重试一次（100ms 延迟）后回退归档 `godot.log.1`（返回 `from_archive` + warning）。
  - 文件不存在时错误信息附目录诊断（`logs_dir_diagnostic`：目录不存在/为空/内容列表）与运行提示（"start it with editor_play_current_scene"）。
- **错误模式**：`error_detail`（open error code + 建议改用 `debugger_get_output`/`debugger_get_errors`）。
- 出站链接：[运行时通道](../modules/entry_runtime.md) · [工具注册表](../modules/tools_registry.md)

## capture_ops — 编辑器视口截图（1 工具 + base64 工具函数）

- **职责**：编辑器 2D/3D 视口截图并 base64 返回。
- **代表工具**：`editor_capture_viewport`（注册名带 `editor_` 前缀，hpp 函数名 `handle_capture_viewport`）。
- **关键事实**：
  - `target` 仅支持 `"editor"`；`"game"` 返回 error_detail（提示运行时通道未就绪，对应 `game_capture`）。
  - 优先 2D 视口（`EditorInterface::get_editor_viewport_2d`）回退 3D；`ViewportTexture::get_image` → `save_png_to_buffer` → `base64_encode`，返回 data/format/width/height。
  - `base64_encode` 为跨模块工具函数（display_ops 截图、runtime_ops 文件回读共用）。
- **错误模式**：视口不可用 → error_detail（"open a scene with a visible viewport first"）；PNG 编码空缓冲 → error_json。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## 与 AGENTS.md 对照结果

- **35 工具排除清单归属**（`tests/runner/traversal.cpp` 的 `kExcludedSideEffectTools`，共 35 项）：本批模块占 **22 项**——os_ops 7（`os_alert`、`os_create_process`、`os_execute`、`os_kill`、`os_shell_open`、`os_move_to_trash`、`os_set_environment`）、display_ops/display_window_ops 14（`display_dialog_show`、`display_tts_speak`、`display_tts_stop`、`display_clipboard_set`、`display_mouse_set_mode`、`display_mouse_warp`、`display_window_*` 9 个）、text_ops 1（`file_write`）；其余 13 项属 editor_ops/input_map/project_settings/script_ops/resource_ops。**与 AGENTS.md「display_*/os_alert 等」表述一致，但注意 `display_` 前缀并非全排除**：只读的 `display_clipboard_get`、`display_mouse_get_position`、`display_screen_*`（5 个）、`display_tts_get_voices`、`display_screen_capture` 共 10 个不排除。
- **不一致点**：
  1. `code_execute` 的 `timeout_ms`：schema 与 prompt 文档声称 "max 30000"，但 `code_exec_ops.cpp` 读取后未钳制（仅 `static_cast<int>`）；`runtime_game_ops` 则有 `GDA_MAX_TIMEOUT_MS=30000` 钳制。超大值会按原值等待（同步调用期间无法中断）。
  2. AGENTS.md「添加工具需在 `<category>_ops.hpp` 声明」与实现不符：`environment_ops.cpp`（7 工具）、`display_window_ops.cpp`（9 工具）、`runtime_game_ops.cpp`（6 工具）无独立 hpp，声明分别复用 `render_ops.hpp`、`display_ops.hpp`、`runtime_ops.hpp`。
  3. 命名约定 `<category>_<action>` 有两个例外：`output_get_log`（debugger_ops 模块，无类别前缀）与 `editor_capture_viewport`（capture_ops 模块，`editor_` 前缀来自历史命名而非模块名）。
  4. `debugger_ops` 中的 `output_get_log`/`debugger_get_*` 7 工具与 `log_ops` 的 `log_get_game_entries` 职责重叠（内存捕获 vs 磁盘日志），AGENTS.md 未区分两条路径。
  5. AGENTS.md 称元工具"不经过 call_tool"：`batch_execute` 元工具本身直连注册 ✓，但其实现内部经 `dispatch::call_handler` 批调领域工具，路径上会与 `call_tool` 一致地命中 `ExportGuard` 等出口。
