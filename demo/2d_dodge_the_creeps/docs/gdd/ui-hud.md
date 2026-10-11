# 菜单 / HUD 实录

对应场景：`hud.tscn`，逻辑：`hud.gd`。HUD 是 `Main` 的子节点 `CanvasLayer`。

## 节点一览（以 hud.tscn 为准）

| 节点 | 类型 | 说明 |
|---|---|---|
| `HUD` | CanvasLayer | 根，挂 `hud.gd` |
| `ScoreLabel` | Label | 顶部居中，字号 60，初始文本 `0` |
| `MessageLabel` | Label | 屏幕中央，字号 60，初始文本 `Dodge the\nCreeps` |
| `StartButton` | Button | 底部中央（offset -90/-200/90/-100），字号 60，文本 `Start`，快捷键绑定 `start_game` 动作 |
| `MessageTimer` | Timer | 单发（`one_shot`），`wait_time` 未显式设置（以检查器读数为准，未核实） |

三处文字节点均使用字体 `fonts/Xolonium-Regular.ttf`。

## 显示时机

| 时机 | 表现 | 源码 |
|---|---|---|
| 启动（未开始） | 标题 + Start 按钮可见，分数 `0` | `hud.tscn` 初始值 |
| 点 Start | 按钮隐藏，发出 `start_game` | `_on_StartButton_pressed()` |
| 新局 | 中央显示 Get Ready（`show_message`），分数归零 | `main.gd` `new_game()` |
| 消息超时 | `MessageLabel` 隐藏 | `_on_MessageTimer_timeout()` |
| Game Over | 中央显示 Game Over → 等 `MessageTimer` → 回标题 → 等 1 秒 → Start 按钮重现 | `show_game_over()` |
| 计分 | `ScoreLabel` 显示整数分数 | `update_score(score)` |

## 信号

- `hud.gd` `signal start_game` → `main.tscn` 接线至 `Main.new_game()`。
- `StartButton.pressed` → `HUD._on_StartButton_pressed()`；`MessageTimer.timeout` → `HUD._on_MessageTimer_timeout()`。

## E2E 核对点

- 打开 `hud.tscn`：应看到三行大字元素（分数/消息/按钮）与一个 Timer。
- 运行 `main.tscn`：标题 + Start 同框；点 Start 后按钮消失、出现 Get Ready；被撞后 Game Over → 标题 → Start 按钮按序重现（需截图三态）。
