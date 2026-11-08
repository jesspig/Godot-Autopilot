# GDD 总纲：GUI in 3D

## 系统总览

本演示由两个场景与一个脚本构成，链路为单向三段：

```text
gui_panel_3d.tscn（SubViewport 内建 2D 界面 → ViewportTexture）
    → 同场景 Quad（StandardMaterial3D 贴视口纹理，3×2 米）
    → gui_in_3d.tscn（把面板实例化进 3D 世界，相机/灯光/背景围观）
```

反向有一条输入链：3D 物理拾取（`Quad/Area3D`）→ `gui_3d.gd` 坐标换算 → `SubViewport.push_input`。机制细节的唯一权威是 [mechanics.md](./mechanics.md)。

## 双场景分工

| 场景 | 路径 | 职责 |
|---|---|---|
| 3D 主场景 | `res://gui_in_3d.tscn` | 世界容器：实例化面板、相机漫游、灯光与天空背景、地面/墙体/立方体布景 |
| GUI 面板场景 | `res://gui_panel_3d.tscn` | 功能主体：SubViewport 建 2D 界面、Quad 成像、Area3D 拾取、挂载 `gui_3d.gd` |

主场景对面板是**整体实例引用**（`ExtResource`），面板内部结构变更自动带入主场景。

## 内容范围

- 2D 侧：1 个标签、1 个按钮、1 个单行输入框、横/竖滑杆各 1、下拉框 1（含 3 个选项）、红色 ColorRect、图标 TextureRect。完整清单见 [ui-hud.md](./ui-hud.md)。
- 3D 侧：1 相机（FOV 74、6 秒循环动画）、1 定向光（默认隐藏）、1 全向光、1 天空环境、2 墙 + 2 地面 + 2 立方体布景。完整布置见 [level-design.md](./level-design.md)。
- 配置侧：以源库原件 `project.godot` 为准（主场景、主题缩放 2.0、3D 物理层命名、120 物理 tick、MSAA + debanding、gl_compatibility），工作树现状为最小模板。详见 [tech/tdd.md](../tech/tdd.md)。

## 完成定义

1. 机制：视口成像链与输入转发链均按 [mechanics.md](./mechanics.md) 工作，无断链。
2. 内容：控件与布景与两份 `.tscn` 磁盘现状一致，无缺失引用（纹理、脚本、子场景均可解析）。
3. 证据：[acceptance.md](../test/acceptance.md) 场景 A–G 全部通过，且成像/交互类项留有截图。
4. 诚实：所有“未核实”项保持原样，不得为通过验收而改写成断言。
