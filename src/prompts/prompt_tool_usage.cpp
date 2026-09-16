#include "prompts/prompt_tool_usage.hpp"

namespace godot_autopilot {

std::string prompt_tool_usage() {
  return R"gda(# 常用工具使用示例

本文档提供了 19 个最常用 MCP 工具的详细使用示例，包含输入输出格式和注意事项。

---

## 1. create_scene_node — 创建子节点

**功能：** 在场景中创建新节点。可指定父节点路径、节点名称和类型。

**输入：**
```json
{
  "name": "create_scene_node",
  "arguments": {
    "parent_path": "",
    "name": "Player",
    "type": "CharacterBody2D"
  }
}
```
- `parent_path`：父节点路径。空字符串表示作为根节点（场景必须无根节点）。
- `name`：节点名称。
- `type`：Godot 节点类名（如 Node2D、Sprite2D、CharacterBody2D）。

**输出：**
```json
{
  "result": {
    "path": "Player",
    "undo": "use editor_undo_redo tools: ..."
  }
}
```

**注意：**
- 如果场景已有根节点且 `parent_path` 为空会报错
- 节点类型必须是 Node 子类，否则报错
- 创建后自动切换 2D/3D 工作区
- 子节点会自动设置 owner 为场景根节点

---

## 2. property_set — 设置属性

**功能：** 设置节点的任意属性值。支持 type_hint 指定 Godot Variant 类型。

**输入（不含 type_hint）：**
```json
{
  "name": "property_set",
  "arguments": {
    "path": "Player",
    "property": "position",
    "value": {"x": 100, "y": 200}
  }
}
```

**输入（含 type_hint）：**
```json
{
  "name": "property_set",
  "arguments": {
    "path": "Player",
    "property": "modulate",
    "value": {"r": 1.0, "g": 0.5, "b": 0.5, "a": 1.0},
    "type_hint": "Color"
  }
}
```

**输出：**
```json
{
  "result": "ok",
  "undo": {
    "path": "Player",
    "property": "position",
    "old_value": {"x": 0, "y": 0}
  }
}
```

**注意：**
- `type_hint` 帮助正确反序列化 Godot Variant（Vector2、Color、Vector3、int、float、Rect2 等）
- 返回值包含 `undo` 信息，可用于撤销操作
- Camera2D 的 `enabled`/`current` 属性是运行时属性，不会保存在 .tscn 文件中

---

## 3. property_get — 获取属性

**功能：** 获取节点的任意属性值。

**输入：**
```json
{
  "name": "property_get",
  "arguments": {
    "path": "Player",
    "property": "position"
  }
}
```

**输出：**
```json
{
  "result": {"x": 100, "y": 200}
}
```

**注意：**
- 返回的 JSON 格式根据 Godot Variant 类型自动映射（Vector2→{x,y}，Color→{r,g,b,a} 等）
- 不存在的属性会返回 nil

---

## 4. signal_connect — 连接信号

**功能：** 连接两个节点的信号到方法。使用 CONNECT_PERSIST 标志确保连接在编辑器中持久化。

**输入：**
```json
{
  "name": "signal_connect",
  "arguments": {
    "source_path": "UI/MainContainer/StartButton",
    "signal": "pressed",
    "target_path": "Player",
    "method": "_on_start_pressed"
  }
}
```

**输出：**
```json
{
  "result": "connected",
  "info": "signal connection created with CONNECT_PERSIST flag"
}
```

**注意：**
- 参数名是 `source_path`、`signal`、`target_path`、`method`
- 信号连接默认使用 `CONNECT_PERSIST` 标志，可在编辑器保存
- 目标方法必须在目标节点的脚本中定义
- 信号不存在会报错

---

## 5. create_script — 创建 GDScript 文件

**功能：** 在指定路径创建 GDScript 文件并写入源码。

**输入：**
```json
{
  "name": "create_script",
  "arguments": {
    "path": "res://player.gd",
    "source_code": "extends CharacterBody2D\n\n@export var speed: float = 200.0\n\nfunc _physics_process(delta: float) -> void:\n    var input_dir := Input.get_vector(\"move_left\", \"move_right\", \"move_up\", \"move_down\")\n    velocity = input_dir * speed\n    move_and_slide()"
  }
}
```

**输出：**
```json
{
  "result": {
    "path": "res://player.gd",
    "class": "GDScript"
  }
}
```

**注意：**
- 自动创建中间目录
- `source_code` 为必填参数
- 路径必须以 `res://` 开头

---

## 6. attach_script_to_node — 附加脚本到节点

**功能：** 将已存在的 GDScript 文件附加到场景中的节点上。

**输入：**
```json
{
  "name": "attach_script_to_node",
  "arguments": {
    "node_path": "Player",
    "script_path": "res://player.gd"
  }
}
```

**输出：**
```json
{
  "result": "script attached to Player"
}
```

**注意：**
- 参数名是 `node_path` 和 `script_path`（不是 `path`）
- 脚本路径必须是已保存的 .gd 文件
- 操作会通过 UndoRedo 管理器执行，支持撤销
- 脚本的 `extends` 类型必须与节点类型兼容

---

## 7. create_resource — 创建资源

**功能：** 在内存中创建指定类型的资源实例。

**输入：**
```json
{
  "name": "create_resource",
  "arguments": {
    "type": "Environment",
    "name": "MyEnvironment"
  }
}
```

**输出：**
```json
{
  "result": {
    "class": "Environment",
    "path": "memory://MyEnvironment",
    "object_id": 1234567890,
    "name": "MyEnvironment"
  }
}
```

**注意：**
- 资源在内存中创建，需通过 `save_resource` 持久化到文件
- `name` 可选，用于设置内存路径标识
- 返回的 `object_id` 可用于 `save_resource` 的 `object_id` 参数
- 类型必须是 Resource 子类

---

## 8. save_resource — 保存资源

**功能：** 将资源保存到文件。支持保存内存中创建的资源（使用 object_id 或 class_type+name 变通方案）。

**输入（使用 class_type + name 创建新资源并保存）：**
```json
{
  "name": "save_resource",
  "arguments": {
    "path": "res://environment.tres",
    "class_type": "Environment",
    "name": "MyEnvironment"
  }
}
```

**输入（保存对象 ID 指定的资源）：**
```json
{
  "name": "save_resource",
  "arguments": {
    "path": "res://environment.tres",
    "object_id": 1234567890
  }
}
```

**输出：**
```json
{
  "result": 0
}
```
（0 = OK，非零值表示错误码）

**注意：**
- `path` 是源路径（用于加载已有资源）或目标路径
- `dest_path` 可选，用于保存到不同路径
- 当资源无法加载时，如果提供了 `class_type` + `name` 会自动创建新资源
- `object_id` 来自 `create_resource` 的返回结果
- 自动创建中间目录

---

## 9. create_editor_scene — 创建新场景

**功能：** 创建新场景并设置根节点。

**输入：**
```json
{
  "name": "create_editor_scene",
  "arguments": {
    "type": "Node2D",
    "name": "Game"
  }
}
```

**输出：**
```json
{
  "result": {
    "path": "Game",
    "type": "Node2D"
  }
}
```

**注意：**
- 当前场景不能有根节点（需要先保存并关闭当前场景）
- `type` 默认 `Node`，`name` 默认 `NewRoot`
- 创建后自动切换到对应的 2D/3D 工作区

---

## 10. open_editor_scene — 打开场景

**功能：** 从文件系统打开一个 .tscn 场景文件。

**输入：**
```json
{
  "name": "open_editor_scene",
  "arguments": {
    "path": "res://game.tscn"
  }
}
```

**输出：**
```json
{
  "result": "ok"
}
```

**注意：**
- 路径必须是以 `res://` 开头的 .tscn 文件
- 打开后自动根据根节点类型切换工作区

---

## 11. save_editor_scene_as — 保存场景

**功能：** 将当前场景保存到指定路径。

**输入：**
```json
{
  "name": "save_editor_scene_as",
  "arguments": {
    "path": "res://game.tscn"
  }
}
```

**输出：**
```json
{
  "result": "saved"
}
```

**注意：**
- 当前必须有已打开的场景
- 路径必须以 `res://` 开头，扩展名为 .tscn

---

## 12. add_input_map_action + add_input_map_action_event — 输入映射

**功能：** 创建输入动作并绑定按键事件。

**步骤 1：创建动作**
```json
{
  "name": "add_input_map_action",
  "arguments": {
    "action": "move_left",
    "deadzone": 0.5
  }
}
```

**步骤 2：绑定按键事件**
```json
{
  "name": "add_input_map_action_event",
  "arguments": {
    "action": "move_left",
    "event": {
      "class": "InputEventKey",
      "keycode": 65,
      "ctrl_pressed": false,
      "shift_pressed": false
    }
  }
}
```

**输出：**
```json
{
  "result": "ok"
}
```

**注意：**
- 参数名是 `action`（不是 `name`）
- `event` 参数必须包含 `"class"` 字段指定具体 InputEvent 子类
- 常用 event 类：`InputEventKey`、`InputEventMouseButton`、`InputEventJoypadButton`
- 必须先用 `add_input_map_action` 创建动作，再绑定事件

---

## 13. save_input_map — 持久化输入映射

**功能：** 将当前运行时 InputMap 中的所有自定义动作保存到 ProjectSettings 并写入 project.godot 文件。

**输入：**
```json
{
  "name": "save_input_map",
  "arguments": {}
}
```

**输出：**
```json
{
  "result": "persisted",
  "actions_persisted": 5,
  "save_error": 0
}
```

**注意：**
- 自动跳过 `ui_` 前缀的内置动作
- 保存后会写入 project.godot 文件
- 建议在完成所有输入映射配置后调用

---

## 14. code_execute — GDScript 代码执行

**功能：** 编译并执行任意 GDScript 代码，返回结果。

**输入：**
```json
{
  "name": "code_execute",
  "arguments": {
    "source_code": "var node = EditorInterface.get_edited_scene_root()\nif node:\n    return node.name\nelse:\n    return \"no scene\"",
    "function_name": "_run",
    "timeout_ms": 5000
  }
}
```

**输出：**
```json
{
  "result": "Game",
  "execution_time_ms": 12
}
```

**SceneRoot 便捷变量（访问编辑场景节点）：**
```json
{
  "name": "code_execute",
  "arguments": {
    "source_code": "return SceneRoot.get_node(\"Player\").name",
    "function_name": "_run",
    "timeout_ms": 5000
  }
}
```

**注意：**
- 代码自动包装在 `@tool extends Node` 脚本中
- 便捷变量 `SceneRoot` 即编辑场景根节点，等价于 `EditorInterface.get_edited_scene_root()`
- 访问编辑场景节点请用 `SceneRoot.get_node("Child")`，不要带根节点名前缀（如 `SceneRoot.get_node("Player")`、`SceneRoot.get_node("Player/CollisionShape2D")`）；执行节点挂在 `/root` 下，`/root` 路径下没有编辑场景
- 使用 `EditorInterface.get_edited_scene_root()` 获取当前场景根节点（不是 `get_tree()`）
- 代码中的 `extends` 行会被自动注释掉
- 默认超时 5000ms，最大 30000ms
- 可以定义多个函数，指定 `function_name` 调用指定函数
- 默认（单函数）模式下源码包装进 `_run()` 函数体内执行，不支持顶层 `func` 定义——代码需内联为表达式/语句；多函数模式（定义命名函数并传 `function_name`）才允许函数定义
- 临时节点会挂载到场景树上，可以访问 `get_tree()`

---

## 15. batch_execute — 批量操作

**功能：** 按顺序执行多个工具操作，可选择遇错即停。

**输入：**
```json
{
  "name": "batch_execute",
  "arguments": {
    "stop_on_error": true,
    "operations": [
      {
        "tool": "create_scene_node",
        "args": {
          "parent_path": "",
          "name": "Game",
          "type": "Node2D"
        }
      },
      {
        "tool": "create_scene_node",
        "args": {
          "parent_path": "Game",
          "name": "Player",
          "type": "CharacterBody2D"
        }
      }
    ]
  }
}
```

**输出：**
```json
{
  "results": [
    {"index": 0, "tool": "create_scene_node", "status": "ok", "data": {"result": {"path": "Game", ...}}},
    {"index": 1, "tool": "create_scene_node", "status": "ok", "data": {"result": {"path": "Game/Player", ...}}}
  ],
  "total": 2,
  "succeeded": 2,
  "failed": 0
}
```

**注意：**
- 每个操作需要 `tool`（必填）和 `args`（可选）字段
- `stop_on_error` 默认为 true
- 操作顺序执行，不是并行
- 失败的操作用 `args` 字段提供错误信息
- 异步 game 工具（`execute_game_script`、`queue_game_input` 等）在 batch 中默认被明确报错拒绝（该操作 `status` 为 `"error"`，pending 记录被释放，不会悬挂等待），`total` = succeeded + failed；传 `await_async: true` 后 batch 经传输线程等待异步 op 的真实结果（受该 op 的 `timeout_ms` 约束）。注意：经 `call_tool` 间接调用 batch 且 `await_async: true` 会命中主线程兜底错误，需直接调用 `batch_execute`；长耗时游戏脚本也可用 `start_game_job` 异步提交（立即返回 `job_id`），再以 `get_game_job` 轮询结果

---

## 16. add_group_node — 组管理

**功能：** 将节点添加到指定组，便于批量操作和查找。

**输入：**
```json
{
  "name": "add_group_node",
  "arguments": {
    "node_path": "Player",
    "group_name": "players"
  }
}
```

**输出：**
```json
{
  "result": "added",
  "node_path": "Player",
  "group": "players"
}
```

**注意：**
- 组名是字符串，不需要预先创建
- 使用 `get_tree().call_group("players", "method_name")` 可调用组内所有节点的方法
- 组信息保存在 .tscn 文件中

---

## 17. capture_editor_viewport — 视口截图

**功能：** 捕获编辑器当前视口的 PNG 截图。

**输入：**
```json
{
  "name": "capture_editor_viewport",
  "arguments": {}
}
```

**输出：**
```json
{
  "result": {
    "data": "iVBORw0KGgo...",
    "format": "png",
    "width": 1920,
    "height": 1080
  }
}
```

**注意：**
- `data` 字段是 Base64 编码的 PNG 图片数据
- 截取的是编辑器基类控件的视口（包含工具栏、视口等）
- 截图是实时编辑器的当前状态

---

## 18. set_tilemap_cells — 批量铺 TileMap（大批量推荐用 code_execute）

**功能：** 一次设置多个 TileMap cell。

**输入（小型批次，建议 ≤64 项）：**
```json
{
  "name": "set_tilemap_cells",
  "arguments": {
    "node_path": "TileMap",
    "cells": [
      {"x": 0, "y": 5, "source_id": 0, "atlas_coords": {"x": 0, "y": 0}},
      {"x": 1, "y": 5, "source_id": 0, "atlas_coords": {"x": 0, "y": 0}}
    ],
    "layer": 0
  }
}
```

**矩形区域 — fill_tilemap_rect 一次铺满：**
```json
{
  "name": "fill_tilemap_rect",
  "arguments": {
    "node_path": "TileMap",
    "from": {"x": 0, "y": 5},
    "to": {"x": 31, "y": 5},
    "source_id": 0,
    "atlas_coords": {"x": 0, "y": 0}
  }
}
```

**替代路径（大批量推荐）— code_execute 循环铺 TileMap：**
```json
{
  "name": "code_execute",
  "arguments": {
    "source_code": "var tilemap = EditorInterface.get_edited_scene_root().get_node(\"TileMap\")\nvar source_id = 0\nvar atlas_coords = Vector2i(0, 0)\nfor x in range(32):\n    tilemap.set_cell(0, Vector2i(x, 5), source_id, atlas_coords)\nreturn \"placed 32 cells\""
  }
}
```

**注意：**
- 矩形区域优先用 `fill_tilemap_rect`：`from`/`to` 为两个对角格（含端点，先后顺序不限），整块记为一次 undo；单次上限 100000 格，传 `erase: true` 则清除区域内所有格子
- `cells` 数组每请求建议保持 ~64 项以下——客户端侧参数构造可能截断更大载荷，导致 JSON 解析失败（报 parse error）
- 大批量铺 TileMap（如整行、整面）改用 `code_execute`（或 `execute_game_script`）程序化循环生成，避免手工构造大型 JSON 出错
- `code_execute` 单函数模式下代码内联在 `_run()` 中执行，循环结束后用 `return` 返回结果

---

## 19. get_scene_tree — 获取场景树

**功能：** 获取当前打开场景的完整场景树结构。

**输入：**
```json
{
  "name": "get_scene_tree",
  "arguments": {}
}
```

**注意：**
- 没有打开的场景时返回错误
- After create_editor_scene / open_editor_scene, the scene tree listing may reflect the previous scene; call again or wait briefly for the editor to refresh.

---

## 通用注意事项

1. **所有节点路径**相对于当前场景根节点（如 `Player`、`UI/MainContainer/StartButton`）
2. **属性值类型**：Vector2→`{x,y}`、Vector3→`{x,y,z}`、Color→`{r,g,b,a}`、Rect2→`{position:{x,y},size:{w,h}}`（size 也可用 `{x,y}` 别名，position 可省略；同一轴两种拼写并存且值不同会报错）
3. **错误处理**：所有工具返回 `{"error": "消息"}` 表示失败
4. **线程安全**：所有 Godot API 调用通过 CommandQueue 在主线程执行)gda";
}

} // namespace godot_autopilot
