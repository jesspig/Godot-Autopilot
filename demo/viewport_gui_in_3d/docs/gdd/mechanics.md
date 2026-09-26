# 机制规格：视口复合与输入转发

本文是视口成像链与输入转发规则的**唯一权威**。控件清单见 [ui-hud.md](./ui-hud.md)；3D 布置见 [level-design.md](./level-design.md)；配置见 [../tech/tdd.md](../tech/tdd.md)。**本演示无玩法**，以下只写机制实录，不虚构任何玩法规则。

## 不变式（任何时候都必须成立）

- 2D 界面只活在 `SubViewport` 内，主视口不直接承载任何演示控件。
- Quad 上的像素恒等于 `SubViewport` 的渲染输出（经 `ViewportTexture`，`viewport_path = SubViewport`）。
- 发往 2D 界面的每个输入事件都经过 `gui_3d.gd` 的坐标换算或分类直投，不存在绕过脚本的拾取路径。
- 鼠标进出状态（`is_mouse_inside`）与视口的 `NOTIFICATION_VP_MOUSE_ENTER / NOTIFICATION_VP_MOUSE_EXIT` 通知成对出现。
- 键盘类事件与鼠标/触屏类事件走互斥分支：前者经 `_unhandled_input` 直投，后者经物理拾取回调换算后投递。

## 成像链（SubViewport → Quad）

| 环节 | 磁盘事实 |
|---|---|
| 视口 | `GUIPanel3D/SubViewport`，`size = 560×360`，`gui_embed_subwindows = true`，`render_target_update_mode = 4`（原始取值；枚举语义未核实） |
| 纹理 | `SubResource ViewportTexture`，`viewport_path = NodePath("SubViewport")` |
| 材质 | `StandardMaterial3D`，`resource_local_to_scene = true`，`transparency = 1`、`shading_mode = 0`（原始取值；通常分别对应 Alpha 透明与无光照着色，语义未核实），`albedo_texture` 为上述视口纹理 |
| 网格 | `QuadMesh`，`size = (3, 2)`；对应碰撞体 `BoxShape3D`，`size = (3, 2, 0.1)` |
| 下拉弹窗 | `gui_embed_subwindows = true` 使 `OptionButton` 弹窗渲染在同一视口内，保证弹窗也能成像于 Quad |

## 拾取链（3D 物理 → 脚本）

- 拾取体为 `Quad/Area3D`：`collision_layer = 2`（源库原件中物理层 2 命名为 `Control`，见 [../tech/tdd.md](../tech/tdd.md)），`input_capture_on_drag = true`。
- `_ready` 中连接三个信号：`mouse_entered → _mouse_entered_area`、`mouse_exited → _mouse_exited_area`、`input_event → _mouse_input_event`。
- `_ready` 末尾检查 Quad 表面材质的 `billboard_mode`：若为 `BILLBOARD_DISABLED` 则 `set_process(false)`，即非 billboard 模式下逐帧旋转补偿不运行。

## 输入转发契约（`gui_3d.gd` 实录）

### 分支一：`_unhandled_input`（键盘类直投）

- 对 `InputEventMouseButton、InputEventMouseMotion、InputEventScreenDrag、InputEventScreenTouch` 四类直接 `return`，其余事件一律 `node_viewport.push_input(input_event)`。
- 含义：键盘等非定点事件不经过坐标换算，直接喂给子视口。

### 分支二：`_mouse_input_event`（定点事件换算）

1. 取 Quad 网格尺寸（仅支持 `PlaneMesh / QuadMesh`，脚本注释原话语义如此）。
2. 碰撞点 `event_position`（世界坐标）经 `node_quad.global_transform.affine_inverse()` 转为 Quad 局部坐标。
3. 若 `is_mouse_inside`：取 `(x, -y)`（Y 翻转），除以网格尺寸归一到 `-0.5~0.5`，加 `0.5` 平移到 `0~1`，再乘以 `node_viewport.size` 得到视口像素坐标；否则回退到上次已知位置 `last_event_pos2D`。
4. 回填 `input_event.position`；若为 `InputEventMouse` 同时回填 `global_position`。
5. 若为 `InputEventMouseMotion / InputEventScreenDrag`：以前后两次位置差计算 `relative`，再除以时间差计算 `velocity`（无历史位置时 `relative` 置零）。
6. 更新 `last_event_pos2D` 与 `last_event_time`（`Time.get_ticks_msec() / 1000.0`），最后 `node_viewport.push_input(input_event)`。

### 进出通知

- 进入：`is_mouse_inside = true`，`node_viewport.notification(NOTIFICATION_VP_MOUSE_ENTER)`。
- 离开：先发 `NOTIFICATION_VP_MOUSE_EXIT`，再 `is_mouse_inside = false`。

### Billboard 补偿（`rotate_area_to_billboard`）

- 仅当材质 `billboard_mode > 0` 时生效；取当前 3D 相机，使 `Area3D` 朝向相机方向；`billboard_mode == 2` 时锁定 Y 旋转（脚本原逻辑；枚举语义未核实）；最后按相机 `rotation.z` 补偿 Z 轴旋转。
- 当前材质是否为 billboard 模式：以磁盘 `.tscn` 中有无设置该属性为准（现状文件中未见设置，默认行为未核实）。

## 交互流程（用户视角）

```text
悬停（enter 通知 → 视口知道鼠标到了）→ 点击/拖动（input_event 换算 → 控件响应）
→ 键盘输入（_unhandled_input 直投 → LineEdit 得字）→ 离开（exit 通知 → 悬停态清除）
```

## 边界情况清单

- 拖拽出 Quad 范围：`is_mouse_inside` 为假时回退到上次位置；`input_capture_on_drag = true` 使拖拽序列不中断。
- 触屏事件（`InputEventScreenTouch/Drag`）走物理拾取分支，而非键盘直投分支。
- 相对位移首帧：无历史位置时 `relative` 为零，避免跳变。
- Billboard 注释中留有 `TODO`（适配 billboard 模式或完全避免），属源码原样，效果未核实。
- 多控件重叠（右侧 ColorRect/TextureRect/VSlider/OptionButton 均锚定右上区域）：命中归属由视口内 GUI 层级决定，3D 侧不裁决。

## 机制验收

- [ ] Quad 上能看到与 560×360 视口一致的完整 2D 界面，无拉伸错位（长宽比 3:2 与 560:360 一致）。
- [ ] 鼠标悬停 Quad 时控件出现悬停态，离开后清除。
- [ ] 点击按钮、输入框打字、拖动横/竖滑杆、展开下拉框并选中，均有可见反馈。
- [ ] 按住拖出 Quad 再松开，拖拽序列行为与直觉一致（不断链、不跳变）。
- [ ] 键盘输入在输入框聚焦时可达（直投分支生效）。
