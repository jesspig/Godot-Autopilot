---
name: godot-autopilot-content
description: "Godot 内容管线指南：TileMap 与 TileSet、AnimationPlayer 与 AnimationTree、音频总线与播放、Control 主题与布局——以及各领域的静默失败坑。适用于搭建瓦片关卡、制作动画、配置声音或设置 UI 样式。"
---

# Godot 内容管线

调用方式：用 `gda search` 与 `gda describe` 发现，再用 `gda call`
调用；原生 MCP 工具（`search_tools`、`get_tool_detail`、`call_tool`）是兼容
路径。协议细节见 `godot-autopilot-tools`。

四个内容领域：TileMap 与 TileSet 关卡、AnimationPlayer 与 AnimationTree
动画、音频总线与播放、Control 主题与布局。每节给出工作流与该领域的高频坑，
含经验证的静默失败细节。场景工具作用于 Godot 编辑器中打开的场景；音频总线
工具作用于编辑器进程（运行中的游戏有自己的设备与布局）。

## TileMap 与 TileSet

构建顺序很重要：节点 → TileSet → 图集源 → 物理层 → 格子。

1. `create_tilemap` — 创建带默认 TileSet 的 TileMap 节点（`name`、
   `tile_size` 像素、`parent_path`；场景必须已有根，否则报错）。
2. `create_tilemap_tileset` — 在内存中创建 TileSet 资源（`memory://` 引用，
   尚未落盘）。
3. `add_tilemap_atlas_source` — 把 `res://` 纹理切成瓦片网格（`source_id`
   0-255 且未使用；可选 `margin`/`spacing`）。返回源 id、创建的瓦片数与网格
   尺寸。
4. `add_tilemap_physics_layer` — 逐瓦片碰撞前必需（可选
   `collision_layer`/`collision_mask` 位掩码，默认 1）。
5. `set_tilemap_cell` / `set_tilemap_cells` — 写单个或批量格子。批量工具
   没有固定服务器侧条数上限，但超大 JSON 会撞传输限制；大批布局用
   `code_execute` 循环。
6. `save_resource` — TileSet 必须先落盘才能赋给场景节点；`memory://` 资源
   不能直接写进场景文件。

坑：

- 未保存的 TileSet 赋给 TileMap 会留下损坏引用；先 `save_resource`。
- `source_id` 未占用才可添加；重复 id 报错。`-1` 清除格子。
- TileMap 数据只在保存场景后写入 .tscn；保存前崩溃会丢失布局。
- 物理层必须在写碰撞格子前添加，否则碰撞数据被忽略。

## 动画

### AnimationPlayer

1. `create_scene_animation_player` 在场景中创建 AnimationPlayer。
2. `create_animation` 创建动画（名称、长度、步长）。
3. `create_animation_track` 添加轨道（属性、变换、方法、音频、动画）。
4. `insert_animation_keyframe` 插入关键帧；`set_animation_keyframe_*` 调整
   插值、过渡与值。
5. `set_scene_animation_player_autoplay` 设置自动播放；保存场景后生效。

坑：

- 轨道路径相对 AnimationPlayer 的根节点；写错路径不会报错，只会静默无效果。
- 关键帧时间必须在动画长度内；超出部分被裁剪。
- 方法轨道的参数数组必须与目标方法签名匹配，否则运行时才报错。
- 动画修改后需要保存场景；仅内存修改不会写盘。

### AnimationTree 状态机

1. `create_scene_animation_tree` 创建 AnimationTree 并绑定
   `anim_player`。
2. `set_scene_animation_tree_root` 设置根节点类型（StateMachine、
   BlendTree 等）。
3. `add_animation_machine_state` 添加状态并绑定动画。
4. `connect_animation_states` 连接状态转移（条件、优先级、自动推进）。
5. `set_scene_animation_tree_active` 激活；保存场景。

坑：

- 状态机节点路径必须已存在；先建状态再连线。
- `travel` 与 AUTO 转移的优先级规则不同；自动转移只在条件满足时触发，手动
  travel 会立即切换。
- AnimationTree 的 `active` 默认 false；不激活时状态机不推进。

### SpriteFrames

`create_spriteframes`、`add_spriteframes_animation`、
`add_spriteframes_frame` 创建并填充。`property_set` 的
`sprite_frames.animations` 接受引擎数组或名称到 `{frames, speed, loop}` 的
字典；帧是纹理引用或 `{texture, duration}`。`speed`/`fps` 必须是数字，
`loop` 是布尔，名称非空且与字典键一致。失败错误给出真实 SpriteFrames 资源
路径，而不是宿主节点。

## 音频

- `create_audio_bus`、`set_audio_bus_*` 创建和配置总线（音量、静音、独奏、
  发送）。总线名全局唯一；发送目标必须已存在。
- `add_audio_bus_effect` 添加效果；`remove_audio_bus_effect` 删除。效果顺序
  影响处理链；插入位置用索引控制。
- `play_audio_stream`、`stop_audio_stream`、`set_audio_stream_*` 在编辑器
  进程播放/停止音频流；运行中的游戏有独立音频设备，编辑器播放不会进入游戏。
- `get_audio_bus_layout`、`get_audio_bus_peak_volume` 读取布局与峰值。

坑：

- 总线名大小写敏感；拼错会静默创建新总线或失败。
- `bypass` 与 `mute` 的 CPU 成本不同：bypass 跳过处理，mute 仍处理。
- 复音数超限时引擎按优先级偷取发声；不要假设所有 `play` 都同时可闻。
- 编辑器播放与游戏播放是两套设备，音量/总线修改不一定同步。

## UI 主题与布局

### 主题文件

1. `create_theme_resource` 创建 Theme（可选保存路径）。
2. `set_theme_color`、`set_theme_font_size`、`set_theme_font`、
   `set_theme_stylebox_flat`、`set_theme_stylebox_texture`、
   `set_theme_constant` 设置各类主题项。
3. `apply_theme_to_control` 把 Theme 赋给 Control；`set_control_theme_*`
   设置局部覆盖。
4. `save_resource` 保存 .tres；场景保存后主题引用才稳定。

坑：

- 主题查找会先走 Control 的局部覆盖，再沿祖先与类型链查找；近祖先的通用项
  可能胜过远祖先的精确 variation，variation 链不跨 Theme 边界。
- `theme_override_*` 属性名含 `/`，必须作为完整属性名写入，不能当路径拆分。
- 修改 Theme 资源后，使用它的场景需要重绘/重载才看到变化；截图验证。

### 锚点预设与运行时侦察

- `set_control_anchor_preset` 设置锚点预设；`set_control_offsets_preset`
  同时设置偏移。预设枚举与运行时布局要读回确认。
- 全窗口根节点的配方：预设设为 full rect，读回 offsets 确认归零，再对两个
  轴做 grow 居中。
- `get_game_ui_elements` 返回运行中游戏的 Control 树、`global_rect` 与可见
  性。用矩形定位按钮/面板，再用 `queue_game_input` 注入点击；全屏 `PASS`
  控件可能吞点击。
- `hit_test_editor_point` 与 `click_editor_element` 用于编辑器 UI；游戏与
  编辑器是两套坐标，不要混用。

## 截图验证内容

- `capture_editor_viewport` 验证编辑器中的场景外观；`capture_game_viewport`
  验证运行中的游戏。
- 可选 `region` 裁剪、`max_dimension` 限制最长边、`scale` 最近邻放大；
  最终尺寸在 `width`/`height` 中报告。
- 短暂反馈（2 秒 toast、0.7 秒死亡画面）用 `after_frames` 或 `when` 条件
  截图；文本条件必须同时检查可见性，隐藏标签的文本不会清空。

## 参见

- `godot-autopilot-scene-system` - 节点生命周期、`property_set` JSON 值形状、
  属性重命名表。
- `godot-autopilot-resources` - `save_resource`、纹理导入管线与 .tres 路径
  规则。
- `godot-autopilot-scripting` - 批量填充格子与程序化内容生成。
- `godot-autopilot-runtime` - 运行游戏、注入输入与截图验证。
