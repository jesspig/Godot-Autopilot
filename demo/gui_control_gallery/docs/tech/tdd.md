# 技术设计文档（TDD）

本文规定硬约束与实现自由度。行为规格见 [../gdd/mechanics.md](../gdd/mechanics.md)；控件属性见 [../gdd/ui-hud.md](../gdd/ui-hud.md)。

## 硬约束

1. **Godot 4.7**：`config/features=PackedStringArray("4.7")`；用 4.x 编辑器打开与验证。
2. **UID 不可手改**：场景/脚本/图标的 `uid://` 引用与 `tree.gd.uid` 由编辑器维护。
3. **`tree.gd` 为 `@tool`**：编辑器内即运行；修改后必须在编辑器与运行时分别确认 Tree 内容正常且无重复追加（重复语义未核实，以实测为准）。
4. **单脚本原则**：新增逻辑优先挂在现有结构下；新增 `.gd` 必须同步更新本文档与 [asset-catalog.md](./asset-catalog.md)，并说明挂载点。
5. **无写盘需求**：本项目无存档；不得引入 `user://` 写盘；测试删档类操作不适用。

## 工程配置对照（两版 project.godot）

| 配置 | 工作树最小模板 | 源库原件 | 说明 |
|---|---|---|---|
| `config/name` | `Control Gallery` | 同左 | 一致 |
| `run/main_scene` | 无（未声明） | `res://control_gallery.tscn` | 验证入口差异：工作树需手动打开场景 |
| `run/low_processor_mode` | 无 | `true` | 静态展示适用；工作树行为未核实 |
| `gdscript/warnings/untyped_declaration` | 无 | `1` | `tree.gd` 内 `image` 等用 `:=` 推断，`root/child` 显式类型；以原件警告为准 |
| `window/stretch` | 无 | `canvas_items` / `expand` | 拉伸表现以实测截图为准 |
| `window/vsync/vsync_mode` | 无 | `0`（关闭） | 同上 |
| `rendering_method` | 无 | `gl_compatibility`（含 mobile） | README 亦标注 Compatibility |

源库原件路径：以任务指定的源库 `gui/control_gallery/project.godot` 为准（只读参照，不复制）。

## 实现自由度

- 文档只约束**行为与表现**，不规定节点重命名以外的实现手法；但本项目现状即完成态，原则上不重构，只修障。
- 允许为验证可行性微调展示属性（如分隔偏移、初始值），必须在交付说明中记录"改了什么、为什么、验证证据"，并同步 [ui-hud.md](../gdd/ui-hud.md)。
- 窗口尺寸、 themes、字体等未声明项由验证者按实际记录，不得反写为"设计值"。

## 技术验收

- [ ] 编辑器打开主场景无错误/警告（除已知未类型化警告配置外）。
- [ ] 运行主场景无报错退出；关闭干净。
- [ ] UID 引用完整，无缺失外部资源。
