# 验收标准（编辑器可执行）

前置：确认 `project.godot` 输入/显示/渲染器已恢复（见 [tdd.md](../tech/tdd.md)）；主场景 `main.tscn`。

## A. 结构验收（编辑器）

- [ ] A1 打开 `main.tscn`：场景树含 `Player`、`MobTimer`、`ScoreTimer`、`StartTimer`、`StartPosition`、`MobPath/MobSpawnLocation`、`HUD`、`Music`、`DeathSound`。截图（视口 + 场景树）。
- [ ] A2 选中 `StartPosition`：坐标 (240, 450)。
- [ ] A3 选中 `MobTimer`：`wait_time = 0.5`；`StartTimer`：2.0 + 单发；`ScoreTimer`：记录检查器实际值（文档未核实）。
- [ ] A4 打开 `player.tscn` / `mob.tscn` / `hud.tscn`：动画名（`right`/`up`；`fly`/`swim`/`walk`）、碰撞体形状、HUD 三节点与字体可见。截图。
- [ ] A5 信号：5 条主接线 + 3 处内部连接与文档一致（检查器 Connections 面板截图）。

## B. 输入验收（编辑器项目设置）

- [ ] B1 `[input]` 含 5 动作；`move_*` 各有键盘 + 手柄事件；`start_game` 含空格/回车。
- [ ] B2 `StartButton` 快捷键绑定 `start_game`（`hud.tscn` 检查器）。

## C. 运行验收（运行 main.tscn）

- [ ] C1 启动：标题 `Dodge the Creeps` + Start 按钮 + 分数 `0` 同框。截图。
- [ ] C2 点 Start（或按空格/回车）：按钮消失，中央 `Get Ready`。截图。
- [ ] C3 约 2 秒后：敌人出现、分数开始跳动。截图（敌人与玩家同框）。
- [ ] C4 按住方向键（WASD/方向键）：玩家向对应方向移动，动画切换；松开后停播。截图（移动中 + Trail 可见为佳）。
- [ ] C5 主动撞怪：玩家消失 → Game Over → 标题 → Start 按钮重现；背景音乐停、死亡音播（听觉，未核实项如实记录）。截图 Game Over 态。
- [ ] C6 再点 Start：清怪、分数归零、玩家回中下方、Get Ready 重现，可再玩一局。

## D. 视觉验收

- [ ] D1 运行画面与 `screenshots/dodge.png` 对比：竖屏、深色背景、顶部大分数，整体一致（文字说明差异点）。
- [ ] D2 三套敌人外观（飞/游/走）在多局或长局中均出现——未出现如实记录为未覆盖，不硬凑。

## 分项索引

行为细节 → [mechanics.md](../gdd/mechanics.md)；HUD 时机 → [ui-hud.md](../gdd/ui-hud.md)；场景结构 → [level-design.md](../gdd/level-design.md)；技术接线 → [tdd.md](../tech/tdd.md)。
