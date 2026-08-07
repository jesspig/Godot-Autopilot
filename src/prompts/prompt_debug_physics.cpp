#include "prompts/prompt_debug_physics.hpp"

namespace godot_self_driving {

std::string prompt_debug_physics() {
  return R"gsd(# 物理调试指南

## 背景
本流程引导你使用 Godot 的物理系统和调试工具进行碰撞检测、射线投射和性能分析。

## 前置条件
- 场景中包含物理体（CharacterBody3D、RigidBody3D、StaticBody3D 等）
- 编辑器处于打开状态

## 分步引导

### 1. 物理射线投射（3D）
从指定位置向目标方向发射射线，检测第一个碰撞物体：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "physics_3d_ray_cast",
    "arguments": {
      "space_rid": 0,
      "from": {"x": 0, "y": 1, "z": 0},
      "to": {"x": 0, "y": 1, "z": -10},
      "collision_mask": 1,
      "hit_from_inside": false
    }
  }
}
```

### 2. 形状投射（3D）
沿方向投射一个形状，检测沿途碰撞：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "physics_3d_shape_cast",
    "arguments": {
      "space_rid": 0,
      "shape_type": "CapsuleShape3D",
      "shape_params": {"radius": 0.5, "height": 2.0},
      "from": {"x": 0, "y": 1, "z": 0},
      "to": {"x": 0, "y": 1, "z": -5},
      "collision_mask": 1
    }
  }
}
```

### 3. 点查询
查询某一点附近的所有碰撞体：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "physics_3d_point_query",
    "arguments": {
      "space_rid": 0,
      "position": {"x": 0, "y": 1, "z": 0},
      "collision_mask": 1
    }
  }
}
```

### 4. 物理 2D 调试
2D 版本的射线投射：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "physics_2d_ray_cast",
    "arguments": {
      "space_rid": 0,
      "from": {"x": 0, "y": 0},
      "to": {"x": 100, "y": 0},
      "collision_mask": 1
    }
  }
}
```

### 5. 调试可视化
开启碰撞形状和导航的可视化：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "debug_collision_debug",
    "arguments": {
      "enabled": true
    }
  }
}
```
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "debug_navigation_debug",
    "arguments": {
      "enabled": true
    }
  }
}
```

### 6. 性能监控
查看物理相关的性能指标：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "debug_get_performance_monitor",
    "arguments": {
      "monitor": "physics_3d_active_objects"
    }
  }
}
```

## 注意事项
- `space_rid` 需要先通过 `physics_3d_space_get_direct_state` 获取
- 射线投射的 `from` 和 `to` 是世界坐标
- 碰撞掩码（collision_mask）基于层的 2^N 位运算
- 调试可视化在编辑器场景运行时才生效
- 使用 `debug_list_performance_monitors` 查看所有可用的性能监控项)gsd";
}

} // namespace godot_self_driving
