# 技术设计实录（TDD）

本文是场景/脚本/配置接线的**唯一权威**。数值见 [../gdd/mechanics.md](../gdd/mechanics.md)，几何见 [../gdd/level-design.md](../gdd/level-design.md)。

## 文件总览（磁盘实有）

- 脚本：`Main.gd`、`Player.gd`、`Mob.gd`、`ScoreLabel.gd`（各有同名 `.uid`）。
- 场景：`Main.tscn`、`Player.tscn`、`Mob.tscn`、`MusicPlayer.tscn`。
- 配置：`project.godot`、`ui_theme.tres`、`icon.webp`。

## 主线程约束（本仓库硬约束）

- 所有 Godot API 调用必须经 `CommandQueue` 在主线程执行；测试与验证中不得以任何方式绕过。本节仅声明约束，不涉及具体接线。

## 场景结构与信号

### Main.tscn（根 Node + Main.gd）

- `mob_scene` 导出已指定为 `Mob.tscn`。
- 子节点：`Ground`、`Walls`、`DirectionalLight3D`、`WorldEnvironment`、`Player`（`Player.tscn` 实例）、`CameraPivot/Camera`、`Cylinders`（4 装饰柱）、`SpawnPath/SpawnLocation`、`MobTimer`、`UserInterface/ScoreLabel`、`UserInterface/Retry/Label`。
- 连接：`Player.hit` → `Main._on_player_hit`；`MobTimer.timeout` → `Main._on_mob_timer_timeout`；运行时 `mob.squashed` → `ScoreLabel._on_Mob_squashed`（每次生成时连接）。

### Player.tscn（CharacterBody3D + Player.gd）

- `collision_mask = 2147483654`（tscn 原值；位含义未核实）。
- `Pivot/Character` 为 `art/player.glb` 实例；`CollisionShape` 球形（半径约 0.792）；`MobDetector`（`Area3D`，`collision_layer = 0`、`collision_mask = 2`、`monitorable = false`）下挂 5 个柱形检测体。
- `AnimationPlayer` 自动播放 `float`（1.2 秒循环）。
- 连接：`MobDetector.body_entered` → `Player._on_MobDetector_body_entered` → `die()`。

### Mob.tscn（CharacterBody3D + Mob.gd，`mob` 组）

- `collision_layer = 2`（enemies），`collision_mask = 2147483648`（tscn 原值；位含义未核实）。
- `Pivot/Character` 为 `art/mob.glb` 实例；1 主盒形 + 4 小盒形碰撞体；`VisibleOnScreenNotifier3D`（AABB 以 tscn 为准）；`AnimationPlayer` 自动播放 `float`。
- 连接：`VisibleOnScreenNotifier3D.screen_exited` → `_on_visible_on_screen_notifier_screen_exited`。

### MusicPlayer.tscn

- `AudioStreamPlayer`，`stream = art/House In a Forest Loop.ogg`，`autoplay = true`。

## 工程配置：源库原件 vs 工作树模板

源库原件指源库 `3d/squash_the_creeps/project.godot`（`config_version=5`），关键节：

| 节 | 源库原件值 |
|---|---|
| application | `name="Squash the Creeps (3D)"`，`main_scene="res://Main.tscn"`，`features=PackedStringArray("4.7")`，`icon="res://icon.webp"` |
| autoload | `MusicPlayer="*res://MusicPlayer.tscn"` |
| display | `stretch/mode="canvas_items"`，`stretch/aspect="expand"` |
| filesystem | `import/blender/enabled=false` |
| input | `move_left/right/forward/back`、`jump` 五动作（细节见 mechanics） |
| layer_names | `3d_physics/layer_1="player"`、`layer_2="enemies"`、`layer_3="world"` |
| physics | `ticks_per_second=120`，`3d/physics_engine="Jolt Physics"`，`physics_interpolation=true` |
| rendering | `soft_shadow_filter_quality=3`，`msaa_3d=2` |

工作树内 `project.godot` 现状（已被清洗为最小模板，仅 15 行）：只保留 `[application]` 的名称/特性/图标，无 `main_scene`、无 `[autoload]`、无 `[input]`、无 `[layer_names]`、无 `[physics]`、无 `[rendering]`、无 `[display]`。因此：凡依赖自动加载音乐、输入动作、物理层名、Jolt、120 tick、插值、MSAA 的验证，必须先确认以哪份配置启动并如实记录；文档记录源库原件值，工作树模板缺失视为已知差异。

## 实现自由度（测试床约束）

- 文档是现状实录：测试不得修改游戏行为代码与配置来"迎合"文档；发现不一致时以磁盘文件为准并记录偏差。
- 允许的写入：仅 `docs/` 文档修正与测试报告；其余文件只读（`docs/` 之外不改动，不运行构建）。
