#include "prompts/prompt_setup_gui.hpp"

namespace godot_autopilot {

std::string prompt_setup_gui() {
  return R"gda(# GUI 界面创建指南

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
      "parent_path": "",
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
      "parent_path": "UI",
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
      "parent_path": "UI/MainContainer",
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
      "parent_path": "UI/MainContainer",
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
      "parent_path": "UI/MainContainer",
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
- 使用 MarginContainer 或设置容器主题的 padding 属性可以添加边距)gda";
}

} // namespace godot_autopilot
