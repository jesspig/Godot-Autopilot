# 控件一览与行为（重点）

本文是场景结构与控件属性的**唯一权威**，逐行依据 `demo/gui_control_gallery/control_gallery.tscn`（634 行）记录。脚本行为见 [mechanics.md](./mechanics.md)。

## 布局骨架

```text
ControlGallery (Control，全屏锚点)
└── MainPanel (ColorRect，深灰底 #363636 量级)
    └── HSplitContainer（顶层，抓取厚度 24）
        ├── BasicControls (PanelContainer)
        │   └── VBoxContainer（标题 + 12 组控件）
        └── VSplitContainer（stretch 2.0，split_offset 242）
            ├── HSplitContainer（split_offset 375）
            │   ├── Numbers (PanelContainer)
            │   └── Dialogs (PanelContainer，标题却写 "GraphEdit")
            └── Lists (PanelContainer，stretch 2.5)
```

面板容器均使用同一 `StyleBoxFlat` 子资源：底色 `#4F4F4F` 量级、四周内容边距 10、圆角 5。面板标题均为 24px 居中 `Label`。

## BasicControls 面板

| 控件 | 类型 | 关键属性实录 |
|---|---|---|
| Title | Label | 文本 `Basic controls`，24px，居中 |
| Button | Button | 文本 `Button` |
| LinkButton | LinkButton | 文本 `LinkButton (hover me for tooltip)`；`tooltip_text` 为多行提示（含 TooltipPanel/TooltipLabel 文案） |
| ColorPickerButton + Label | HBoxContainer | 颜色按钮初始色 `#478CC0` 量级（`Color(0.278431, 0.54902, 0.74902, 1)`）；旁标 `ColorPickerButton` |
| CheckBox | CheckBox | 文本 `CheckBox` |
| CheckButton | CheckButton | 文本 `CheckButton` |
| LineEdit | LineEdit | 文本 `LineEdit`，占位 `LineEdit placeholder` |
| TextEdit | TextEdit | 多行文本（含 "Unlike LineEdit, I accept multiple lines."），最小高 96，占位 `TextEdit placeholder`，`tab_input_mode = false` |
| CodeEdit | CodeEdit | 示例 GDScript 文本；挂 `CodeHighlighter` 子资源（数字黄/符号青灰/函数青/成员变量橙，关键字 `func`/`return` 配色，字符串与注释区域配色）；开启行号/折叠/断点槽/书签/补全/自动缩进/括号补全等；最小高 122 |
| Label | Label | 文本 `Label` |
| TabContainer | TabContainer | 最小高 140；Tab 1–7，每页为 `MarginContainer` + `RichTextLabel`（BBCode 居中文本，标明当前页签，如 `Tab 1 is selected`，可选词、右键菜单开） |

## Numbers 面板

| 控件 | 类型 | 关键属性实录 |
|---|---|---|
| Title | Label | 文本 `Numbers`，24px，居中 |
| SpinBox | SpinBox | 前缀 `SpinBox`，最小宽 255 |
| HSlider + Label | HBoxContainer | `HSlider` 值 50（横向扩展），旁标 `HSlider` |
| ProgressBar + Label | HBoxContainer | `ProgressBar` 值 50（横向扩展），旁标 `ProgressBar` |
| HSeparator ×2 + Label | HBoxContainer | 左右分隔线夹中间 `HSeparator` 文字的对称展示 |
| TextureProgressBar + Label | HBoxContainer | 64×64 `Control` 内嵌 `TextureProgressBar`（`offset 128` 缩放 0.5，值 67，`fill_mode = 4`，纹理为 `icon.webp` 外部资源）；旁标 `TextureProgressBar` |

## Dialogs / GraphEdit 面板

- 节点名 `Dialogs`，其标题 Label 文本却为 `GraphEdit`——现状实录，改动时注意名实差异。
- 内容：`GraphEdit`（纵向扩展，`minimap_size 72×72`）内含 `GraphFrame`（标题 `GraphFrame`，位置偏移 `(111, 52)`），框内再嵌 `GraphNode`（标题 `GraphNode`）。均为静态摆放，无连接、无逻辑代码。

## Lists 面板

| 控件 | 类型 | 关键属性实录 |
|---|---|---|
| Title | Label | 文本 `Lists`，24px，居中 |
| OptionButton | OptionButton | 5 项：`OptionButton` / `Item 1` / `Item 2` / 分隔符 / 禁用项 `Disabled Item` |
| MenuButton | MenuButton | 文本 `MenuButton`，8 项：动作项、分隔符、勾选项 ×2、禁用勾选项、分隔符、单选项 ×2（首单选已选中） |
| RadioButtons | VBoxContainer | 3 个 `CheckBox` 共享同一 `ButtonGroup` 子资源实现单选，第一项初始按下 |
| ItemList | ItemList | 4 项，多选模式关闭（`select_mode = 1` 单选）：Item 1、Item 2、禁用带图标项（图标 `icon.webp`）、带图标 Item 3 |
| Tree | Tree | 挂 `tree.gd`，`select_mode = 2`（多选）；内容由脚本填充（根 + 2 子 + 1 孙，根带两按钮），见 [mechanics.md](./mechanics.md) |
| FoldableContainer ×2 | FoldableContainer | 共享 `FoldableGroup`（`allow_folding_all = true`）；第一展开（说明"同组一次只开一个"），第二初始折叠（`folded = true`，标题在下 `title_position = 1`） |

## 子资源

| 子资源 | 用途 |
|---|---|
| `StyleBoxFlat_bl4wp` | 四面板底样式（边距 10、圆角 5、灰底） |
| `CodeHighlighter_wdd8d` | CodeEdit 语法高亮配色 |
| `ButtonGroup_t0nh8` | 三 CheckBox 单选互斥 |
| `FoldableGroup_wdd8d` | 两折叠容器同时最多一展开 |

## 非 HUD 声明

本项目没有游戏意义上的 HUD（血条/计数/小地图等）；`GraphEdit` 的小地图（minimap）是控件自带预览，不是游戏 HUD。
