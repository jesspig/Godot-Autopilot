---
name: godot-autopilot-servers
description: "Godot 服务器层 RID 工具指南：RenderingServer、PhysicsServer 与 NavigationServer 语义——RID 生命周期与静默失败、物理 tick 顺序、导航同步槽与编辑器单线程假设。适用于服务器侧绘制、物理模拟或寻路。"
---

# Godot 服务器 RID 工具

调用方式：用 `gda search` 与 `gda describe` 发现，再用 `gda call`
调用；原生 MCP 工具（`search_tools`、`get_tool_detail`、`call_tool`）是兼容
路径。协议细节见 `godot-autopilot-tools`。

godot-autopilot 直接暴露引擎服务器层：`RenderingServer`、`TextServer`、
`PhysicsServer2D`、`PhysicsServer3D`、`NavigationServer2D` 与
`NavigationServer3D`。这些工具用 RID 句柄（普通整数 id）创建和驱动服务器侧
对象，而不是场景节点。适用于射线/形状/点查询、服务器侧模拟、寻路设置、自定义
2D 绘制、仅渲染的 3D 设置、环境后处理、着色器与字体塑形测量。

## RID 心智模型

- 每个 `create_*` 工具返回 RID 句柄。物理创建工具返回带数值 `id` 与
  `is_valid` 的对象；导航工具返回 `rid`；渲染工具返回 `result.rid`。保存
  数字，并作为后续工具的 `rid`/`space_rid`/`map_rid`/`canvas_item_rid`
  参数传入。
- 服务器对象与场景节点是两个世界。`create_physics_2d_body` 或
  `create_physics_3d_body` 创建的身体**没有场景节点**：编辑器场景树里看不到，
  也不会写进场景文件。RID 只存在于运行中的编辑器进程。
- 桥接工具把 RID 与编辑场景连起来：`get_physics_node_rid` 取
  `CollisionObject2D`/`CollisionObject3D` 的 RID；
  `get_navigation_map_rid` 取场景世界的导航地图 RID。方向反过来时，用
  `set_physics_body_bundle` 把形状与参数配置到场景碰撞对象对应的服务器身体。
- RID 失效后再次使用通常不会报错：服务器解析不到对象，直接丢弃调用。删除
  前确认 `is_valid`，并在同一会话内使用。

## 物理查询

查询前先确认空间有效并附着在场景上：

- `get_physics_2d_space_state` / `get_physics_3d_space_state` 返回空间 RID、
  valid 标志与可用查询列表。空间未激活或不在场景中时失败。
- `intersect_physics_2d_ray` / `intersect_physics_3d_ray` 做射线查询；
  `intersect_point`、`intersect_shape`、`cast_motion`、`collide_shape`、
  `get_rest_info` 分别做点、形状、运动投射、碰撞形状与静止信息查询。
- 3D 查询在未激活空间上失败；2D 同样要求空间附着场景。查询结果只反映当前
  物理 tick 的状态，修改后至少等一个物理帧再查询。

## 物理身体、关节与区域

- `create_physics_2d_body` / `create_physics_3d_body` 创建服务器身体，返回
  `id` 与 `is_valid`。用 `set_physics_body_bundle` 配置形状与参数，或配置
  场景碰撞对象对应的服务器身体。
- 形状创建工具返回形状 RID；它们**不能**直接赋给 `CollisionShape2D.shape`
  或 `CollisionShape3D.shape`——那些属性要 Resource。需要给场景节点形状时，
  用资源工具创建/保存 Shape 资源再赋值。
- 区域、关节与身体都用 RID 交互；删除用对应 `free_*` 工具。服务器不会自动
  回收 RID，长时间会话要显式释放。
- `set_physics_body_bundle` 是创建身体并配置形状/参数的组合入口，也可用于
  场景碰撞对象。

## 导航地图、区域与路径

导航服务器遵循同样的 RID 模式：map → region → 查询。

1. `create_navigation_map` 创建地图 RID；场景世界已有地图时用
   `get_navigation_map_rid`。
2. `create_navigation_region` 创建区域并挂到地图；`set_navigation_region_transform`
   设置变换。
3. `bake_navigation_region` 或 `set_navigation_region_navigation_mesh` 提供
   导航网格。
4. `get_navigation_path` 查询路径；`map_get_path` 类工具接受起点、终点与
   optimize 参数。
5. 区域变更后至少等一个同步帧再查询；立即查询可能得到空路径。

## 导航代理与障碍

`create_navigation_agent`、`set_navigation_agent_target`、
`get_navigation_agent_path` 驱动代理；`create_navigation_obstacle` 与
`set_navigation_obstacle_*` 配置动态障碍。代理与障碍都绑定地图 RID；地图
删除后它们失效。代理路径在服务器同步后更新，不要在同一帧内假设目标已生效。

## 渲染：2D 画布项

`create_render_canvas_item` 创建画布项 RID；后续 `canvas_item_add_*` 系列
添加矩形、线条、圆、多边形、文本与纹理。这些绘制只存在于服务器，不产生
场景节点，也不会写进场景文件。画布项需要有效 canvas 与父项；父项删除后子项
失效。自定义 2D 绘制优先用这些工具，而不是往场景里塞大量 Node2D。

## 渲染：场景、相机、灯光与网格

- `create_render_scenario` 创建场景；`create_render_camera` 创建相机；
  `set_render_camera_transform`、`set_render_camera_environment` 配置。
- `create_render_light`、`set_render_light_*` 配置方向光/点光/聚光。
- `create_render_mesh`、`set_render_mesh_surface_*` 创建并填充网格；
  `create_render_instance` 把网格实例挂到场景。
- 实例 RID 必须来自外部来源或上述创建工具；不能凭整数猜。删除场景前先删除
  实例，避免悬挂。

## 渲染：视口、粒子、环境、着色器与纹理

- `create_render_viewport`、`set_render_viewport_size`、`set_render_viewport_update_mode`
  配置视口。尺寸 ≤1px 的视口静默不渲染；`UPDATE_ONCE` 渲染一次后自动变为
  disabled。
- `create_render_particles`、`set_render_particles_*`、
  `restart_render_particles` 控制粒子；一次性爆发用 restart 开始新发射周期。
- `create_render_environment`、`set_render_environment_*` 配置环境与后处理。
- `create_render_shader`、`set_render_shader_code` 创建/替换着色器源码；
  编译在服务器侧发生，错误不会同步返回，读引擎日志确认。
- `create_render_texture`、`set_render_texture_*` 创建服务器纹理；纹理数据
  来自外部资源或显式像素数组。

## TextServer：字体与塑形文本

1. `create_text_server_font` 创建字体 RID；`set_text_server_font_data` 提供
   字体字节。
2. `create_text_server_shaped_text` 创建塑形文本；`set_text_server_shaped_text_*`
   设置文本、字体、大小与方向。
3. `has_text_feature` 在依赖某能力前检查 TextServer 支持。
4. 塑形结果用 `get_text_server_shaped_text_*` 读取；释放 RID 前不要复用。

## 最小工作流

物理查询：

```bash
gda search "physics 3d ray"
gda describe intersect_physics_3d_ray
gda call intersect_physics_3d_ray --args '{"space_rid": 1, "from": {"x": 0, "y": 1, "z": 0}, "to": {"x": 0, "y": -1, "z": 0}}'
```

导航路径：

1. 取地图 RID（场景世界或 `create_navigation_map`）。
2. 创建/配置区域并 bake。
3. 等一个同步帧。
4. 查询路径；空结果时检查区域是否启用、地图是否正确。

## RID 生命周期与静默失败

- 服务器对象不会随场景保存，也不会在场景切换时自动清理。会话结束或不再需要
  时显式释放。
- 失效 RID 的调用通常静默丢弃；`is_valid` 是唯一可靠检查。
- 删除父对象会让子对象失效；先删子再删父。
- 只有更底层的原生渲染设备 RID 会触发泄漏警告；工具层 RID 泄漏不会自动
  报警。
- 服务器侧创建的资源（形状、网格、纹理）需要显式释放；不要依赖 GC。

## 时序与线程

- 编辑器是单线程渲染（多线程渲染会在启动时崩溃），RenderingServer 调用必须
  在编辑器主线程执行；GDA 已通过主线程队列保证这一点。
- 物理 tick 顺序：physics step → flush queries → 节点回调。节点物理回调看到的
  是上一步状态。
- 导航同步在物理 tick 之后、渲染同步之前；区域变更后至少等一个同步帧。
- 每帧强制 RenderingServer 同步超过连续 5 帧会明显拖慢编辑器；需要时合并
  变更，不要逐条强制同步。

## 参见

- `godot-autopilot-content` - TileMap、动画、音频与 UI 主题工作流。
- `godot-autopilot-scene-system` - 构建和编辑这些 RID 工具桥接的场景节点。
- `godot-autopilot-resources` - 把形状资源保存为 `.tres` 供
  `CollisionShape` 赋值。
- `godot-autopilot-scripting` - 外部 RID 场景的 `code_execute` 用法。
