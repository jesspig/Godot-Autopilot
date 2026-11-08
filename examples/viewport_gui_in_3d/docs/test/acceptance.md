# 验收标准汇总

顶层场景 A–G 全部通过方为通过；其中 B/C/D/E/F 为视觉验收，必须留截图。分项细节索引见每条后的链接。

## 场景 A：主场景打开与首帧成像

- 经 GDA 打开 `res://gui_in_3d.tscn`，场景树含 `GUIPanel3D` 实例、`Camera3D`、`Camera_Move`、`WorldEnvironment`、灯光与 `Background` 布景（编辑器截图）。
- 运行后首帧：2D 面板完整成像于 Quad，无缺块、无拉伸，与 `res://screenshots/gui.png` 布局一致（运行截图）。
- 索引：[gdd](../gdd/gdd.md) 分工表、[level-design](../gdd/level-design.md) 观察动线第 1 步。

## 场景 B：视口成像链完整

- Quad 长宽比（3:2）与视口（560:360）一致，文字无拉伸；红色块、图标纹理与官方截图色彩一致。
- 鼠标悬停 Quad 出现控件悬停态，离开后清除（动作前后运行截图）。
- 索引：[mechanics](../gdd/mechanics.md) 成像链与进出通知、[ui-hud](../gdd/ui-hud.md) 全局呈现约定。

## 场景 C：按钮点击与文本输入（UI 点击能力核心项）

- 点击 3D 中的 `A button!`，可见按下反馈（前后截图）。
- 聚焦输入框并输入字符串，字符可见，占位文本 `Enter text here...` 行为正常（前后截图）。
- 索引：[mechanics](../gdd/mechanics.md) 分支一/分支二、[ui-hud](../gdd/ui-hud.md) 左侧表单列。

## 场景 D：滑杆拖动

- 横向拖动 `HSlider`、纵向拖动 `VSlider`，手柄位置跟随，无跳变（拖动前后截图）。
- 拖出 Quad 范围再松开，行为不断链（问题记录如有异常按五要素记录）。
- 索引：[mechanics](../gdd/mechanics.md) 相对位移计算与边界清单。

## 场景 E：下拉框弹窗

- 点击 `OptionButton` 展开 3 项（`Item 0/1/2`），弹窗成像于 Quad 内（展开态截图）。
- 选中一项后回显正确（选中后截图）。
- 索引：[mechanics](../gdd/mechanics.md) `gui_embed_subwindows` 行、[ui-hud](../gdd/ui-hud.md) 右侧展示列。

## 场景 F：真 3D 性（相机动画）

- `Camera_Move` 以约 6 秒周期循环播放；动画不同相位下，面板呈透视变化（至少两张不同相位截图）。
- 面板在漫游中始终可交互（任选一控件复点一次）。
- 索引：[level-design](../gdd/level-design.md) 相机与动画、观察动线第 4 步。

## 场景 G：结构与配置一致性（只读）

- `gui_panel_3d.tscn` 直连结构：`SubViewport(560×360)` → `ViewportTexture` → Quad 材质；`Area3D` 碰撞层 2、拖拽捕获开启；脚本挂载与三信号连接与 [mechanics](../gdd/mechanics.md) 一致。
- 配置：主场景指向、`default_theme_scale = 2.0`、物理层 2 命名 `Control`、120 tick、gl_compatibility、MSAA 2、debanding——以**源库原件**为准；工作树 `project.godot` 为最小模板的现状如实记录，不得混淆两者。
- 索引：[tdd](../tech/tdd.md) 双版本说明、[asset-catalog](../art/asset-catalog.md) 清单。

## 分项索引

| 分项 | 权威文档 |
|---|---|
| 转发规则、换算步骤、边界 | [gdd/mechanics.md](../gdd/mechanics.md) |
| 控件初始文本与期望响应 | [gdd/ui-hud.md](../gdd/ui-hud.md) |
| 相机/灯光/布景与动线 | [gdd/level-design.md](../gdd/level-design.md) |
| 色彩/风格基线 | [art/art-direction.md](../art/art-direction.md) |
| 文件存在性 | [art/asset-catalog.md](../art/asset-catalog.md) |
| 配置值与自由度 | [tech/tdd.md](../tech/tdd.md) |
| 执行方法与证据规范 | [test-plan.md](./test-plan.md) |
