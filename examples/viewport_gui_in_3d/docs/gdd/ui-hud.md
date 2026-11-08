# 2D 面板规格（控件清单与信息契约）

本文是 `SubViewport/GUI` 内控件的**唯一权威清单**，路径均以 `gui_panel_3d.tscn` 内 `SubViewport/GUI` 为根。转发规则见 [mechanics.md](./mechanics.md)。

## 容器

| 节点 | 类型 | 实录 |
|---|---|---|
| `GUI` | Control | 全视口矩形（`offset_right = 560`，`offset_bottom = 360`），`mouse_filter = 1`（原始取值；语义未核实） |
| `GUI/Panel` | Panel | 全锚点铺满视口的底板 |

## 左侧表单列（`Panel/VBoxContainer`）

容器：上 20、左 20、右至 319、下留 20，项间距 13。内含 4 个控件：

| 控件 | 初始文本/属性 | 期望响应（验证点） |
|---|---|---|
| `Label` | `SubViewport is rendered on Quad`，居中、自动换行，最小宽 200 | 静态展示；成像后文字清晰可读 |
| `Button` | `A button!` | 点击后出现按下/悬停视觉反馈 |
| `LineEdit` | 占位 `Enter text here...` | 聚焦后键盘输入可见；占位文本在空值时显示 |
| `HSlider` | `step = 0.0`（连续），`ticks_on_borders = true` | 可横向拖动，手柄位置跟随 |

## 右侧展示列（锚定面板右上）

| 控件 | 实录 | 期望响应 |
|---|---|---|
| `ColorRect` | 纯红 `Color(1, 0, 0, 1)`，右上区域块 | 静态色块，验证成像色彩保真 |
| `TextureRect` | 纹理 `res://icon.webp`，`expand_mode = 1`（原始取值；语义未核实） | 图标可见，验证纹理链路 |
| `VSlider` | `step = 0.0`（连续），锚定右侧自上而下 | 可纵向拖动 |
| `OptionButton` | 3 项：`Item 0 / Item 1 / Item 2`，当前选中 0 | 点击展开下拉（弹窗须成像于 Quad，依赖 `gui_embed_subwindows`），选中项回显 |

## 全局呈现约定

- 源库原件默认主题缩放 `2.0`（见 [../tech/tdd.md](../tech/tdd.md)）：所有控件按双倍主题尺寸渲染，截图比对时以此为准。
- 文本初始值（标签句、按钮句、占位句、选项名）即验收基线，不得改写后验收。
- 本面板无 HUD、无菜单状态机：控件即全部界面，不存在界面跳转。
