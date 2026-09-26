# GDD 总纲：Control Gallery

本文是文档集的总纲：系统总览、内容范围、完成定义。控件细节见 [mechanics.md](./mechanics.md) 与 [ui-hud.md](./ui-hud.md)；浏览路线见 [level-design.md](./level-design.md)；技术约束见 [../tech/tdd.md](../tech/tdd.md)。

## 系统总览

- **单场景 + 单脚本**：`demo/gui_control_gallery/control_gallery.tscn` 是唯一的运行时内容；`demo/gui_control_gallery/tree.gd` 是唯一的脚本（`@tool`，`extends Tree`），只负责填充 Lists 面板中的 `Tree` 节点。
- **布局骨架**：根 `ControlGallery`（`Control`，全屏锚点）→ `MainPanel`（`ColorRect` 深灰底）→ 顶层 `HSplitContainer`，左侧 `BasicControls` 面板，右侧 `VSplitContainer`（上为 `Numbers` + `Dialogs`/`GraphEdit` 的 `HSplitContainer`，下为 `Lists` 面板）。
- **三面板 + 一**：项目 README 称三个主面板（Basic controls、Numbers、Lists）；实际场景中还有一个标题为 `GraphEdit` 的面板（节点名 `Dialogs`），属现状实录，见 [ui-hud.md](./ui-hud.md)。
- **无状态机**：不存在游戏状态、流程切换、存档写盘；"状态"仅指各个控件自身的展示态（按下/勾选/页签/折叠）。

## 内容范围

| 系统 | 说明 | 详见 |
|---|---|---|
| 基础控件陈列 | 按钮、链接按钮、颜色选择、勾选、输入框、代码编辑、标签、页签 | [ui-hud.md](./ui-hud.md) |
| 数值控件陈列 | SpinBox、HSlider、ProgressBar、HSeparator、TextureProgressBar | [ui-hud.md](./ui-hud.md) |
| 图编辑陈列 | GraphEdit + GraphFrame + GraphNode（静态摆放） | [ui-hud.md](./ui-hud.md) |
| 列表控件陈列 | OptionButton、MenuButton、ButtonGroup 单选、ItemList、Tree、FoldableContainer | [ui-hud.md](./ui-hud.md)、[mechanics.md](./mechanics.md) |
| 浏览与拖拽 | 分隔条拖拽、页签切换、折叠开合 | [mechanics.md](./mechanics.md) |

## 非目标

无玩法系统、无关卡系统、无敌人、无 HUD（游戏意义上的）、无音频、无存档、无本地化。文档中不设这些章节的虚构内容。

## 完成定义

- 本项目现状即完成态：场景在编辑器与运行时正常显示，控件可交互，Tree 内容正常填充。
- 任何对场景/脚本/配置的改动必须同步更新本文档集对应章节，并重新执行 [../test/acceptance.md](../test/acceptance.md) 的顶层场景。
- 文档侧完成定义：11 个文件齐备、全简体中文、引用路径全部真实存在、无虚构玩法描述。
