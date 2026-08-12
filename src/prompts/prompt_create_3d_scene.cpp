#include "prompts/prompt_create_3d_scene.hpp"

namespace godot_autopilot {

std::string prompt_create_3d_scene() {
  return R"gda(# 创建 3D 场景指南

## 背景
本流程引导你在 Godot 中创建一个完整的 3D 场景，包含摄像机、光照、环境和测试对象。

## 前置条件
- 编辑器处于打开状态
- 已有一个空白场景（可通过 `create_scene_node` 创建 Node3D 根节点）

## 分步引导

### 步骤 1：创建根节点
```json
{
  "name": "create_scene_node",
  "arguments": {
    "parent_path": "",
    "name": "MyScene",
    "type": "Node3D"
  }
}
```

### 步骤 2：添加摄像机
```json
{
  "name": "create_scene_node",
  "arguments": {
    "parent_path": "MyScene",
    "name": "Camera3D",
    "type": "Camera3D"
  }
}
```
可选——设置摄像机位置：
```json
{
  "name": "property_set",
  "arguments": {
    "path": "Camera3D",
    "property": "position",
    "value": {"x": 0, "y": 2, "z": 5}
  }
}
```

### 步骤 3：添加方向光
```json
{
  "name": "create_scene_node",
  "arguments": {
    "parent_path": "MyScene",
    "name": "DirectionalLight3D",
    "type": "DirectionalLight3D"
  }
}
```
可选——调整光照角度：
```json
{
  "name": "property_set",
  "arguments": {
    "path": "DirectionalLight3D",
    "property": "rotation_degrees",
    "value": {"x": -45, "y": 30, "z": 0}
  }
}
```

### 步骤 4：添加环境
```json
{
  "name": "create_scene_node",
  "arguments": {
    "parent_path": "MyScene",
    "name": "WorldEnvironment",
    "type": "WorldEnvironment"
  }
}
```
创建 Environment 资源并设置：
```json
{
  "name": "property_set",
  "arguments": {
    "path": "WorldEnvironment",
    "property": "environment",
    "value": {"type": "Environment"}
  }
}
```

### 步骤 5：添加测试对象
```json
{
  "name": "create_scene_node",
  "arguments": {
    "parent_path": "MyScene",
    "name": "TestMesh",
    "type": "MeshInstance3D"
  }
}
```
```json
{
  "name": "property_set",
  "arguments": {
    "path": "TestMesh",
    "property": "mesh",
    "value": {
      "type": "BoxMesh",
      "size": {"x": 1, "y": 1, "z": 1}
    }
  }
}
```

## 注意事项
- WorldEnvironment 的 environment 属性需要先创建 Environment 资源再赋值
- Camera3D 默认位置在原点，需要调整位置才能看到场景
- 使用 `property_get_list` 查看节点的所有可用属性
- 使用 `save_editor_scene` 保存当前场景)gda";
}

} // namespace godot_autopilot
