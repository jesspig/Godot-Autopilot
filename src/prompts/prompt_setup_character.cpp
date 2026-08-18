#include "prompts/prompt_setup_character.hpp"

namespace godot_autopilot {

std::string prompt_setup_character() {
  return R"gda(# 3D 角色控制器设置指南

## 背景
本流程引导你在 Godot 中创建一个 3D 角色控制器，包含 CharacterBody3D、碰撞形状和移动脚本。

## 前置条件
- 已有一个 3D 场景（如果是空白场景，先参照 create-3d-scene 创建基本场景）
- EditorInterface 可用

## 分步引导

### 步骤 1：创建 CharacterBody3D
```json
{
  "name": "create_scene_node",
  "arguments": {
    "parent_path": "",
    "name": "Player",
    "type": "CharacterBody3D"
  }
}
```

### 步骤 2：添加碰撞形状
```json
{
  "name": "create_scene_node",
  "arguments": {
    "parent_path": "Player",
    "name": "CollisionShape3D",
    "type": "CollisionShape3D"
  }
}
```

### 步骤 3：设置胶囊形碰撞体
```json
{
  "name": "property_set",
  "arguments": {
    "path": "Player/CollisionShape3D",
    "property": "shape",
    "value": {
      "type": "CapsuleShape3D",
      "radius": 0.5,
      "height": 2.0
    }
  }
}
```

### 步骤 4：创建 GDScript 文件
```json
{
  "name": "create_script",
  "arguments": {
    "path": "res://player.gd",
    "source_code": "extends CharacterBody3D\n\n@export var speed: float = 5.0\n@export var jump_velocity: float = 4.5\n\nfunc _physics_process(delta: float) -> void:\n    # 获取输入方向\n    var input_dir: Vector2 = Input.get_vector(\"move_left\", \"move_right\", \"move_forward\", \"move_back\")\n    var direction: Vector3 = (transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()\n    \n    # 水平移动\n    if direction:\n        velocity.x = direction.x * speed\n        velocity.z = direction.z * speed\n    else:\n        velocity.x = move_toward(velocity.x, 0, speed)\n        velocity.z = move_toward(velocity.z, 0, speed)\n    \n    # 重力\n    if not is_on_floor():\n        velocity.y += get_gravity().y * delta\n    \n    # 跳跃\n    if Input.is_action_just_pressed(\"ui_accept\") and is_on_floor():\n        velocity.y = jump_velocity\n    \n    move_and_slide()"
  }
}
```

### 步骤 5：附加脚本到角色
```json
{
  "name": "attach_script_to_node",
  "arguments": {
    "path": "Player",
    "script_path": "res://player.gd"
  }
}
```

## 注意事项
- 脚本中的输入动作（move_left/move_right/move_forward/move_back）需要先在 Input Map 中配置（参考 setup-input-map）
- 碰撞形状的尺寸需要与视觉模型匹配
- 使用 `property_set` 可以调整角色的其他物理属性（重力倍率、最大滑动角度等）
- CharacterBody3D 默认没有视觉表现——可以添加 MeshInstance3D 作为子节点)gda";
}

} // namespace godot_autopilot
