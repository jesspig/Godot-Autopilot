# 场景实录（关卡 / Level Design）

本游戏无多关卡，只有一个可玩场景 `main.tscn` + 两个可实例化子场景。所谓“关卡设计”即主场景的固定结构与敌人生成区间。

## 主场景 main.tscn

| 节点 | 类型 | 关键属性 |
|---|---|---|
| `Main` | Node（挂 `main.gd`） | `mob_scene` 指向 `mob.tscn` |
| `ColorRect` | ColorRect | 全屏锚定，颜色深青灰（0.22, 0.37, 0.38） |
| `Player` | `player.tscn` 实例 | 出生由 `start()` 动态设置 |
| `MobTimer` | Timer | `wait_time = 0.5` |
| `ScoreTimer` | Timer | `wait_time` 未显式设置（未核实，见 mechanics） |
| `StartTimer` | Timer | `wait_time = 2.0`，单发 |
| `StartPosition` | Marker2D | (240, 450)，场地中下方 |
| `MobPath` | Path2D | 矩形环路曲线（5 点，覆盖 480×720 边缘，`_data` 见 tscn） |
| `MobPath/MobSpawnLocation` | PathFollow2D | 生成时 `progress_ratio` 随机 |
| `HUD` | `hud.tscn` 实例 | CanvasLayer |
| `Music` | AudioStreamPlayer | `art/House In a Forest Loop.ogg` |
| `DeathSound` | AudioStreamPlayer | `art/gameover.wav` |

信号接线 5 条：`Player.hit` → `game_over`；三个 Timer 超时各接对应回调；`HUD.start_game` → `new_game`。

## 玩家场景 player.tscn

- 根 `Area2D`，`z_index = 10`，挂 `player.gd`。
- `AnimatedSprite2D`：`right`（`art/playerGrey_walk1.png`、`art/playerGrey_walk2.png`，5.0 fps）与 `up`（`art/playerGrey_up1.png`、`art/playerGrey_up2.png`）两套动画，缩放 0.5。
- `CollisionShape2D`：胶囊形，半径 27、高 68。
- `Trail`：`GPUParticles2D`，数量 10，贴图复用 `playerGrey_walk1.png`，白色渐隐材质。
- 自身 `body_entered` 回接到 `_on_body_entered`。

## 敌人场景 mob.tscn

- 根 `RigidBody2D`，分组 `mobs`，`collision_mask = 0`，`gravity_scale = 0.0`，挂 `mob.gd`。
- `AnimatedSprite2D`：`fly`（`art/enemyFlyingAlt_1.png`、`art/enemyFlyingAlt_2.png`，3.0 fps）、`swim`（`art/enemySwimming_1.png`、`art/enemySwimming_2.png`，4.0 fps）、`walk`（`art/enemyWalking_1.png`、`art/enemyWalking_2.png`，4.0 fps），缩放 0.75。
- `CollisionShape2D`：胶囊形半径 37、高 100，旋转 90°。
- `VisibleOnScreenNotifier2D`：`screen_exited` → `_on_VisibilityNotifier2D_screen_exited`（`queue_free`）。

## 生成区间

生成区间即 `MobPath` 矩形环路：敌人出现在屏幕边缘随机点，朝场地内侧（法线 ±45° 扰动）飞入。曲线 `_data` 覆盖 (0,0)→(480,0)→(480,720)→(0,720) 回环，具体控制柄以 tscn 文本为准。

## 显示配置

480×720 竖屏（`window/size/viewport_width/height`），窗口覆盖同值，拉伸模式 `canvas_items`（源库原件；工作树模板缺失 `[display]` 段，运行前需恢复）。
