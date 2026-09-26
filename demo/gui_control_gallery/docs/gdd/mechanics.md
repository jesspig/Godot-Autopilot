# 交互机制实录

本文是交互规则与脚本行为的**唯一权威**。控件属性与布局见 [ui-hud.md](./ui-hud.md)；浏览路线见 [level-design.md](./level-design.md)；资源清单见 [../art/asset-catalog.md](../art/asset-catalog.md)。

**前置声明：本项目无玩法/胜负/数值系统。** 以下"机制"指真实存在的编辑器/运行时交互，不虚构任何玩法规则。

## 启动行为

- 工作树内 `demo/gui_control_gallery/project.godot` 为最小模板，**未声明 `run/main_scene`**（已逐行核实：仅 `config/name`、`config/features`、`config/icon`）。
- 源库原件声明主场景为 `res://control_gallery.tscn`，并附低处理器模式、拉伸、vsync、渲染法配置（见 [../tech/tdd.md](../tech/tdd.md)）。
- 含义：在当前工作树工程里，"启动即进入陈列室"未核实；验证时以**在编辑器中手动打开 `control_gallery.tscn`** 为入口（见 [../test/acceptance.md](../test/acceptance.md) 场景 A）。

## 不变式（任何改动都必须成立）

- 单场景即全部内容：不得引入第二主场景、游戏状态机、存档写盘。
- `tree.gd` 只填充其所挂载的 `Tree` 节点，不触碰其他节点。
- 三面板 + GraphEdit 面板的分组不被打散；分隔容器层级保持可拖拽。
- 每个展示控件保留其名称标注（文本或相邻 Label），不得出现无名控件。
- 子资源（`StyleBoxFlat`、`CodeHighlighter`、`ButtonGroup`、`FoldableGroup`）的引用关系保持有效。

## 交互模型

| 交互 | 参与节点 | 行为要求 |
|---|---|---|
| 分隔拖拽 | 顶层 `HSplitContainer`、右侧 `VSplitContainer` 及其内层 `HSplitContainer` | 拖动面板间空隙调整相邻区域大小；分隔条最小抓取厚度 24（`minimum_grab_thickness = 24`，场景实录） |
| 页签切换 | `BasicControls` 内 `TabContainer`（Tab 1–7） | 点击页签头切换可见页；每页显示对应 `RichTextLabel` BBCode 文本 |
| 控件试用 | 各面板内控件实例 | 保留引擎默认交互（点击/勾选/输入/拖动/下拉）；默认行为细节未逐项实测，验收只要求"可交互、无报错" |
| 列表点选 | `OptionButton`、`MenuButton`、`ItemList`、ButtonGroup 三勾选 | 点选反馈正常；`ItemList` 禁用项不可选；ButtonGroup 内同时最多一选中（引擎默认语义，未核实处以实测截图为准） |
| 折叠开合 | 两个 `FoldableContainer`（共享 `FoldableGroup`，`allow_folding_all = true`） | 同组同时最多一展开（`FoldableGroup` 语义）；第二容器初始为折叠（`folded = true`） |
| 提示条 | `LinkButton`（`tooltip_text` 非空） | 悬停显示提示文本；提示文案提及的 TooltipPanel/TooltipLabel Theme 类在工程内无对应 Theme 资源，属文案引用现状 |

## tree.gd 行为（逐行实录）

文件：`demo/gui_control_gallery/tree.gd`（全文 18 行，`@tool`，`extends Tree`），挂载于 `.../Lists/VBoxContainer/HBoxContainer/VBoxContainer2/Tree`。

- `_ready()` 创建根 `TreeItem`，文本 `Tree - Root TreeItem`。
- 预加载 `res://icon.webp`，取图像并 `resize(16, 16)`，生成 `ImageTexture` 后给根项添加**两个按钮**：一个可用、一个禁用（`disabled` 分别为 `false`/`true`），提示文本分别为 `Example TreeItem button.` / `Example disabled TreeItem button.`。
- 创建 `child1`（`Tree - TreeItem 1`）、`child2`（`Tree - TreeItem 2`），以及 `child1` 的子项 `subchild1`（`Tree - TreeItem 1 Child`）。
- `@tool` 含义：该填充在编辑器内同样生效，打开场景即可在 Tree 节点看到上述四项；未核实编辑器反复开关场景是否重复追加（`create_item` 在 `_ready` 的语义按引擎默认，标注未核实）。

## 边界情况清单

- 工作树 `project.godot` 无主场景声明：从工程根直接运行进入什么场景，**未核实**，不写结论。
- GraphEdit / GraphFrame / GraphNode 为静态摆放，无连接线、无代码逻辑；拖拽节点等行为未核实。
- `TextureProgressBar`（值 67、填充模式 4）为静态展示值，无驱动代码；是否随时间变化：否（无相关代码，静态）。
- `CodeEdit` 内示例代码（`func get_engine_name(): return "Godot Engine"`）为展示文本，不被执行。
- `SpinBox`、`HSlider`、`ProgressBar` 之间无绑定代码（互不联动，场景内无相关脚本）。

## 交互验收

- [ ] 编辑器打开主场景无报错；Tree 节点显示根 + 2 子项 + 1 孙项文本。
- [ ] 拖动三处分隔条，相邻面板尺寸跟随变化。
- [ ] Tab 1–7 可逐一切换，每页 RichTextLabel 文本与页签对应。
- [ ] 按钮/勾选/输入框/滑块/下拉/列表/折叠容器均可交互且无错误输出。
- [ ] 运行画面与编辑器视口排布一致；截图见 [../test/acceptance.md](../test/acceptance.md)。
