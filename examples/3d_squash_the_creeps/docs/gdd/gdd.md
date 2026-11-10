# GDD 总纲（现状实录）

本文是系统总览与内容范围说明。数值以 [mechanics.md](./mechanics.md) 为准，几何以 [level-design.md](./level-design.md) 为准，资产以 [../art/asset-catalog.md](../art/asset-catalog.md) 为准，接线以 [../tech/tdd.md](../tech/tdd.md) 为准。

## 系统总览

- 主场景：`Main.tscn`（根为 `Node`，挂 `Main.gd`，`mob_scene` 导出指向 `Mob.tscn`）。
- 玩家：`Player.tscn` 实例（`CharacterBody3D` + `Player.gd`），含球形碰撞体、Area3D 死亡检测器（`MobDetector`）、动画播放器、glb 角色模型。
- 小怪：运行时由 `MobTimer`（0.5 秒）从 `Mob.tscn` 实例化，经 `SpawnPath/SpawnLocation` 随机位置生成，`initialize()` 朝向玩家后直线冲锋。
- 计分：`UserInterface/ScoreLabel`（挂 `ScoreLabel.gd`），小怪 `squashed` 信号直连其 `_on_Mob_squashed`，每次 +1。
- 失败：玩家 `hit` 信号直连 Main 的 `_on_player_hit`，停止生成计时器并显示 `UserInterface/Retry` 遮罩；`_unhandled_input` 监听 `ui_accept` 重载当前场景。
- 音乐：`MusicPlayer.tscn`（`AudioStreamPlayer`，自动播放）在源库原件中注册为 autoload，工作树模板现状见 [tdd.md](../tech/tdd.md)。

## 内容范围

| 在内 | 说明 |
|---|---|
| 竞技场 | 地面、隐形边界墙、4 根装饰柱、方向光、天空环境、正交相机 |
| 玩家 | 移动、跳跃、压扁弹跳、被撞死亡 |
| 小怪 | 一种，随机速度直线冲锋，出屏销毁 |
| UI | 左上分数、失败重试遮罩 |
| 音乐 | 自动循环播放（源库配置；实际循环点未核实） |

| 在外 | 说明 |
|---|---|
| 菜单/选关/暂停/存档 | 磁盘上不存在相关场景与代码 |
| 第二种敌人/道具 | 不存在 |
| C# 版 | 官方 README 提及另有 C# 版，本仓库 demo 仅 GDScript 版 |

## 完成定义（现状即完成）

- 11 份文档与磁盘文件一致；凡引用路径真实存在；不确定处标"未核实"。
- 按 [../test/acceptance.md](../test/acceptance.md) 的验证点在编辑器里可逐项执行并截图取证。
