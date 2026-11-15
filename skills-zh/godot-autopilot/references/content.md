# 内容管线

TileMap 关卡、动画、音频与 UI 主题。场景工具作用于被编辑场景；音频总线工具作用于编辑器进程。

## TileMap 与 TileSet

按顺序构建：节点、TileSet、图集源、物理层、单元格。

1. `create_tilemap` 加节点并附默认 TileSet；场景必须已有根。
2. `create_tilemap_tileset` 创建内存 TileSet。
3. `add_tilemap_atlas_source` 切分 `res://` 纹理；`source_id`（0-255）不能重复，`margin` 与 `spacing` 可选。
4. `add_tilemap_physics_layer` 是逐瓦片碰撞的前提。
5. 用 `save_resource` 保存 TileSet，再用 `property_set` 把节点 `tile_set` 指向保存路径；绝不能直接赋 `memory://`。最后 `save_editor_scene`。
6. `set_tilemap_cell` / `set_tilemap_cells` 放置瓦片；TileMap 与 TileMapLayer 都接受。
7. `set_tilemap_tile_collision` 的多边形点相对瓦片中心；数组套数组设置多个多边形，空数组清空。

失败点：

- `set_cell` 只给 source id、atlas coords 或 alternative 之一时，会静默清空单元格。
- 超大批量 JSON 可能在工具运行前以解析错误失败；地图级填充用 `code_execute` 循环 `set_cell`。`source_id: -1` 清空单元格。
- 批量中的非法项会被跳过并返回 warnings 与 set 计数；两者都要读。
- 纹理缺失或未导入会在创建图集时报错。
- 4.8 中 TileMap 是 TileMapLayer 的薄包装；新工作优先 TileMapLayer。

## 动画

`AnimationPlayer`：`create_scene_animation_player` 加节点（`parent_path` 必填）；`create_animation` 建片段（`loop_mode` 0 无、1 线性、2 pingpong）；`create_animation_track` 加值或方法轨道（值轨道需要 `property`，如 `position:x`）；`insert_animation_keyframe` 传时间与 JSON 值或 `{"method": ..., "args": [...]}`。`get_animation_list` 校验长度与循环模式。`remove_animation_track` 按索引删轨道；`remove_animation` 删片段。没有单键删除工具，用 `code_execute`。

`AnimationTree`：`create_scene_animation_tree` 建空状态机。`add_animation_machine_state` 加状态，`connect_animation_states` 连接；`condition` 指定 bool 参数以自动推进。两个状态都必须存在，重复过渡会被拒绝。

`SpriteFrames`：`create_spriteframes` 建内存资源并移除默认动画。`add_spriteframes_animation` 设 fps/loop；`add_spriteframes_frame` 加帧，`hframes`/`vframes` 切分图集，`duration` 调整时序。保存后 `property_set` 节点的 `sprite_frames`。

失败点：

- 在已有时间插入关键帧会替换它但保留旧过渡；引擎返回旧索引。
- `travel` 没有可构造路径时静默传送；目标名缺失报错；删除正在播放的状态会停止播放。
- 模式不是 AUTO、条件参数未注册或为假、表达式出错时，AUTO 过渡静默失败。
- 值轨道更新模式只有 CONTINUOUS、DISCRETE、CAPTURE；bool、int、String 属性需要 DISCRETE。

## 音频

Master 是 0 号总线；setter 接受从 0 开始的 `bus_index` 并立即生效。`get_audio_bus_layout` 快照布局，也是名称、音量与效果链的读回。`set_audio_bus_volume_db`、`set_audio_bus_mute`、`set_audio_bus_solo`、`set_audio_bus_bypass_effects` 是每个总线的开关。`add_audio_bus_effect` 接受 AudioEffect 类名；`at_position` 插入，-1 追加。`set_audio_bus_layout` 替换整个布局：先取对象，编辑后写回。

播放器工具：`play_audio_player`、`stop_audio_player`、`set_audio_player_volume_db`、`set_audio_player_pitch_scale`、`get_audio_player_playback_position`、`seek_audio_player`。`set_audio_device_output` / `set_audio_device_input` 切换编辑器设备。

失败点：

- 播放器的 `bus` 指向不存在的总线时，静默在 Master 上播放。
- 添加或重命名到已有名称时，静默追加 " 2"。
- bypass 跳过效果链；mute 在效果之后乘零，因此静音总线仍耗 CPU。Solo 覆盖 mute。

## UI 主题与布局

1. `create_theme_resource` 创建 `.tres`；缺失目录会创建，已有文件不覆盖。
2. `set_theme_color`、`set_theme_constant`、`set_theme_font_size`、`set_theme_stylebox_flat` 接受 `path`、`theme_type` 与 `varname`，然后 load-modify-save。先用 `get_theme_info` 查合法 type/item 对。
3. `apply_theme_to_control` 在被编辑场景赋主题；最后 `save_editor_scene`。

`set_control_anchor_preset` 应用 16 种预设之一（`center` 是 8，`full_rect` 是 15）。`keep_offsets` 默认 false，offset 会重算。

运行时 UI 用 `get_game_ui_elements` 取 Control 矩形；短命 HUD 反馈用 `capture_game_viewport` 的 `when`。

失败点：

- `set_anchors_preset` 默认保留 offset，而 `set_anchors_and_offsets_preset` 强制重算。
- 锚定 Control 不持久化 `position`/`size`；`anchor_*`、`offset_*`、`grow_*` 才是保存真相。
- 主题查找先穷尽每个祖先的整条类型链再向外；近处通用项会压过远处精确变体。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| 放好的瓦片消失 | `set_cell` 只给了一个坐标字段 | 同时传全 cell 字段 |
| 瓦片碰撞缺失 | 没有物理层或多边形 | 加物理层后设置碰撞 |
| AUTO 过渡不生效 | 模式不对或条件未注册 | 检查模式与 bool 参数 |
| 声音在 Master 上播放 | 播放器总线名不存在 | 读总线布局并修正名称 |
| 主题项未应用 | type/item 对或变体链不对 | 跑 `get_theme_info` 并检查祖先 |
