# 技术设计文档（TDD）

## 配置权威与双版本说明

本演示的工程配置存在两个版本，测试与验收必须区分：

- **源库原件**（`godot-demo-projects/viewport/gui_in_3d/project.godot`，已逐行核对）：下表所列即其全文关键段，是配置断言的权威来源。
- **工作树现状**（本仓库 `demo/viewport_gui_in_3d/project.godot`）：**最小模板**，仅含 `config/name="GUI in 3D"`、`config/features=PackedStringArray("4.7")`、`config/icon="res://icon.webp"`，其余段缺失。凡验收涉及配置，必须先确认当前打开的是哪个版本，不得把原件的值当成工作树的现状。

## 源库原件关键配置

| 段/键 | 值 | 用途 |
|---|---|---|
| `application/run/main_scene` | `res://gui_in_3d.tscn` | 主场景 |
| `application/config/description` | 官方英文简介 | 与 `README.md` 同文 |
| `application/config/tags` | `3d, demo, gui, official` | 分类标签 |
| `debug/gdscript/warnings/untyped_declaration` | `1` | 未类型化声明警告 |
| `gui/theme/default_theme_scale` | `2.0` | 默认主题双倍缩放（截图比对基线） |
| `layer_names/3d_physics/layer_2` | `Control` | 物理层 2 命名，与 `Area3D collision_layer = 2` 对应 |
| `physics/common/physics_ticks_per_second` | `120` | 物理 tick（拾取回调频率相关，定量关系未核实） |
| `rendering/renderer/rendering_method(+.mobile)` | `gl_compatibility` | 兼容渲染器（与官方 README 的 `Renderer: Compatibility` 一致） |
| `rendering/anti_aliasing/quality/msaa_3d` | `2` | 3D MSAA |
| `rendering/anti_aliasing/quality/use_debanding` | `true` | 去色带 |

## 硬约束

1. 2D 界面必须位于 `SubViewport` 内，不得移入主视口；Quad 材质必须保持视口纹理绑定。
2. 输入转发必须经过 `gui_3d.gd` 的既有分支（直投/换算），不得新增绕过脚本的拾取路径。
3. `Area3D` 碰撞层必须与命名为 `Control` 的物理层一致（原件层 2）。
4. 文档与测试不得运行 Godot、不得构建、不得改动 `docs/` 之外任何文件。

## 实现自由度

本演示已完整实现，无待开发功能；以下自由度仅适用于“为测试需要搭建的临时验证动作”（且不得落盘为改动）：切换相机视角、暂停漫游动画以对齐截图相位、调整编辑器视口布局以同框展示场景树/检查器。任何涉及场景与脚本的修改都不在自由度内。

## 唯一写盘说明

本演示运行时**无存档、无写盘需求**（无可写盘数据）。测试中唯一允许的写盘是测试报告与截图证据，归档于仓库 `report/`（见 [../test/test-plan.md](../test/test-plan.md)），不得写入演示工程目录。
