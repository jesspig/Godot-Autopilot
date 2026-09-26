# 验收标准汇总

顶层场景 A–E 全过即通过；分项索引指向各文档对应章节。本文所有步骤均可在编辑器里执行，无需构建（构建/运行 Godot 不在本写作任务内，执行验证时按 [test-plan.md](./test-plan.md) 约定操作）。

## 场景 A：打开主场景

- 步骤：在编辑器中打开 `demo/gui_control_gallery/control_gallery.tscn`（工作树无 `main_scene`，须手动指定）。
- 预期：场景正常加载，无错误；场景树根为 `ControlGallery`（`Control`）；视口显示深灰底 + 四面板排布。
- 证据：编辑器截图（视口 + 场景树 + 检查器根节点）。

## 场景 B：分隔拖拽与页签

- 步骤：依次拖动顶层 HSplit、右侧 VSplit、右上 HSplit 的分隔条；将 TabContainer 从 Tab 1 切到 Tab 7 再切回。
- 预期：相邻面板尺寸跟随变化；每页 RichTextLabel 文本与页签号对应；无报错。
- 证据：拖拽前后对比截图 + 各页签截图（至少首尾两页）。

## 场景 C：控件交互

- 步骤：运行主场景；逐项试用 Button、CheckBox/CheckButton、LineEdit/TextEdit 输入、HSlider 拖动、OptionButton/MenuButton 下拉、ItemList 点选（含禁用项不可选）、两 FoldableContainer 开合互斥、LinkButton 悬停提示。
- 预期：全部可交互、无错误输出；ButtonGroup 三单选同时最多一选中；折叠组同时最多一展开。
- 证据：运行截图（含划定交互前后的状态）+ 运行日志无报错说明。

## 场景 D：Tree 脚本行为

- 步骤：定位 `MainPanel/HSplitContainer/VSplitContainer/Lists/VBoxContainer/HBoxContainer/VBoxContainer2/Tree`；核对文本与按钮。
- 预期：四项文本为 `Tree - Root TreeItem` / `Tree - TreeItem 1` / `Tree - TreeItem 2` / `Tree - TreeItem 1 Child`；根项带一可用一禁用两个图标按钮。
- 证据：Tree 节点特写截图 + `tree.gd` 挂载关系说明（检查器 `script` 指向 `res://tree.gd`）。

## 场景 E：视觉一致性

- 步骤：将运行/编辑器截图与 `demo/gui_control_gallery/screenshots/control_gallery.webp` 并排对照。
- 预期：面板相对位置、四标题、控件种类一致；仅窗口尺寸/分辨率差异可接受，须书面说明。
- 证据：对照说明 + 标注实际窗口尺寸。

## 分项索引

| 分项 | 位置 |
|---|---|
| 交互规则与 tree.gd 实录 | [../gdd/mechanics.md](../gdd/mechanics.md) |
| 控件属性与布局 | [../gdd/ui-hud.md](../gdd/ui-hud.md) |
| 浏览路线 | [../gdd/level-design.md](../gdd/level-design.md) |
| 美术与资源 | [../art/art-direction.md](../art/art-direction.md)、[../art/asset-catalog.md](../art/asset-catalog.md) |
| 技术约束与配置对照 | [../tech/tdd.md](../tech/tdd.md) |
| 执行与记录规范 | [test-plan.md](./test-plan.md) |
