# 服务器 RID 工具

服务器层直接暴露：`RenderingServer`、`PhysicsServer2D/3D` 与 `NavigationServer2D/3D`。这些工具用 RID 句柄（整数 id）创建和驱动服务器对象，而不是场景节点。

## RID 心智模型

- 每个 `create_*` 返回 RID。物理工具返回 `{id, is_valid}`；导航返回 `rid`；渲染返回 `result.rid`。把数字作为 `rid`、`space_rid`、`map_rid` 或 `canvas_item_rid` 传给后续工具。
- 服务器对象与场景节点是两个世界。`create_physics_2d_body` 或 `create_physics_3d_body` 创建的 body 没有场景节点：树与场景文件里都看不到。RID 只活在编辑器进程。
- 桥接：`get_physics_node_rid` 取 `CollisionObject2D/3D` 节点；`get_render_canvas_item_rid` 取 `CanvasItem`；`find_render_node_from_rid` 做反向查找。
- RID 形状不是资源。形状 RID 不能赋给 `CollisionShape2D.shape` 或 `CollisionShape3D.shape`；它们要 `Shape2D`/`Shape3D`。用 `save_resource` 建 `.tres` 再赋值，或在 `code_execute` 里赋值。

## 物理查询

2D：先用 `get_physics_2d_space_direct_state` 确认空间。ray、shape、point 查询分别返回最近命中、重叠 collider 或接触某点的全部对象；`max_results` 默认 32。`space_rid` 可省略，自动使用被编辑场景的 2D world。

3D：同样三种查询，但 `space_rid` 必填。用 `code_execute` 取 world space，再用 `get_physics_3d_space_direct_state` 确认。

共享过滤：`collision_mask`、`exclude`、`collide_with_bodies`、`collide_with_areas`；ray 还接受 `hit_back_faces` 与 `hit_from_inside`。

## Body、关节与区域

- `create_physics_*_body` 返回 RID。模式：0 static、1 kinematic、2 rigid、3 rigid_linear；只有 rigid 响应力与冲量。
- `apply_physics_*_body_force` 是持续力；`apply_physics_*_body_impulse` 是瞬时冲量。可选 `position` 偏心施加并产生旋转。3D 还有 torque。
- `set_physics_*_body_state` / `get_physics_*_body_state` 按索引读写：0 transform、1 线速度、2 角速度、3 sleeping、4 can_sleep。
- 3D 额外：transform、body 参数（bounce、friction、mass、inertia、damp）、axis lock 与碰撞例外。
- 形状：circle/sphere RID 用 `set_physics_*_shape_data` 定尺寸；可喂给查询与 3D body shape。
- 关节：2D pin/groove/damped_spring；3D pin/hinge/slider/cone_twist/generic_6dof。`body_a_rid` 必填。
- 区域检测重叠并施加效果；3D 还有 area 参数、transform 与 space。

## 导航

Map -> region ->（3D navmesh）-> path 查询。导航 RID 不会持久化。

1. 用 `active: true` 创建 2D/3D map；map 默认 inactive。3D 还要 cell size、cell height 与 up，需与 navmesh 烘焙一致。
2. 在 map 上建 region。没有 region 的 map 返回空路径。3D 用 `set_nav_3d_region_navigation_mesh` 指定 `NavigationMesh`。
3. 用 `get_nav_2d_map_path` / `get_nav_3d_map_path` 查询；没有烘焙区域时为空。`get_nav_3d_map_closest_point_to_segment` 把位置吸附到 mesh。

Agent：在 map 上用 `map_rid` 与 `position` 创建；避障默认关闭。障碍只影响开启避障的 agent。

## 渲染

2D canvas：`create_render_canvas_item` 返回 item RID。用 `add_render_canvas_item_rect`、`_circle`、`_texture_rect`、`_line` 绘制。`set_render_canvas_item_transform` 设缩放与原点；`set_render_canvas_item_visible` 是硬开关。

3D：`create_render_scenario` 承载相机、灯光、mesh 与雾；`set_render_scenario_environment` 接受外部 environment RID。`create_render_camera` 只应用原点并朝 -Z；用透视或正交 setter。`create_render_light` 支持 directional/omni/spot 与 energy、range、size、color。`create_render_mesh` 持有几何；`add_render_mesh_surface` 从数组加 surface。材质可设命名 shader 参数，但没有工具把 shader 挂到材质上。

Viewport、粒子、环境、shader 与纹理使用同一 RID 模式；environment 与 instance RID 必须来自 `code_execute`。

## RID 生命周期与静默失败

- 无效 RID 静默失败。对过期或从未存在的 RID 调用任何工具（包括 `free`）都不崩溃也不报错；服务器解析不到就丢弃调用。自己记录哪个 create 产生了哪个 id。
- canvas 拆除不级联：释放 canvas RID 会让其 canvas item 变成孤儿，继续绘制直到逐个释放。
- 形状会自动脱离：释放形状会把它从所有引用 body 上移除，body 仍存活。
- 没有泄漏报告：泄漏的 RID 静默累积到编辑器退出。

## 时序与线程

- 编辑器强制单线程渲染，所以主线程上的 RenderingServer 调用同步执行；下一次绘制前已生效。
- 物理 tick 顺序：state sync、查询 flush、`_physics_process`、导航、消息 flush、积分、flush。节点回调看到上一步状态。
- 同步窗口外取 direct state 返回 null；space 锁定时相交查询返回空。要在 `_physics_process` 查询，不要在物理回调内查询。
- 导航 setter 会排队到下一次 sync；立即读回仍是旧值且不报错。
- map 重建使用双缓冲迭代槽；轮询 `map_get_iteration_id` 跟踪。首次 sync 前，release 构建的路径查询可能为空。
- 导航每帧 sync 两次：process（仅 sync）与 physics（sync 加避障）。避障速度只在物理 tick 更新。
- 每帧顺序：`_process`、消息 flush、导航 sync、渲染 sync、draw。连续超过 5 帧强制渲染 sync 会记录警告。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| 服务器调用成功但无变化 | RID 过期或从未创建 | 跟踪 id；检查 `is_valid` |
| 形状赋给节点失败 | RID 形状不是资源 | 保存 `.tres` 后赋资源 |
| 3D 物理查询失败 | 缺 `space_rid` | 先取 world space |
| 导航路径为空 | 无 region、无 navmesh 或未 sync | 加 region；等一次 sync |
