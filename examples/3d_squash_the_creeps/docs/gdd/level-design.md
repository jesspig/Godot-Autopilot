# 关卡实录：竞技场

本文是场景几何与布置的**唯一权威**。行为数值见 [mechanics.md](./mechanics.md)；节点接线见 [../tech/tdd.md](../tech/tdd.md)。全部数据抄自 `Main.tscn`。

## 场地总览

- 单一竞技场，无多关卡、无出生点选择：玩家实例放在主场景原点附近（`Player` 实例节点无额外变换，即场景原点）。
- 地面：`Ground`（`StaticBody3D`，`transform` 原点 y = -1），盒形碰撞 60×2×60，顶面约 y = 0（按碰撞半高 1 推算；视觉网格同尺寸）。
- 边界：`Walls`（`StaticBody3D`）下挂 4 个 `WorldBoundaryShape3D` 无限平面，呈方形围住场地（编辑器中显示为有限平面，实际无限大；节点 `editor_description` 原文如此）。
- 装饰：`Cylinders` 下 4 根红柱（`CylinderMesh`，24 径向段），位置分别在 (14,1,-15)、(-13,1,-15)、(-13,1,16)、(14,1,16)（`Cylinders` 父节点另有 x = 8 的偏移，上述为子节点本地坐标；世界坐标需叠加，未逐项展开）。
- 装饰柱是否参与碰撞：`Cylinders` 为纯 `Node3D` + `MeshInstance3D`，无碰撞体，应为纯装饰（以 tscn 为准）。

## 生成路径

- `SpawnPath`（`Path3D`，位于 y = 1）曲线 5 点，矩形环线（`Curve3D` 数据点按 tscn 记录）：
  - (14,0,-15) → (-13,0,-15) → (-13,0,16) → (14,0,16) → 回到 (14,0,-15）。
- `SpawnLocation`（`PathFollow3D`，`rotation_mode = 1`、`loop = false`、`cubic_interp = false`），生成时取 `progress_ratio = randf()` 随机点，高度为路径高度（y = 1）。
- 生成高度与落地关系：Mob 生成后直线移动，重力与地面碰撞由其 `CharacterBody3D` 承担（`Mob.gd` 每帧 `move_and_slide`）。

## 相机与视角

- `CameraPivot`（`Marker3D`，位于 (0.183514, 0, 0)，俯仰约 45 度），子节点 `Camera`（`Camera3D`，本地 (0, 0, 19)）。
- tscn 记载：`projection = 1`、`fov = 48.6`、`size = 24.0`、`far = 40.0`（`projection = 1` 对应的投影类型此处不转述枚举含义，未核实）。
- 相机固定不跟随（场景中无跟随脚本）；玩家在场地内移动，视角为固定斜俯视。验证时以运行截图为准。

## 灯光与环境

- `DirectionalLight3D`：启用阴影（`shadow_enabled`），`shadow_bias = 0.04`、`shadow_blur = 1.5`、`directional_shadow_mode = 0`、`fade_start = 1.0`、`max_distance = 40.0`。
- `WorldEnvironment`：`ProceduralSkyMaterial` 天空 + `Environment`（`background_mode = 2`、`tonemap_mode = 4`）。
- 低端回退：`Main.gd` `_ready()` 中若渲染方法为 `gl_compatibility`，则提升软阴影质量并复制一盏方向光补光（能量 0.35）。触发条件与效果以代码为准，未在编辑器静态验证。

## 物理层归属（场景侧）

- `Ground` 与 `Walls`：`collision_layer = 4`（world），`collision_mask = 0`。
- 玩家与小怪的层/掩码见 [tdd.md](../tech/tdd.md)。
