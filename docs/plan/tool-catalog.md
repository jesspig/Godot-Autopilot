# 工具清单

> **版本**: 0.1.0 | **更新**: 2026-07-29
>
> **摘要**: 完整的当前工具清单，按分类列出全部 ~205 个工具（5 元工具 + ~200 领域工具）。每个工具标注对应引擎源文件路径和行号。
>
> **关联文档**:
> - [tool-discovery.md](tool-discovery.md) — 工具的 3 层发现机制
> - [tool-expansion.md](tool-expansion.md) — 计划扩展的工具
> - [execution-engine.md](execution-engine.md) — 执行引擎元工具

---

## 1. 工具注册位置

所有工具的注册入口位于 `src/tools/register_all.cpp`：

| 组件 | 位置 | 说明 |
|------|------|------|
| 元工具注册 | `register_all.cpp:252-510` | `server.RegisterTool()` 直接注册 5 个元工具 |
| g_handlers 映射 | `register_all.cpp:37-246` | ~200 条 `g_handlers["name"] = handler;` |
| ToolCatalog 填充 | `register_all.cpp:514-847` | ~205 条 `catalog.add_tool(...)` |
| BM25 索引 | `register_all.cpp:850-852` | 遍历 catalog.get_all_tools() 建立索引 |

## 2. 元工具（5 个）

注册在 `register_all.cpp:252-510`，通过 `server.RegisterTool()` 直接注册到 MCP：

| 工具名 | 用途 | 分类 | 标签 |
|--------|------|------|------|
| `ping` | 健康检查 | Meta | ping, health |
| `search_tools` | BM25 搜索工具目录 | Meta | search, discover |
| `list_categories` | 列出所有分类 | Meta | list, categories |
| `get_tool_detail` | 获取单工具 Schema | Meta | detail, schema |
| `call_tool` | 代理执行领域工具 | Meta | proxy, execute |

## 3. 领域工具

### 3.1 Scene（3 个）— `scene_ops`

| 工具名 | 描述 | 引擎源 | 包 | 注册行 |
|--------|------|--------|-----|--------|
| `scene_node_create` | 创建场景节点 | `scene/main/node.h` | scene_ops | 49 |
| `scene_node_delete` | 删除场景节点 | `scene/main/node.h` | scene_ops | 50 |
| `scene_tree_get` | 获取场景树 | `scene/main/scene_tree.h` | scene_ops | 51 |

### 3.2 Property（4 个）— `property_ops`

| 工具名 | 描述 | 引擎源 | 包 | 注册行 |
|--------|------|--------|-----|--------|
| `property_get` | 获取属性值 | `core/object/object.h` | property_ops | 52 |
| `property_set` | 设置属性值 | `core/object/object.h` | property_ops | 53 |
| `property_get_list` | 列出所有属性 | `core/object/object.h` | property_ops | 54 |
| `signal_connect` | 连接信号 | `core/object/object.h` | property_ops | 55 |

### 3.3 Resources（20 个）— `resource_ops`

| 工具名 | 描述 | 引擎源 | 注册行 |
|--------|------|--------|--------|
| `resource_load` | 加载资源 | `core/io/resource_loader.h` | 217 |
| `resource_load_threaded` | 线程加载资源 | `core/io/resource_loader.h` | 218 |
| `resource_load_threaded_get_status` | 线程加载状态 | `core/io/resource_loader.h` | 219 |
| `resource_load_threaded_wait` | 等待线程加载 | `core/io/resource_loader.h` | 220 |
| `resource_save` | 保存资源 | `core/io/resource_saver.h` | 221 |
| `resource_create` | 创建资源 | `core/io/resource.h` | 222 |
| `resource_duplicate` | 复制资源 | `core/io/resource.h` | 223 |
| `resource_get_type` | 获取资源类型 | `core/io/resource.h` | 224 |
| `resource_exists` | 检查资源是否存在 | `core/io/resource_loader.h` | 225 |
| `resource_list_types` | 列出所有资源类型 | `core/io/resource.h` | 226 |
| `resource_get_extensions` | 获取文件扩展名 | `core/io/resource_loader.h` | 227 |
| `resource_list_dir` | 列出目录资源 | `core/io/dir_access.h` | 228 |
| `resource_get_uid` | 获取 UID | `core/io/resource_uid.h` | 229 |
| `resource_set_uid` | 设置 UID | `core/io/resource_uid.h` | 230 |
| `resource_remove` | 删除资源文件 | `core/io/dir_access.h` | 231 |
| `resource_rename` | 重命名资源 | `core/io/dir_access.h` | 232 |
| `resource_get_dependencies` | 获取依赖 | `core/io/resource_loader.h` | 233 |
| `resource_has_dependency` | 检查依赖 | `core/io/resource_loader.h` | 234 |
| `resource_import` | 导入资源 | `core/io/resource_importer.h` | 235 |
| `resource_reimport` | 重新导入 | `core/io/resource_importer.h` | 236 |

### 3.4 Scripts（10 个）— `script_ops`

| 工具名 | 描述 | 引擎源 | 注册行 |
|--------|------|--------|--------|
| `script_execute_gdscript` | 执行 GDScript 表达式 | `modules/gdscript/gd_script.h` | 237 |
| `script_load` | 加载脚本文件 | `core/io/resource_loader.h` | 238 |
| `script_create` | 创建 GDScript | `modules/gdscript/gd_script.h` | 239 |
| `script_attach_to_node` | 附加脚本到节点 | `scene/main/node.h` | 240 |
| `script_detach_from_node` | 从节点分离脚本 | `scene/main/node.h` | 241 |
| `script_get_property` | 获取脚本属性 | `core/object/script.h` | 242 |
| `script_set_property` | 设置脚本属性 | `core/object/script.h` | 243 |
| `script_call_function` | 调用脚本函数 | `core/object/object.h` | 244 |
| `script_reload` | 重新加载脚本 | `modules/gdscript/gd_script.h` | 245 |
| `script_get_variable_list` | 获取变量列表 | `core/object/script.h` | 246 |

### 3.5 Physics（36 个）— `physics_ops`

2D（14 个）：

| 工具名 | 描述 | 注册行 |
|--------|------|--------|
| `physics_2d_space_get_direct_state` | 获取 2D 空间直接状态 | 56 |
| `physics_2d_ray_cast` | 2D 射线投射 | 57 |
| `physics_2d_shape_cast` | 2D 形状投射 | 58 |
| `physics_2d_point_query` | 2D 点查询 | 59 |
| `physics_2d_intersect_shape` | 2D 形状相交检测 | 60 |
| `physics_2d_intersect_point` | 2D 点相交检测 | 61 |
| `physics_2d_body_create` | 创建 2D 物理体 | 62 |
| `physics_2d_body_set_mode` | 设置 2D 物理体模式 | 63 |
| `physics_2d_body_apply_force` | 对 2D 物理体施力 | 64 |
| `physics_2d_body_apply_impulse` | 对 2D 物理体施加冲量 | 65 |
| `physics_2d_body_set_state` | 设置 2D 物理体状态 | 66 |
| `physics_2d_body_get_state` | 获取 2D 物理体状态 | 67 |
| `physics_2d_joint_create` | 创建 2D 关节 | 68 |
| `physics_2d_area_create` | 创建 2D 区域 | 69 |

3D（22 个）：

| 工具名 | 描述 | 注册行 |
|--------|------|--------|
| `physics_3d_space_get_direct_state` | 获取 3D 空间直接状态 | 71 |
| `physics_3d_ray_cast` | 3D 射线投射 | 72 |
| `physics_3d_shape_cast` | 3D 形状投射 | 73 |
| `physics_3d_point_query` | 3D 点查询 | 74 |
| `physics_3d_intersect_shape` | 3D 形状相交 | 75 |
| `physics_3d_intersect_point` | 3D 点相交 | 76 |
| `physics_3d_body_create` | 创建 3D 物理体 | 77 |
| `physics_3d_body_set_mode` | 设置 3D 物理体模式 | 78 |
| `physics_3d_body_apply_force` | 施力 | 79 |
| `physics_3d_body_apply_impulse` | 施加冲量 | 80 |
| `physics_3d_body_set_state` | 设置状态 | 81 |
| `physics_3d_body_get_state` | 获取状态 | 82 |
| `physics_3d_joint_create` | 创建关节 | 83 |
| `physics_3d_area_create` | 创建区域 | 84 |
| `physics_3d_area_set_monitorable` | 设置区域可监控 | 85 |
| `physics_3d_body_apply_torque` | 施加扭矩 | 86 |
| `physics_3d_body_set_axis_lock` | 设置轴锁定 | 87 |
| `physics_3d_body_add_collision_exception` | 添加碰撞例外 | 88 |
| `physics_3d_body_remove_collision_exception` | 移除碰撞例外 | 89 |
| `physics_3d_joint_set_param` | 设置关节参数 | 90 |
| `physics_3d_area_set_space_override` | 设置空间覆盖 | 91 |
| `physics_3d_soft_body_create` | 创建软体 | 94 |

### 3.6 Render（29 个）— `render_ops`

| 工具名 | 描述 | 引擎源 | 注册行 |
|--------|------|--------|--------|
| `canvas_item_create` | 创建 Canvas 项 | `servers/rendering_server.h` | 96 |
| `canvas_item_draw_rect` | 绘制矩形 | ... | 97 |
| `canvas_item_draw_circle` | 绘制圆形 | ... | 98 |
| `canvas_item_draw_texture` | 绘制纹理 | ... | 99 |
| `canvas_item_draw_line` | 绘制线条 | ... | 100 |
| `canvas_item_set_transform` | 设置变换 | ... | 101 |
| `canvas_item_set_visible` | 设置可见性 | ... | 102 |
| `scenario_create` | 创建场景 | ... | 103 |
| `scenario_set_environment` | 设置环境 | ... | 104 |
| `camera_create` | 创建摄像机 | ... | 105 |
| `camera_set_transform` | 设置摄像机变换 | ... | 106 |
| `camera_set_perspective` | 设置透视投影 | ... | 107 |
| `camera_set_orthogonal` | 设置正交投影 | ... | 108 |
| `light_create` | 创建光源 | ... | 109 |
| `light_set_param` | 设置光源参数 | ... | 110 |
| `light_set_color` | 设置光源颜色 | ... | 111 |
| `mesh_create` | 创建网格 | ... | 112 |
| `mesh_add_surface` | 添加网格表面 | ... | 113 |
| `mesh_set_material` | 设置网格材质 | ... | 114 |
| `material_create` | 创建材质 | ... | 115 |
| `material_set_param` | 设置材质参数 | ... | 116 |
| `viewport_create` | 创建视口 | ... | 117 |
| `viewport_set_size` | 设置视口尺寸 | ... | 118 |
| `viewport_set_clear_mode` | 设置清除模式 | ... | 119 |
| `particle_create` | 创建粒子系统 | ... | 120 |
| `environment_set_bg_color` | 设置环境背景色 | ... | 121 |
| `environment_set_ambient` | 设置环境光 | ... | 122 |
| `fog_create` | 创建雾 | ... | 123 |
| `shader_create` | 创建着色器 | ... | 124 |

### 3.7 Navigation（15 个）— `nav_ops`

| 工具名 | 注册行 |
|--------|--------|
| `nav_2d_map_create`, `nav_2d_region_create`, `nav_2d_path_query` | 125-127 |
| `nav_2d_agent_create`, `nav_2d_agent_set_target` | 128-129 |
| `nav_3d_map_create`, `nav_3d_region_create`, `nav_3d_path_query` | 130-132 |
| `nav_3d_path_query_segment`, `nav_3d_agent_create` | 133-134 |
| `nav_3d_agent_set_velocity`, `nav_3d_agent_get_next_path` | 135-136 |
| `nav_3d_map_set_cell_size`, `nav_3d_region_set_nav_mesh` | 137-138 |
| `nav_3d_obstacle_create` | 139 |

### 3.8 Audio（15 个）— `audio_ops`

| 工具名 | 注册行 |
|--------|--------|
| `audio_bus_get_layout`, `audio_bus_set_layout` | 140-141 |
| `audio_bus_get_count`, `audio_bus_get_name` | 142-143 |
| `audio_bus_set_volume`, `audio_bus_set_mute` | 144-145 |
| `audio_bus_set_bypass` | 146 |
| `audio_effect_add`, `audio_effect_remove` | 147-148 |
| `audio_stream_play`, `audio_stream_stop` | 149-150 |
| `audio_stream_set_volume`, `audio_stream_set_pitch` | 151-152 |
| `audio_stream_get_playback_position`, `audio_stream_seek` | 153-154 |

### 3.9 Input（10 个）— `input_ops`

| 工具名 | 注册行 |
|--------|--------|
| `input_action_press`, `input_action_release` | 155-156 |
| `input_is_action_pressed`, `input_is_action_just_pressed` | 157-158 |
| `input_key_press`, `input_key_release` | 159-160 |
| `input_mouse_move`, `input_mouse_button_press` | 161-162 |
| `input_mouse_button_release`, `input_gamepad_simulate` | 163-164 |

### 3.10 Editor（20 个）— `editor_ops`

| 工具名 | 注册行 |
|--------|--------|
| `editor_get_selection`, `editor_set_selection` | 165-166 |
| `editor_get_edited_scene_root`, `editor_save_scene` | 167-168 |
| `editor_save_all_scenes`, `editor_reload_scene` | 169-170 |
| `editor_inspect_object`, `editor_undo_redo_start` | 171-172 |
| `editor_undo_redo_commit`, `editor_undo_redo_add_do` | 173-174 |
| `editor_undo_redo_add_undo` | 175 |
| `editor_file_system_get_resources`, `editor_file_system_scan` | 176-177 |
| `editor_import_resource`, `editor_set_main_scene` | 178-179 |
| `editor_play_current_scene`, `editor_stop_playing` | 180-181 |
| `editor_get_resource_filesystem` | 182 |
| `editor_get_plugin_list`, `editor_set_plugin_enabled` | 183-184 |

### 3.11 Config（13 个）— `config_ops`

| 工具名 | 注册行 |
|--------|--------|
| `project_settings_get`, `project_settings_set` | 185-186 |
| `project_settings_has`, `project_settings_save` | 187-188 |
| `engine_get_version`, `engine_get_fps` | 189-190 |
| `engine_get_frames_drawn`, `engine_set_time_scale` | 191-192 |
| `engine_get_time_scale`, `engine_set_max_fps` | 193-194 |
| `editor_settings_get`, `editor_settings_set` | 195-196 |
| `editor_settings_has` | 197 |

### 3.12 Debug（15 个）— `debug_ops`

| 工具名 | 注册行 |
|--------|--------|
| `debug_print`, `debug_print_stack` | 198-199 |
| `debug_get_performance_monitor` | 200 |
| `debug_list_performance_monitors` | 201 |
| `debug_get_object_count`, `debug_get_object_count_by_class` | 202-203 |
| `debug_get_memory_usage` | 204 |
| `debug_profile_start`, `debug_profile_stop` | 205-206 |
| `debug_profile_get_data` | 207 |
| `debug_set_fps_limit`, `debug_set_physics_fps` | 208-209 |
| `debug_collision_debug`, `debug_navigation_debug` | 210-211 |
| `debug_performance_debug` | 212 |

### 3.13 Docs（4 个）— `doc_ops`

| 工具名 | 描述 | 注册行 |
|--------|------|--------|
| `doc_get_class` | 获取 Godot 类文档 | 213 |
| `doc_search` | 搜索 Godot 类 | 214 |
| `doc_get_method` | 获取方法签名 | 215 |
| `doc_get_property` | 获取属性信息 | 216 |

## 4. 添加新工具的流程

以在 Physics 类中添加一个工具为例（`register_all.cpp` + 处理函数文件）：

**步骤 1**: 在 `src/tools/physics_ops.hpp` 声明：
```cpp
mcp::JsonValue handle_3d_new_feature(const mcp::JsonValue& args);
```

**步骤 2**: 在 `src/tools/physics_ops.cpp` 实现：
```cpp
mcp::JsonValue handle_3d_new_feature(const mcp::JsonValue& args) {
    // 验证参数 → 调用 Godot API → 返回结果
}
```

**步骤 3**: 在 `src/tools/register_all.cpp` 注册：
- `g_handlers["physics_3d_new_feature"] = physics_ops::handle_3d_new_feature;`
- `catalog.add_tool({"physics_3d_new_feature", "Description", "Physics", {"tags..."}, schema});`

## 5. 工具 JSON Schema 标准

所有 ~205 个工具的 Schema 将遵循统一格式：

```json
{
  "type": "object",
  "properties": {
    "param_name": {
      "type": "string",
      "description": "参数用途的精确描述"
    }
  },
  "required": ["param_name"]
}
```

### 5.1 ToolOptions 标注

| 标注 | 含义 | 适用场景 |
|------|------|----------|
| `read_only_hint: true` | 只读操作，无副作用 | 查询类工具（get, search, list） |
| `destructive: true` | 可能导致数据丢失 | 删除类工具（scene_node_delete, resource_remove） |
| `idempotent: true` | 重复执行结果一致 | 设置类工具（set, create） |

### 5.2 当前 Schema 覆盖状态

- 完整 JSON Schema：仅 `ping`, `search_tools`, `list_categories`, `get_tool_detail`, `call_tool`, `scene_node_create`, `scene_node_delete`, `scene_tree_get`, `property_get`, `property_set`, `property_get_list`, `signal_connect` 共 12 个
- 空 Schema `{}`：其余 ~193 个领域工具（规划 0.4.0 版本补齐）

详细扩展计划见 [tool-expansion.md](tool-expansion.md)。
