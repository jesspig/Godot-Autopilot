# 菜单/HUD 实录

本文记录界面元素的信息契约与出现时机。数值与行为以 [mechanics.md](./mechanics.md) 为准，节点路径与主题接线以 [../tech/tdd.md](../tech/tdd.md) 为准。

本游戏无主菜单、无暂停菜单、无结算面板，只有以下两个 UI 元素（均在 `Main.tscn` 的 `UserInterface` 下）。

## ScoreLabel（分数）

- 节点：`Main/UserInterface/ScoreLabel`（`Label`，挂 `ScoreLabel.gd`）。
- 位置：左上，`offset_left = 16`、`offset_top = 16`（`offset_right = 77`、`offset_bottom = 42`）。
- 样式：字体颜色黑（`font_color = Color(0,0,0,1)`），字号 32；主题来自 `ui_theme.tres`（默认字体为 `fonts/Montserrat-Medium.ttf`）。
- 初始文字：`"Score: 0"`。
- 更新时机：每次压扁一只 Mob，`squashed` 信号触发 `_on_Mob_squashed`，`score + 1` 并刷新为 `"Score: %s"`。
- 常驻可见，死亡与重试期间不隐藏（代码无隐藏逻辑）。

## Retry（失败重试遮罩）

- 节点：`Main/UserInterface/Retry`（`ColorRect`，半透明黑 `Color(0,0,0,0.447059)`，初始 `visible = false`），子节点 `Label`。
- 覆盖范围：全屏锚定（`anchor_right = 1`、`anchor_bottom = 1`）。
- 提示文字：`"Press Space or Enter to retry"`，字号 32，居中（锚点 0.5，偏移 ±75/±13）。
- 出现时机：仅在玩家 `hit` 后由 `_on_player_hit` 显示；`_ready()` 中默认隐藏。
- 交互：可见时按 `ui_accept`（空格/回车）重载当前场景；遮罩本身无按钮。
- 显示期间游戏状态：玩家已删除、生成计时器已停；旧怪仍在场上直线移动直至出屏（未核实是否全部离屏，写实为止）。

## 无障碍与提示

- 除上述两项外无其他 HUD、教程、提示、菜单；操作说明仅见仓库 `README.md`，游戏内无文字教程。
