# 一页纸说明：Control Gallery

## 一句话概念

一个单场景的 Godot **UI 控件陈列室**：把常用 `Control` 节点摆在同一屏里，每个控件旁标注名称，开箱即看、即点即试。灵感来自 GTK 等 GUI 工具包的 control gallery（见 `demo/gui_control_gallery/README.md`）。

## 非目标（本项目明确没有的东西）

- 无玩法、无胜负、无得分、无敌人、无关卡系统、无存档。
- 无游戏循环（主菜单 → 游玩 → 结算这类结构不存在）。
- 本页以下"机制"位置只描述真实存在的交互，不虚构玩法。

## 真实的交互模型

1. **浏览**：单场景 `demo/gui_control_gallery/control_gallery.tscn` 即全部内容；三个主面板（Basic controls、Numbers、Lists，外加 GraphEdit 面板）并排/上下排列，一眼看全。
2. **分隔拖拽**：面板之间由 `HSplitContainer` / `VSplitContainer` 分隔，拖动面板间空隙可调整各区域大小（原文："Drag the empty space between panels to resize them"）。
3. **控件试用**：每个展示的控件都是真实可交互实例——按钮可点、勾选可切、输入框可打字、滑块可拖、页签可换、列表可点选、折叠容器可开合（具体一览见 [../gdd/ui-hud.md](../gdd/ui-hud.md)；引擎默认行为未逐项实测处标注"未核实"）。

## 范围

- 在内：主场景的面板组织、四组子资源（`StyleBoxFlat`、`CodeHighlighter`、`ButtonGroup`、`FoldableGroup`）、`tree.gd` 的 Tree 填充行为、工程配置与素材。
- 在外：任何玩法数值、关卡几何、敌人 AI、HUD 规则——均不存在，不写。

## 成功标准

- 在编辑器中打开主场景，三面板排布与截图 `demo/gui_control_gallery/screenshots/control_gallery.webp` 一致（允许窗口尺寸差异）。
- 分隔条可拖、控件可交互、Tree 节点显示根/子项文本（见 [../test/acceptance.md](../test/acceptance.md)）。
- 所有文档引用的文件路径真实存在，无编造。

## E2E 测试床定位

本工程是 GDA **编辑器 UI 自动化**的最小试验场：节点数量多、类型全、层级深，适合验证"打开场景 → 场景树定位 → 检查器核对属性 → 视口截图 → 运行截图"整条链路。测试计划见 [../test/test-plan.md](../test/test-plan.md)。
