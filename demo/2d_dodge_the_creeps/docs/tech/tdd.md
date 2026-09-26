# 技术实录（TDD）

## 脚本分工

| 脚本 | 挂载 | 职责 |
|---|---|---|
| `main.gd`（`extends Node`） | `Main` | 开局/结束流程、刷怪、计分；`@export mob_scene`；`score` 成员 |
| `player.gd`（`extends Area2D`） | `Player` | 四向移动、动画切换、出生重置、碰撞上报（`signal hit`） |
| `mob.gd`（`extends RigidBody2D`） | `Mob` | 出生随机选动画、出屏自毁 |
| `hud.gd`（`extends CanvasLayer`） | `HUD` | 消息/分数/按钮三件套（`signal start_game`） |

Godot API 约束（本仓库硬约束）：如需经 GDA 改动运行时行为，所有 Godot API 调用走 `CommandQueue` 主线程；文档本身不涉及实现。

## 信号接线（main.tscn 5 条 + 子场景 3 条）

- `Player.hit` → `Main.game_over`
- `MobTimer.timeout` → `Main._on_MobTimer_timeout`
- `ScoreTimer.timeout` → `Main._on_ScoreTimer_timeout`
- `StartTimer.timeout` → `Main._on_StartTimer_timeout`
- `HUD.start_game` → `Main.new_game`
- `Player.body_entered` → `Player._on_body_entered`（`player.tscn` 内）
- `VisibleOnScreenNotifier2D.screen_exited` → `Mob._on_VisibilityNotifier2D_screen_exited`（`mob.tscn` 内）
- `StartButton.pressed` / `MessageTimer.timeout` → HUD 内部回调（`hud.tscn` 内）

## 关键函数签名（转录）

- `Main.new_game()` / `Main.game_over()` / `Main._on_MobTimer_timeout()` / `Main._on_ScoreTimer_timeout()` / `Main._on_StartTimer_timeout()`
- `Player._ready()` / `Player._process(delta)` / `Player.start(pos)` / `Player._on_body_entered(_body)`
- `Mob._ready()` / `Mob._on_VisibilityNotifier2D_screen_exited()`
- `HUD.show_message(text)` / `HUD.show_game_over()` / `HUD.update_score(score)` / `HUD._on_StartButton_pressed()` / `HUD._on_MessageTimer_timeout()`

## 工程配置差异（重要）

| 段 | 源库原件（可玩） | 工作树现状 |
|---|---|---|
| `[application]` | 名称/描述/tags/主场景/特性/图标俱全 | 仅名称、特性、图标 |
| `[display]` | 480×720、窗口覆盖 480×720、`canvas_items` 拉伸 | 缺失 |
| `[input]` | 5 动作完整映射（见 mechanics） | 缺失 |
| `[rendering]` | `gl_compatibility`（含 mobile） | 缺失 |

E2E 运行前必须先恢复 `[display]` / `[input]` / `[rendering]`（以源库原件为准），否则移动、快捷开始与窗口尺寸均不符合文档。恢复操作本身可作为 GDA 写配置能力的测试点，但必须记录改了什么。

## 自由度边界

- 文档记录现状，禁止“优化”数值（如 Timer、速度、位置）除非测试任务明确要求，且改动必须记录。
- 新增 `.cpp` 与本工程无关；本工程只含 `.gd` / `.tscn`，不触发 C++ 登记规则。
- 不运行 Godot、不构建：文档核对只读 `.gd` / `.tscn` / `project.godot` 文本与编辑器界面。
