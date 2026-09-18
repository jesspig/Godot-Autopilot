#ifndef GODOT_AUTOPILOT_SCENE_SPEC_OPS_HPP
#define GODOT_AUTOPILOT_SCENE_SPEC_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace scene_spec_ops {

// 声明式建树的安全上限：递归深度与单次节点总数，超限显式失败。
inline constexpr int SPEC_MAX_DEPTH = 32;
inline constexpr int SPEC_MAX_NODES = 500;

// 用声明式 spec 一次建好整棵子树：
// spec{type, name, props, children} 递归，dry_run/preview 只校验并返回节点清单。
// 与 create_scene_node / code_execute 并存：定型结构用 spec，异形逻辑用代码。
mcp::JsonValue handle_build_from_spec(const mcp::JsonValue &args);

} // namespace scene_spec_ops
} // namespace godot_autopilot

#endif
