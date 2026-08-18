#include "prompts/prompt_keycode_reference.hpp"

namespace godot_autopilot {

std::string prompt_keycode_reference() {
  return R"gda(# Godot 按键码与 Variant 类型参考

本文档提供 `add_input_map_action_event` 所需的按键码（keycode）和 Godot Variant 类型在 JSON 中的表示方式。

---

## 一、常用 ASCII 按键码

| 按键 | Keycode | 说明 |
|------|---------|------|
| Space | 32 | 空格键 |
| 0-9 | 48-57 | 数字键 |
| A | 65 | 字母 A |
| B | 66 | 字母 B |
| C | 67 | 字母 C |
| D | 68 | 字母 D |
| E | 69 | 字母 E |
| F | 70 | 字母 F |
| G | 71 | 字母 G |
| H | 72 | 字母 H |
| I | 73 | 字母 I |
| J | 74 | 字母 J |
| K | 75 | 字母 K |
| L | 76 | 字母 L |
| M | 77 | 字母 M |
| N | 78 | 字母 N |
| O | 79 | 字母 O |
| P | 80 | 字母 P |
| Q | 81 | 字母 Q |
| R | 82 | 字母 R |
| S | 83 | 字母 S |
| T | 84 | 字母 T |
| U | 85 | 字母 U |
| V | 86 | 字母 V |
| W | 87 | 字母 W |
| X | 88 | 字母 X |
| Y | 89 | 字母 Y |
| Z | 90 | 字母 Z |

---

## 二、Godot 特殊按键码

Godot 使用 `Key` 枚举，大部分特殊键的码值以 `4194304`（`KEY_SPECIAL`）为基值。

| Godot 常量 | 值 | 说明 |
|-----------|-----|------|
| KEY_NONE | 0 | 无按键 |
| KEY_SPACE | 32 | 空格 |
| KEY_ESCAPE | 4194305 | Esc |
| KEY_TAB | 4194306 | Tab |
| KEY_SHIFT | 4194307 | Shift |
| KEY_ALT | 4194308 | Alt |
| KEY_CONTROL | 4194309 | Ctrl |
| KEY_BACKSPACE | 4194311 | Backspace |
| KEY_ENTER | 4194312 | Enter/Return |
| KEY_KP_ENTER | 4194313 | 小键盘 Enter |
| KEY_DELETE | 4194315 | Delete |
| KEY_HOME | 4194318 | Home |
| KEY_END | 4194321 | End |
| KEY_PAGEUP | 4194323 | Page Up |
| KEY_PAGEDOWN | 4194325 | Page Down |
| KEY_UP | 4194326 | ↑ 方向键 |
| KEY_DOWN | 4194327 | ↓ 方向键 |
| KEY_LEFT | 4194328 | ← 方向键 |
| KEY_RIGHT | 4194329 | → 方向键 |
| KEY_CAPSLOCK | 4194334 | Caps Lock |
| KEY_F1 | 4194385 | F1 |
| KEY_F2 | 4194386 | F2 |
| KEY_F3 | 4194387 | F3 |
| KEY_F4 | 4194388 | F4 |
| KEY_F5 | 4194389 | F5 |
| KEY_F6 | 4194390 | F6 |
| KEY_F7 | 4194391 | F7 |
| KEY_F8 | 4194392 | F8 |
| KEY_F9 | 4194393 | F9 |
| KEY_F10 | 4194394 | F10 |
| KEY_F11 | 4194395 | F11 |
| KEY_F12 | 4194396 | F12 |
| KEY_SHIFT | 4194307 | 左/右 Shift |
| KEY_CTRL | 4194309 | 左/右 Ctrl |
| KEY_META | 4194310 | Windows/Cmd 键 |
| KEY_QUOTELEFT | 4194347 | 反引号 ` |

---

## 三、`event` 参数结构

`add_input_map_action_event` 的 `event` 参数必须是包含 `"class"` 字段的 JSON 对象。

### InputEventKey（键盘事件）

```json
{
  "class": "InputEventKey",
  "keycode": 65,
  "ctrl_pressed": false,
  "shift_pressed": false,
  "alt_pressed": false,
  "meta_pressed": false,
  "pressed": true
}
```

### InputEventMouseButton（鼠标事件）

```json
{
  "class": "InputEventMouseButton",
  "button_index": 1,
  "button_mask": 1,
  "pressed": true
}
```
- `button_index`：1=左键，2=右键，3=中键，4=滚轮上，5=滚轮下

### InputEventJoypadButton（手柄事件）

```json
{
  "class": "InputEventJoypadButton",
  "button_index": 0,
  "device": 0,
  "pressed": true
}
```

---

## 四、Godot Variant → JSON 映射

`property_set`、`property_get`、`code_execute` 等工具使用以下映射。

| Godot Variant 类型 | JSON 表示 | 示例 |
|-------------------|-----------|------|
| int | number | `42` |
| float | number | `3.14` |
| bool | boolean | `true` |
| String | string | `"hello"` |
| Vector2 | `{x, y}` | `{"x":100,"y":200}` |
| Vector2i | `{x, y}` | `{"x":1,"y":2}` |
| Vector3 | `{x, y, z}` | `{"x":0,"y":2,"z":5}` |
| Vector3i | `{x, y, z}` | `{"x":1,"y":2,"z":3}` |
| Vector4 | `{x, y, z, w}` | `{"x":0,"y":0,"z":0,"w":1}` |
| Color | `{r, g, b, a}` | `{"r":1.0,"g":0.5,"b":0.5,"a":1.0}` |
| Rect2 | `{position, size}` | `{"position":{"x":0,"y":0},"size":{"w":100,"h":200}}` |
| Rect2i | `{position, size}` | `{"position":{"x":0,"y":0},"size":{"w":100,"h":200}}` |
| Plane | `{normal, d}` | `{"normal":{"x":0,"y":1,"z":0},"d":0}` |
| Quaternion | `{x, y, z, w}` | `{"x":0,"y":0,"z":0,"w":1}` |
| AABB | `{position, size}` | `{"position":{"x":0,"y":0,"z":0},"size":{"w":1,"h":2,"d":3}}` |
| Transform2D | `{origin, x, y}` | `{"origin":{"x":0,"y":0},"x":{"x":1,"y":0},"y":{"x":0,"y":1}}` |
| Transform3D | `{origin, basis}` | `{"origin":{"x":0,"y":0,"z":0},"basis":{"x":{"x":1,"y":0,"z":0},"y":...}}` |
| NodePath | string | `"Player/Camera"` |
| Rid | number | `0` |
| Dictionary | object | `{"key":"value"}` |
| Array | array | `[1, 2, 3]` |
| PackedByteArray | string (base64) | `"SGVsbG8="` |
| Color（无 alpha） | `{r, g, b}` | `{"r":1.0,"g":1.0,"b":1.0}` |

### 使用 type_hint

当为复合类型属性赋值时，建议提供 `type_hint` 参数帮助正确反序列化：

```json
{
  "path": "TestMesh",
  "property": "mesh",
  "value": {
    "type": "BoxMesh",
    "size": {"x": 2, "y": 1, "z": 1}
  },
  "type_hint": "BoxMesh"
}
```

常用 type_hint 值：
- `Vector2`、`Vector3`、`Color`、`Rect2`、`Transform2D`、`Transform3D`
- `int`、`float`、`bool`、`String`
- `Dictionary`、`Array`、`PackedStringArray`
- `NodePath`、`RID`
- 具体资源类名：`BoxMesh`、`Environment`、`CapsuleShape3D`

---

## 五、特殊注意项

### Camera2D enabled 属性
Camera2D 的 `enabled` 和 `current` 属性是**运行时属性**，不会序列化到 .tscn 文件中。设置这些属性后返回值会包含以下提示：

```json
{
  "result": "ok",
  "serialization_note": "Camera2D enabled is runtime-only and won't appear in .tscn. Use code_execute to call set_enabled() after _ready() for initial state."
}
```

建议在 `_ready()` 函数中通过 `code_execute` 设置：
```json
{
  "name": "code_execute",
  "arguments": {
    "source_code": "var cam = EditorInterface.get_edited_scene_root().get_node(\"Camera2D\")\ncam.set_enabled(true)"
  }
}
```)gda";
}

} // namespace godot_autopilot
