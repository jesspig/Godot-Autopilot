#include "prompts/prompt_handlers.hpp"
#include "core/log_system.hpp"
#include <mcp/Content.hpp>
#include <string>

namespace godot_self_driving {

namespace {

static mcp::GetPromptResult make_result(const std::string& content) {
    mcp::GetPromptResult r;
    mcp::PromptMessage pm;
    pm.role = "assistant";
    pm.content = mcp::TextContent{"text", content};
    r.messages = {std::move(pm)};
    return r;
}

static std::string prompt_create_3d_scene() {
    return R"gsd(# 创建 3D 场景指南

## 背景
本流程引导你在 Godot 中创建一个完整的 3D 场景，包含摄像机、光照、环境和测试对象。

## 前置条件
- 编辑器处于打开状态
- 已有一个空白场景（可通过 `scene_node_create` 创建 Node3D 根节点）

## 分步引导

### 步骤 1：创建根节点
```json
{
  "name": "scene_node_create",
  "arguments": {
    "parent": "",
    "name": "MyScene",
    "type": "Node3D"
  }
}
```

### 步骤 2：添加摄像机
```json
{
  "name": "scene_node_create",
  "arguments": {
    "parent": "MyScene",
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
  "name": "scene_node_create",
  "arguments": {
    "parent": "MyScene",
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
  "name": "scene_node_create",
  "arguments": {
    "parent": "MyScene",
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
  "name": "scene_node_create",
  "arguments": {
    "parent": "MyScene",
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
- 使用 `editor_save_scene` 保存当前场景)gsd";
}

static std::string prompt_setup_character() {
    return R"gsd(# 3D 角色控制器设置指南

## 背景
本流程引导你在 Godot 中创建一个 3D 角色控制器，包含 CharacterBody3D、碰撞形状和移动脚本。

## 前置条件
- 已有一个 3D 场景（如果是空白场景，先参照 create-3d-scene 创建基本场景）
- EditorInterface 可用

## 分步引导

### 步骤 1：创建 CharacterBody3D
```json
{
  "name": "scene_node_create",
  "arguments": {
    "parent": "",
    "name": "Player",
    "type": "CharacterBody3D"
  }
}
```

### 步骤 2：添加碰撞形状
```json
{
  "name": "scene_node_create",
  "arguments": {
    "parent": "Player",
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
  "name": "script_create",
  "arguments": {
    "path": "res://player.gd",
    "content": "extends CharacterBody3D\n\n@export var speed: float = 5.0\n@export var jump_velocity: float = 4.5\n\nfunc _physics_process(delta: float) -> void:\n    # 获取输入方向\n    var input_dir: Vector2 = Input.get_vector(\"move_left\", \"move_right\", \"move_forward\", \"move_back\")\n    var direction: Vector3 = (transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()\n    \n    # 水平移动\n    if direction:\n        velocity.x = direction.x * speed\n        velocity.z = direction.z * speed\n    else:\n        velocity.x = move_toward(velocity.x, 0, speed)\n        velocity.z = move_toward(velocity.z, 0, speed)\n    \n    # 重力\n    if not is_on_floor():\n        velocity.y += get_gravity().y * delta\n    \n    # 跳跃\n    if Input.is_action_just_pressed(\"ui_accept\") and is_on_floor():\n        velocity.y = jump_velocity\n    \n    move_and_slide()"
  }
}
```

### 步骤 5：附加脚本到角色
```json
{
  "name": "script_attach_to_node",
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
- CharacterBody3D 默认没有视觉表现——可以添加 MeshInstance3D 作为子节点)gsd";
}

static std::string prompt_debug_physics() {
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

static std::string prompt_setup_input_map() {
    return R"gsd(# 输入动作（Input Map）配置指南

## 背景
本流程引导你在 Godot 项目设置中创建和配置输入动作，并测试输入响应。

## 前置条件
- 项目已打开
- 编辑器处于打开状态

## 分步引导

### 步骤 1：创建输入动作
通过 project_settings_set 创建新的输入动作映射：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "project_settings_set",
    "arguments": {
      "key": "input/move_left",
      "value": {
        "deadzone": 0.5,
        "events": []
      }
    }
  }
}
```

重复操作为以下动作创建配置：
- `input/move_right`
- `input/move_forward`
- `input/move_back`
- `input/jump`
- `input/interact`

### 步骤 2：为动作绑定按键
使用 `project_settings_set` 添加按键事件到动作。每个事件使用 Dictionary 格式：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "project_settings_set",
    "arguments": {
      "key": "input/move_left",
      "value": {
        "deadzone": 0.5,
        "events": [
          {
            "type": 4,
            "keycode": 65,
            "device": -1
          }
        ]
      }
    }
  }
}
```
常用按键 Keycode：
- A=65, D=68, W=87, S=83
- Space=32, Shift=340
- Escape=4194305（KEY_ESCAPE）
- Enter=4194310（KEY_ENTER）

### 步骤 3：保存项目设置
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "project_settings_save",
    "arguments": {}
  }
}
```

### 步骤 4：测试输入动作
在场景中运行后，可以通过 `input_is_action_pressed` 检查动作状态：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "input_is_action_pressed",
    "arguments": {
      "action": "move_left"
    }
  }
}
```

### 步骤 5：模拟输入
在编辑器中模拟按键动作进行测试：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "input_action_press",
    "arguments": {
      "action": "jump"
    }
  }
}
```
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "input_action_release",
    "arguments": {
      "action": "jump"
    }
  }
}
```

## 注意事项
- 键位 Keycode 对应 Godot 的 `Key` 枚举值
- 创建动作后不会自动绑定按键——需要手动添加 events 数组
- `project_settings_save` 将设置持久化到 project.godot 文件
- 为同一个动作绑定多个按键（如键盘 A 键和手柄左摇杆）可以提高可用性
- 输入动作名称应与脚本中使用的名称一致)gsd";
}

static std::string prompt_setup_gui() {
    return R"gsd(# GUI 界面创建指南

## 背景
本流程引导你在 Godot 中创建一个简单的 GUI 界面，包含容器布局、按钮和标签，以及信号连接。

## 前置条件
- 已有一个场景（2D 或 3D 均可）
- 编辑器处于打开状态

## 分步引导

### 步骤 1：创建 CanvasLayer
CanvasLayer 确保 UI 不受游戏世界缩放影响：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "scene_node_create",
    "arguments": {
      "parent": "",
      "name": "UI",
      "type": "CanvasLayer"
    }
  }
}
```

### 步骤 2：创建根容器
使用 VBoxContainer 自动垂直排列子控件：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "scene_node_create",
    "arguments": {
      "parent": "UI",
      "name": "MainContainer",
      "type": "VBoxContainer"
    }
  }
}
```
设置容器锚点以填充全屏：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "property_set",
    "arguments": {
      "path": "UI/MainContainer",
      "property": "anchor_right",
      "value": 1.0
    }
  }
}
```
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "property_set",
    "arguments": {
      "path": "UI/MainContainer",
      "property": "anchor_bottom",
      "value": 1.0
    }
  }
}
```

### 步骤 3：添加标题标签
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "scene_node_create",
    "arguments": {
      "parent": "UI/MainContainer",
      "name": "Title",
      "type": "Label"
    }
  }
}
```
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "property_set",
    "arguments": {
      "path": "UI/MainContainer/Title",
      "property": "text",
      "value": "Hello, Godot!"
    }
  }
}
```
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "property_set",
    "arguments": {
      "path": "UI/MainContainer/Title",
      "property": "horizontal_alignment",
      "value": 1
    }
  }
}
```

### 步骤 4：添加按钮
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "scene_node_create",
    "arguments": {
      "parent": "UI/MainContainer",
      "name": "StartButton",
      "type": "Button"
    }
  }
}
```
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "property_set",
    "arguments": {
      "path": "UI/MainContainer/StartButton",
      "property": "text",
      "value": "开始游戏"
    }
  }
}
```

### 步骤 5：添加退出按钮
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "scene_node_create",
    "arguments": {
      "parent": "UI/MainContainer",
      "name": "QuitButton",
      "type": "Button"
    }
  }
}
```
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "property_set",
    "arguments": {
      "path": "UI/MainContainer/QuitButton",
      "property": "text",
      "value": "退出"
    }
  }
}
```

### 步骤 6：连接按钮信号
连接按钮的 pressed 信号到自定义方法：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "signal_connect",
    "arguments": {
      "path": "UI/MainContainer/StartButton",
      "signal": "pressed",
      "callable": {
        "type": "method",
        "target_path": "",
        "method": "_on_start_pressed"
      }
    }
  }
}
```

## 注意事项
- CanvasLayer 默认在最上层渲染，适合做 HUD 和菜单
- VBoxContainer 自动按添加顺序垂直排列子节点
- 如果按钮需要间距，可以在容器中添加控制节点（Control）作为间隔
- `horizontal_alignment` 值：0=左对齐，1=居中对齐，2=右对齐
- 信号连接需要对应的脚本中存在目标方法
- 使用 MarginContainer 或设置容器主题的 padding 属性可以添加边距)gsd";
}

} // anonymous namespace

void register_all_prompts(mcp::McpServer& server, CommandQueue& queue) {
    server.RegisterPrompt("create-3d-scene",
        mcp::PromptOptions{}.Description("Guide to create a basic 3D scene with camera, lighting, and a test object"),
        [&queue](const std::string&, const std::optional<mcp::JsonValue>&) -> mcp::GetPromptResult {
            std::string content;
            queue.submit([&content]() {
                content = prompt_create_3d_scene();
            }).get();
            return make_result(content);
        });

    server.RegisterPrompt("setup-character",
        mcp::PromptOptions{}.Description("Guide to set up a 3D character controller with CharacterBody3D, collision, and movement script"),
        [&queue](const std::string&, const std::optional<mcp::JsonValue>&) -> mcp::GetPromptResult {
            std::string content;
            queue.submit([&content]() {
                content = prompt_setup_character();
            }).get();
            return make_result(content);
        });

    server.RegisterPrompt("debug-physics",
        mcp::PromptOptions{}.Description("Guide to use physics debugging tools including ray casts, shape casts, and performance monitors"),
        [&queue](const std::string&, const std::optional<mcp::JsonValue>&) -> mcp::GetPromptResult {
            std::string content;
            queue.submit([&content]() {
                content = prompt_debug_physics();
            }).get();
            return make_result(content);
        });

    server.RegisterPrompt("setup-input-map",
        mcp::PromptOptions{}.Description("Guide to configure input actions in Project Settings and test them"),
        [&queue](const std::string&, const std::optional<mcp::JsonValue>&) -> mcp::GetPromptResult {
            std::string content;
            queue.submit([&content]() {
                content = prompt_setup_input_map();
            }).get();
            return make_result(content);
        });

    server.RegisterPrompt("setup-gui",
        mcp::PromptOptions{}.Description("Guide to create a simple GUI with CanvasLayer, containers, buttons, and labels"),
        [&queue](const std::string&, const std::optional<mcp::JsonValue>&) -> mcp::GetPromptResult {
            std::string content;
            queue.submit([&content]() {
                content = prompt_setup_gui();
            }).get();
            return make_result(content);
        });

    LogSystem::instance().log(LogLevel::Info, LogCategory::Prompts,
        "5 prompt templates registered");
}

} // namespace godot_self_driving
