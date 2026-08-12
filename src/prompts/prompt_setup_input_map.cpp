#include "prompts/prompt_setup_input_map.hpp"

namespace godot_autopilot {

std::string prompt_setup_input_map() {
  return R"gda(# 输入动作（Input Map）配置指南

## 背景
本流程引导你在 Godot 项目设置中创建和配置输入动作，并测试输入响应。

## 前置条件
- 项目已打开
- 编辑器处于打开状态

## 分步引导

### 步骤 1：创建输入动作
通过 set_project_settings 创建新的输入动作映射：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "set_project_settings",
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
使用 `set_project_settings` 添加按键事件到动作。每个事件使用 Dictionary 格式：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "set_project_settings",
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
    "name": "save_project_settings",
    "arguments": {}
  }
}
```

### 步骤 4：测试输入动作
在场景中运行后，可以通过 `is_input_action_pressed` 检查动作状态：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "is_input_action_pressed",
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
    "name": "press_input_action",
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
    "name": "release_input_action",
    "arguments": {
      "action": "jump"
    }
  }
}
```
**注意：** `input_*` 工具仅作用于编辑器进程（MCP 服务器只在编辑器进程加载），运行中的游戏是独立进程，不会收到这些事件。要注入运行中的游戏进程，请使用 `queue_game_input` 工具（Game 类别）：
```json
{
  "name": "call_tool",
  "arguments": {
    "name": "queue_game_input",
    "arguments": {
      "type": "action",
      "action": "move_right",
      "mode": "api",
      "duration_ms": 1000
    }
  }
}
```

## 注意事项
- 键位 Keycode 对应 Godot 的 `Key` 枚举值
- 创建动作后不会自动绑定按键——需要手动添加 events 数组
- `save_project_settings` 将设置持久化到 project.godot 文件
- 为同一个动作绑定多个按键（如键盘 A 键和手柄左摇杆）可以提高可用性
- 输入动作名称应与脚本中使用的名称一致)gda";
}

} // namespace godot_autopilot
