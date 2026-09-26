# 美术方向

## 基调

默认 Godot 主题 + 深灰陈列底：`MainPanel`（`ColorRect`，`#363636` 量级）衬四块稍浅面板（`#4F4F4F` 量级圆角面板）。无自定义主题资源（场景内无 `Theme`，仅 `StyleBoxFlat` 修饰面板），呈现即"引擎默认控件长什么样"。

## 排版规则

- 面板标题：24px 居中 Label（`Basic controls` / `Numbers` / `GraphEdit` / `Lists`）。
- 每个展示控件自带名称标注：或自身 `text`（如 `Button`），或相邻 Label（如 `HSlider`、`TextureProgressBar`）。
- 面板内边距 10（`StyleBoxFlat` 内容边距），圆角 5；分隔条抓取厚度 24、编辑器内高亮开（`drag_area_highlight_in_editor = true`）。

## 图标复用

`icon.webp`（工程图标）在场景内两处复用：`TextureProgressBar` 的进度纹理、Tree 根项的两个 16×16 按钮图标（`tree.gd` 内 `resize(16, 16)` 生成）。

## 代码编辑配色

`CodeHighlighter` 子资源实录：数字黄、符号青灰、函数青、成员变量橙；关键字 `func` 偏红、`return` 偏紫；字符串区暖黄、注释区灰。验收以编辑器截图为准，不逐色断言数值。

## 渲染

源库原件指定 `gl_compatibility`（兼容渲染）；项目 README 亦标注 Renderer: Compatibility。工作树最小模板无渲染声明，实际以编辑器实测为准。
