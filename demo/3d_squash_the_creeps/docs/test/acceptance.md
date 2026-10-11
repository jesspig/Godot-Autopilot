# 验收标准（可在编辑器里执行）

本文是顶层验收汇总。分项细节见括号内引用。每条均按"打开→操作→期望→截图"编写。

## A. 场景结构（编辑器静态检查）

- A1 打开 `Main.tscn`：场景树含 `Ground`、`Walls`、`DirectionalLight3D`、`WorldEnvironment`、`Player`、`CameraPivot/Camera`、`Cylinders`、`SpawnPath/SpawnLocation`、`MobTimer`、`UserInterface/ScoreLabel`、`UserInterface/Retry`（见 [tdd.md](../tech/tdd.md)）。期望：节点齐全，`mob_scene` 指向 `Mob.tscn`。截图：编辑器视口 + 场景树。
- A2 打开 `Player.tscn`：`MobDetector`（Area3D，mask 含 enemies）下挂 5 个碰撞体；`AnimationPlayer` 自动播放 `float`。截图：场景树 + 检查器。
- A3 打开 `Mob.tscn`：根在 `mob` 组，`collision_layer = 2`；`VisibleOnScreenNotifier3D` 存在且已连接出屏自删。截图：检查器 + 连接面板。
- A4 打开 `MusicPlayer.tscn`：`AudioStreamPlayer`，`stream` 为 `art/House In a Forest Loop.ogg`，`autoplay = true`。截图：检查器。

## B. 运行基本行为

- B1 运行 `Main.tscn`：3D 斜俯视竞技场，地面、红柱、天空可见，玩家角色在场中央，左上显示 `Score: 0`，无重试遮罩。按键 WASD/方向键移动，角色转向与移动方向一致。截图：运行画面（含角色 + HUD）。
- B2 按空格跳跃：角色上跳后回落，跳跃弧线带俯仰（见 [mechanics.md](../gdd/mechanics.md)）。截图：起跳与腾空各一张。
- B3 等待 0.5 秒×N：小怪从场地边缘持续生成并冲向玩家（速度各异）。截图：小怪与玩家同框。
- B4 跳到小怪正上方落下：小怪消失，分数 +1，玩家弹跳。截图：压扁前后（含分数变化）。
- B5 被小怪正面撞到：玩家消失，生成停止，半透明重试遮罩显示 `Press Space or Enter to retry`，分数停留。截图：重试遮罩 + 分数。
- B6 按空格/回车：场景重载，分数回 0，玩家回到初始位，小怪重新生成。截图：重载后的初始画面。

## C. 视觉验收

- C1 3D 视角：固定斜俯视，整个竞技场与 UI 同框，无穿帮黑屏（以运行截图为准）。
- C2 动画：玩家移动时浮动加快、小怪按速度浮动（目测即可，如实记录是否可辨）。
- C3 HUD：分数黑色 32 号左上；重试遮罩半透明黑全屏、白字居中。
- C4 对照：与 `screenshots/squash_the_creeps.webp` 对比，大体一致即可（细节差异如实记录）。

## D. 配置验收（读回即过，注意模板差异）

- D1 输入动作（源库原件）：`move_left/right/forward/back`、`jump` 五动作存在，键位与手柄映射如 [mechanics.md](../gdd/mechanics.md)。若以工作树模板启动缺失，当场记录，不判失败归因于游戏。
- D2 物理层名：`player/enemies/world`；`Ground/Walls` 层为 world；Mob 层为 enemies。
- D3 物理：120 tick、Jolt、插值开启（源库原件值）。
- D4 自动加载：`MusicPlayer`（源库原件）；运行有背景音乐（有/无如实记录）。
- D5 显示与导入：拉伸 `canvas_items/expand`；`import/blender/enabled=false`；MSAA 3D=2。

## 分项索引

- 数值：[mechanics.md](../gdd/mechanics.md)；几何：[level-design.md](../gdd/level-design.md)；UI：[ui-hud.md](../gdd/ui-hud.md)；资产：[asset-catalog.md](../art/asset-catalog.md)；接线：[tdd.md](../tech/tdd.md)。
