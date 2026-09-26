# 3D 场景布置实录（无传统关卡）

本演示**没有传统意义上的关卡**：`gui_in_3d.tscn`（根 `Node3D`，名 `GUIin3D`）是一间静态陈列室 + 一台漫游相机。本文是该场景布置的**唯一权威实录**，只写磁盘现状；观察/点击动线供测试执行用。

## 实例：GUI 面板

- `GUIPanel3D`：整体实例引用 `res://gui_panel_3d.tscn`，位于场景原点附近（实例根无额外变换，原始文件如此）。
- 面板即视觉与交互的唯一主体；其余一切皆为布景。

## 相机与动画

- `Camera3D`：`fov = 74.0`，`near = 0.1`，初始位于 `(0, 0, 3)` 朝向面板。
- `Camera_Move`（`AnimationPlayer`）：`autoplay = Move_camera`，动画长 6.0 秒、循环模式 1（原始取值；通常为循环，语义未核实），轨道为 `Camera3D:transform` 的 4 个关键帧——相机绕面板小幅摆动后回到起点。
- 含义：运行时视角持续变化，可验证面板是真 3D 物体；截图验收须注明动画相位，跨截图逐像素对比前先暂停动画或对齐相位。

## 灯光与环境

- `DirectionalLight3D`：`visible = false`（默认关闭），`shadow_enabled = true`。
- `WorldEnvironment`：`ProceduralSkyMaterial` 灰调天空（天地平线同色），`background_mode = 2`、`tonemap_mode = 4`（原始取值；语义未核实）。
- `OmniLight3D`：位于 `(1.39, 1.24, 2.72)`，`omni_range = 10.0`，`shadow_blur = 3.0`，阴影开启——面板 Quad 为无光照材质（`shading_mode = 0`），此灯主要影响布景。

## 布景（`Background` 下）

| 节点 | 网格 | 作用实录 |
|---|---|---|
| `Wall、Wall2、Wall3` | `PlaneMesh` | 三面围墙，构成房间感 |
| `Floor、Floor2` | `PlaneMesh` | 两块地面 |
| `Cube、Cube2` | `BoxMesh`，浅蓝材质（`albedo (0.72, 0.79, 1.0)`，`roughness = 0.0`） | 装饰立方体，分别位于面板左右两侧高低处 |

布景无碰撞、无交互，仅衬托面板的空间感；材质色值以 `.tscn` 原文为准。

## 观察/点击动线（测试用）

1. **全景位**：默认相机位确认面板、布景、天空同框（对照 `screenshots/gui.png`）。
2. **近读位**：推近至标签文字可读，确认 560×360 界面无拉伸。
3. **点击位**：正对面板，依次点击按钮、输入框、滑杆、下拉框（见 [../test/acceptance.md](../test/acceptance.md) 场景 C–E）。
4. **环绕位**：等待动画播过半程，确认面板随视角变化呈透视变化（真 3D 证据）。
