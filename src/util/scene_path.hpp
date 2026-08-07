#ifndef GODOT_SELF_DRIVING_SCENE_PATH_HPP
#define GODOT_SELF_DRIVING_SCENE_PATH_HPP

#include "error_util.hpp"

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <string>

namespace godot_self_driving {
namespace util {

inline std::string scene_path_hint(godot::Node *scene_root) {
  if (!scene_root) {
    return "当前没有可编辑的场景根节点";
  }
  std::string root_name = to_std(scene_root->get_name());
  return "当前场景根为 \"" + root_name + "\"；合法路径写法：" + root_name +
         "/子路径、/root/" + root_name + "/子路径、子路径";
}

inline void strip_leading_segment_if(std::string &path,
                                     const std::string &segment) {
  if (segment.empty())
    return;
  if (path == segment) {
    path.clear();
    return;
  }
  if (path.size() > segment.size() &&
      path.compare(0, segment.size(), segment) == 0 &&
      path[segment.size()] == '/') {
    path = path.substr(segment.size() + 1);
  }
}

inline godot::Node *resolve_scene_node(const std::string &path_str,
                                       godot::Node *scene_root,
                                       std::string *out_hint = nullptr) {
  if (!scene_root) {
    if (out_hint)
      *out_hint = scene_path_hint(nullptr);
    return nullptr;
  }
  std::string clean = path_str;
  if (!clean.empty() && clean[0] == '/') {
    clean = clean.substr(1);
  }
  strip_leading_segment_if(clean, "root");
  std::string root_name = to_std(scene_root->get_name());
  strip_leading_segment_if(clean, root_name);
  if (clean.empty() || clean == root_name) {
    return scene_root;
  }
  godot::Node *node = scene_root->get_node_or_null(
      godot::NodePath(godot::String(clean.c_str())));
  if (node) {
    if (node == scene_root) {
      return node;
    }
    godot::Node *parent = node->get_parent();
    while (parent) {
      if (parent == scene_root)
        return node;
      parent = parent->get_parent();
    }
  }
  if (out_hint)
    *out_hint = scene_path_hint(scene_root);
  return nullptr;
}

} // namespace util
} // namespace godot_self_driving

#endif
