# 关卡说明：本项目无关卡系统

**如实声明：Control Gallery 没有关卡系统。** 无地图、无出生点、无终点、无难度曲线。本文件转而记录与之对应的真实结构：单场景内的面板分区与建议浏览路线。

## "关卡"的对应物：面板分区

| 分区 | 位置 | 内容 |
|---|---|---|
| BasicControls | 左列整列 | 输入与文本类控件 + 7 页签 |
| Numbers | 右上左格 | 数值与进度类控件 |
| GraphEdit（节点名 Dialogs） | 右上右格 | 图编辑控件静态摆放 |
| Lists | 右下整块 | 选择与层级类控件 + Tree + 折叠容器 |

分区间由三处可拖拽分隔（顶层 HSplit、右侧 VSplit、右上 HSplit，抓取厚度均为 24），"地形"即此分隔结构。

## 浏览路线（验收走场顺序）

1. 左列自上而下：按钮 → 链接按钮（悬停看提示）→ 颜色按钮 → 勾选 → 输入框 → 代码编辑 → 页签 1–7 逐一切换。
2. 右上左：SpinBox → HSlider → ProgressBar → 分隔线 → TextureProgressBar。
3. 右上右：GraphEdit 框选/缩放外观（静态，无逻辑；行为未核实）。
4. 右下：OptionButton 下拉 → MenuButton 菜单 → 三单选 → ItemList（含禁用项）→ Tree 四项 → 两折叠容器开合互斥。

## 显示与拉伸

- 源库原件拉伸配置：`window/stretch/mode="canvas_items"`、`aspect="expand"`（窗口放大时内容扩展，见 [../tech/tdd.md](../tech/tdd.md)）；工作树最小模板未含该配置，实际拉伸表现以编辑器实测截图为准。
- 视口尺寸策略：未核实（两版 `project.godot` 均无明确视口宽高声明），验收截图需标注实际窗口尺寸。

## 关卡验收（对应物）

- [ ] 三面板 + GraphEdit 面板一次同屏可见（或经分隔拖拽后可见）。
- [ ] 上述浏览路线全程无报错、无控件渲染缺失。
- [ ] 与 `demo/gui_control_gallery/screenshots/control_gallery.webp` 对照，面板相对位置一致。
