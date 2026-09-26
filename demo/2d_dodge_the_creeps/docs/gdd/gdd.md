# GDD 总纲（现状实录）

## 系统总览

Dodge the Creeps 是 Godot 官方“第一个 2D 游戏”教程的完成版。游戏只做一件事：**在有限场地里活下去**。

| 系统 | 落点 | 说明 |
|---|---|---|
| 开局/重开 | `main.gd` `new_game()` + `hud.gd` | 清怪、记分清零、玩家回出生点、`StartTimer` 2 秒后开刷怪与计分 |
| 敌人生成 | `main.gd` `_on_MobTimer_timeout()` | 每 0.5 秒沿 `MobPath` 随机位置生成一只 `mob.tscn` 实例 |
| 记分 | `main.gd` `_on_ScoreTimer_timeout()` + `hud.gd` `update_score()` | 计分 Timer 每次 +1，HUD 显示 |
| 碰撞结束 | `player.gd` `_on_body_entered()` → `hit` → `main.gd` `game_over()` | 玩家隐藏、停 Timer、播 Game Over、停音乐、播死亡音 |
| HUD/菜单 | `hud.tscn` + `hud.gd` | 分数、消息、Start 按钮三件套 |
| 移动 | `player.gd` `_process()` | 四向 400 px/s，钳制在屏幕内 |

## 内容范围

- 场景：`main.tscn`（主场景）、`player.tscn`、`mob.tscn`、`hud.tscn`。
- 脚本：`main.gd`、`player.gd`、`mob.gd`、`hud.gd`，分工见 [tech/tdd.md](../tech/tdd.md)。
- 资源：`art/` 12 个文件、`fonts/` 3 个文件，见 [asset-catalog.md](../art/asset-catalog.md)。
- 配置：显示 480×720、`canvas_items` 拉伸、`gl_compatibility`；输入 5 动作（以源库 `project.godot` 原件为准，工作树模板缺失这些段，见 [tdd.md](../tech/tdd.md)）。

## 完成定义

- 打开 `main.tscn` 即是可运行游戏，无需额外拼装。
- 一局流程闭环：Start → Get Ready → 刷怪计分 → 碰撞 Game Over → 回标题可再开。
- 文档中引用的每个文件路径在磁盘上真实存在；行为描述与 `.gd` / `.tscn` 文本一致。

## 与 GDA 测试床的关系

本工程体量小、信号链短、成功现象明显（怪出现、分数跳、Game Over 回标题），适合做 GDA 的 E2E 回归床：导航找节点、读脚本确认行为、运行验证、截图比对。测试方法见 [test-plan.md](../test/test-plan.md)，验收项见 [acceptance.md](../test/acceptance.md)。
