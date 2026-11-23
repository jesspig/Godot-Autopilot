# 内容管线

TileMap 关卡、动画、音频与 UI 主题。场景工具作用于被编辑场景；音频总线工具作用于编辑器进程。

## TileMap 与 TileSet

按顺序构建：节点、TileSet、图集源、物理层、单元格。

1. `create_tilemap` 加节点并附默认 TileSet；场景必须已有根。
2. `create_tilemap_tileset` 创建内存 TileSet，`name` 是后续引用名。
3. `update_tileset` 的 action=add_atlas_source 切分 `res://` 纹理；`source_id`（0-255）不能重复，`margin` 与 `spacing` 可选。
4. action=add_physics_layer 是逐瓦片碰撞的前提。
5. 用 `resource_save` 的 action=save 保存 TileSet，再用 `property_set` 把节点 `tile_set` 指向保存路径；绝不能直接赋 `memory://`。最后 `save_editor_scene` 的 action=save。
6. `set_tilemap_cells` 放置瓦片；TileMap 与 TileMapLayer 都接受。逐格用 `cells`，整块矩形用 `from`/`to`。
7. `update_tileset` 的 action=set_tile_collision 多边形点相对瓦片中心；数组套数组设置多个多边形，空数组清空。

失败点：

- `set_cell` 只给 source id、atlas coords 或 alternative 之一时，会静默清空单元格。
- 超大批量 JSON 可能在工具运行前以解析错误失败；地图级填充用 `set_tilemap_cells` 的 `from`/`to` 矩形形式，或用 `code_execute` 循环 `set_cell`。`source_id: -1` 清空单元格。
- 批量中的非法项会被跳过并返回 warnings 与 set 计数；两者都要读。
- 纹理缺失或未导入会在创建图集时报错。
- 4.8 中 TileMap 是 TileMapLayer 的薄包装；新工作优先 TileMapLayer。

## 动画

`AnimationPlayer`：`create_scene_animation_player` 加节点（`parent_path` 必填）；`create_animation` 建片段（`loop_mode` 0 无、1 线性、2 pingpong）；`create_animation_track` 加值或方法轨道（值轨道需要 `property`，如 `position:x`）；`insert_animation_keyframe` 传时间与 JSON 值或 `{"method": ..., "args": [...]}`。`get_animation_list` 校验长度与循环模式；它的 action=inspect 读轨道与关键帧，action=library 管理 AnimationLibrary 容器。`remove_animation_track` 按索引删轨道；`remove_animation` 删片段。没有单键删除工具，用 `code_execute`。

`AnimationTree`：`animation_tree` 的 action=create 建空状态机（`parent_path`，可选 `name` 与 `anim_player`）。action=add_node 用 `state_name` 与 `animation` 加状态；action=connect 连接 `from_state` 与 `to_state`；`condition` 指定 bool 参数以自动推进。两个状态都必须存在，重复过渡会被拒绝。

`SpriteFrames`：`update_spriteframes` 的 action=create 建内存资源并移除默认动画。action=add_animation 设 fps/loop；action=add_frame 加帧，`hframes`/`vframes` 切分图集，`duration` 调整时序。用 `resource_save` 的 action=save 落盘，再 `property_set` 节点的 `sprite_frames`。

失败点：

- 在已有时间插入关键帧会替换它但保留旧过渡；引擎返回旧索引。
- `travel` 没有可构造路径时静默传送；目标名缺失报错；删除正在播放的状态会停止播放。
- 模式不是 AUTO、条件参数未注册或为假、表达式出错时，AUTO 过渡静默失败。
- 值轨道更新模式只有 CONTINUOUS、DISCRETE、CAPTURE；bool、int、String 属性需要 DISCRETE。

## 音频

Master 是 0 号总线；`audio_bus` 的 setter 接受从 0 开始的 `bus_index` 并立即生效。action=get_layout 快照布局，也是名称、音量与效果链的读回。action=set_volume、action=set_mute、action=set_solo 与 action=set_bypass 是每个总线的开关。action=add_effect 接受 AudioEffect 类名（`effect_type`）；`at_position` 插入，-1 追加。action=set_layout 替换整个布局：先取对象，编辑后写回。

播放器工具：`audio_player` 的 action=play、action=stop、action=set_volume（`volume_db`）、action=set_pitch（`pitch_scale`）、action=get_position 与 action=seek。`audio_device` 的 action=set_output / action=set_input 切换编辑器设备。

失败点：

- 播放器的 `bus` 指向不存在的总线时，静默在 Master 上播放。
- 添加或重命名到已有名称时，静默追加 " 2"。
- bypass 跳过效果链；mute 在效果之后乘零，因此静音总线仍耗 CPU。Solo 覆盖 mute。

## UI 主题与布局

1. `update_theme` 的 action=create 创建 `.tres`；缺失目录会创建，已有文件不覆盖。
2. action=bundle 接受 `path` 与 `colors`、`constants`、`font_sizes`、`styleboxes`，每项把 `theme_type` 映射到 `{varname: value}`，然后一次 load-modify-save。先用 `get_theme_info` 查合法 type/item 对。
3. `update_control_layout` 的 action=apply 接受 `theme_path` 与 `control_path`，在被编辑场景赋主题；最后 `save_editor_scene` 的 action=save。

action=layout 应用 16 种预设之一（`center` 是 8，`full_rect` 是 15），并可同时设 anchors、offsets、size flags、grow 与最小尺寸。`keep_offsets` 默认 false，offset 会重算。

运行时 UI 用 `get_game_ui` 的 action=elements 取 Control 矩形；短命 HUD 反馈用 `capture_editor_viewport` 的 target=game 与 `when`。

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
