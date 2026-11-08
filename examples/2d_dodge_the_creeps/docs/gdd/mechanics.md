# 玩法机制实录

本文是玩法行为的**唯一权威**。节点结构见 [level-design.md](./level-design.md)，信号接线见 [tdd.md](../tech/tdd.md)。

## 不变式（源码保证）

- 玩家 `hide()` 后碰撞体延迟禁用，同一局内不会二次触发 `hit`（`player.gd` `_on_body_entered()`）。
- `game_over()` 必停 `ScoreTimer` 与 `MobTimer`，分数 freeze（`main.gd`）。
- `new_game()` 必清场：`call_group("mobs", "queue_free")`，分数归零（`main.gd`）。
- 出生点固定为 `StartPosition`（240, 450），`start(pos)` 重置旋转、显示、启用碰撞（`player.gd`、`main.tscn`）。

## 输入语义（源库 project.godot 原件）

| 动作 | 键位（物理键） | 手柄/轴事件 | 行为 |
|---|---|---|---|
| `move_left` | A、← | 14 号键、0 轴负向 | 左移 |
| `move_right` | D、→ | 15 号键、0 轴正向 | 右移 |
| `move_up` | W、↑ | 12 号键、1 轴负向 | 上移 |
| `move_down` | S、↓ | 13 号键、1 轴正向 | 下移 |
| `start_game` | 空格、回车（Enter） | 无 | 触发 Start 按钮（`hud.tscn` 快捷键） |

> 注意：工作树内 `project.godot` 当前是最小模板，**没有** `[input]` 段。运行前需恢复上述映射，否则移动与快捷开始无效。手柄部分仅转录配置，未核实真机行为。

## 移动

- 速度 400 px/s（`player.gd` `@export var speed = 400`）。
- 每帧按四个动作合成向量，归一化后 `position += velocity * delta`，并钳制在 `(0,0)` 与屏幕尺寸之间。
- 有速度播 `AnimatedSprite2D`，无速度停播；`velocity.x != 0` 用 `right` 动画并按方向翻转，纯纵向用 `up` 动画并按上下旋转（`player.gd` `_process()`）。

## 敌人生成

- 周期：`MobTimer` `wait_time = 0.5`；准备期：`StartTimer` 2.0 秒单发，超时后才启动 `MobTimer` 与 `ScoreTimer`。
- 位置：`MobPath/MobSpawnLocation.progress_ratio = randf()`，取其 `position`。
- 朝向：路径切线法线方向 `± PI/4` 随机扰动，`mob.rotation = direction`。
- 速度：`Vector2(randf_range(150.0, 250.0), 0.0).rotated(direction)`，写入 `mob.linear_velocity`。
- 外观：`mob.gd` `_ready()` 从 `fly` / `swim` / `walk` 三套动画随机选一套播放。
- 清理：`mob.gd` 出屏 `queue_free()`；`new_game()` 清掉 `mobs` 组全部实例。

## 记分

- `ScoreTimer` 每次超时 `score += 1`，经 `HUD.update_score(score)` 显示为整数文本。
- `ScoreTimer` 在 `main.tscn` 中未显式写 `wait_time`，按 Timer 默认值应为 1.0 秒——**未核实**，以编辑器检查器读数为准。
- `new_game()` 先 `score = 0` 并刷新 HUD，再显示 Get Ready。

## 碰撞与结束

1. `Player`（Area2D）`body_entered` → `hide()` + `hit.emit()` + 碰撞体延迟禁用。
2. `Main.game_over()`：停 `ScoreTimer` / `MobTimer` → `HUD.show_game_over()` → `$Music.stop()` → `$DeathSound.play()`。

## 重开流程

1. `HUD.show_game_over()`：显示 Game Over → 等 `MessageTimer` → 显示标题 → 等 1 秒 → 显示 Start 按钮。
2. 点 Start（或 `start_game` 快捷键）：隐藏按钮 → `start_game.emit()` → `Main.new_game()`。
3. `new_game()`：清怪 → 分数归零 → 玩家放到 `StartPosition` → 启动 `StartTimer` → 显示 Get Ready → 播背景音乐。

## 边界情况

- 准备期（`StartTimer` 未超时）被撞：按源码仍会走 `game_over()`，此时计分/刷怪 Timer 尚未启动——实际游玩中玩家刚出生在场地中央，是否会立即被撞取决于历史残留怪是否清干净；`new_game()` 已清场，故该窗口理论安全。
- 同帧多怪碰撞：首次 `body_entered` 后隐藏 + 延迟禁用碰撞，后续同帧信号是否再触发取决于物理回调顺序——未核实。
