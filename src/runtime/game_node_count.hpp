#ifndef GODOT_AUTOPILOT_RUNTIME_GAME_NODE_COUNT_HPP
#define GODOT_AUTOPILOT_RUNTIME_GAME_NODE_COUNT_HPP

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>

#include <cstdint>

namespace godot_autopilot {
namespace runtime {
namespace node_count {

inline constexpr const char *kReservedRuntimeNodeName = "__gda_runtime";

inline int64_t count_subtree(const godot::Node *node) {
  if (node == nullptr)
    return 0;
  int64_t total = 1;
  const int children = node->get_child_count(true);
  for (int i = 0; i < children; ++i)
    total += count_subtree(node->get_child(i, true));
  return total;
}

inline int64_t count_reserved_subtree(const godot::Node *node) {
  if (node == nullptr)
    return 0;
  int64_t total = 0;
  const int children = node->get_child_count(true);
  for (int i = 0; i < children; ++i) {
    godot::Node *child = node->get_child(i, true);
    if (child == nullptr)
      continue;
    if (child->get_name() == godot::StringName(kReservedRuntimeNodeName)) {
      total += count_subtree(child);
      continue;
    }
    total += count_reserved_subtree(child);
  }
  return total;
}

inline int64_t instrumentation_node_count(const godot::SceneTree *tree) {
  if (tree == nullptr)
    return 0;
  return count_reserved_subtree(tree->get_root());
}

inline int64_t user_node_count(const godot::SceneTree *tree) {
  if (tree == nullptr)
    return 0;
  return static_cast<int64_t>(tree->get_node_count()) -
         instrumentation_node_count(tree);
}

} // namespace node_count
} // namespace runtime
} // namespace godot_autopilot

#endif
