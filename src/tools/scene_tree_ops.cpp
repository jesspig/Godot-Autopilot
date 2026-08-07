#include "scene_tree_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <string>

namespace godot_self_driving {
namespace scene_tree_ops {

using JV = mcp::JsonValue;

namespace {

std::string to_std(const godot::String &s) {
  godot::CharString utf8 = s.utf8();
  return std::string(utf8.ptr());
}

godot::SceneTree *get_tree() {
  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    auto *root = editor->get_edited_scene_root();
    if (root) {
      auto *tree = root->get_tree();
      if (tree)
        return tree;
    }
  }
  auto *engine = godot::Engine::get_singleton();
  if (engine) {
    auto *ml = engine->get_main_loop();
    if (ml) {
      return godot::Object::cast_to<godot::SceneTree>(ml);
    }
  }
  return nullptr;
}

} // namespace

JV handle_call_group(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_call_group called");
  auto *gn = args.Find("group_name");
  if (!gn || !gn->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: group_name");
    return e;
  }
  auto *mn = args.Find("method");
  if (!mn || !mn->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: method");
    return e;
  }
  auto *tree = get_tree();
  if (!tree) {
    JV e(JV::object_tag);
    e["error"] = JV("SceneTree not available");
    return e;
  }
  std::string group_name = gn->GetString();
  std::string method = mn->GetString();
  auto *ap = args.Find("arguments");
  if (ap && ap->IsArray()) {
    const auto &arr = ap->GetArray();
    godot::Array gd_args;
    for (const auto &item : arr) {
      gd_args.push_back(VariantJson::deserialize(item));
    }
    tree->call_group(godot::StringName(group_name.c_str()),
                     godot::StringName(method.c_str()), gd_args);
  } else {
    tree->call_group(godot::StringName(group_name.c_str()),
                     godot::StringName(method.c_str()));
  }
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_call_group completed");
  return r;
}

JV handle_create_timer(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_create_timer called");
  auto *dp = args.Find("delay_sec");
  if (!dp || !dp->IsNumber()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: delay_sec");
    return e;
  }
  auto *tree = get_tree();
  if (!tree) {
    JV e(JV::object_tag);
    e["error"] = JV("SceneTree not available");
    return e;
  }
  double delay =
      dp->IsDouble() ? dp->GetDouble() : static_cast<double>(dp->GetInt());
  bool process_always = true;
  auto *pa = args.Find("process_always");
  if (pa && pa->IsBool()) {
    process_always = pa->GetBool();
  }
  bool process_in_physics = false;
  auto *pp = args.Find("process_in_physics");
  if (pp && pp->IsBool()) {
    process_in_physics = pp->GetBool();
  }
  auto timer = tree->create_timer(delay, process_always, process_in_physics);
  if (timer.is_null()) {
    JV e(JV::object_tag);
    e["error"] = JV("failed to create timer");
    return e;
  }
  JV r(JV::object_tag);
  r["result"] = VariantJson::serialize(godot::Variant(timer));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_create_timer completed");
  return r;
}

JV handle_get_nodes_in_group(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_get_nodes_in_group called");
  auto *gn = args.Find("group_name");
  if (!gn || !gn->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: group_name");
    return e;
  }
  auto *tree = get_tree();
  if (!tree) {
    JV e(JV::object_tag);
    e["error"] = JV("SceneTree not available");
    return e;
  }
  std::string group_name = gn->GetString();
  auto nodes = tree->get_nodes_in_group(godot::StringName(group_name.c_str()));
  JV result_arr(JV::array_tag);
  for (int i = 0; i < nodes.size(); i++) {
    auto *node = godot::Object::cast_to<godot::Node>(nodes[i]);
    if (!node)
      continue;
    result_arr.PushBack(JV(to_std(node->get_path())));
  }
  JV r(JV::object_tag);
  r["result"] = std::move(result_arr);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_get_nodes_in_group completed");
  return r;
}

JV handle_is_paused(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_is_paused called");
  auto *tree = get_tree();
  if (!tree) {
    JV e(JV::object_tag);
    e["error"] = JV("SceneTree not available");
    return e;
  }
  bool paused = tree->is_paused();
  JV r(JV::object_tag);
  r["result"] = JV(paused);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_is_paused completed");
  return r;
}

JV handle_notify_group(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_notify_group called");
  auto *gn = args.Find("group_name");
  if (!gn || !gn->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: group_name");
    return e;
  }
  auto *ni = args.Find("notification");
  if (!ni || !ni->IsInt()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: notification");
    return e;
  }
  auto *tree = get_tree();
  if (!tree) {
    JV e(JV::object_tag);
    e["error"] = JV("SceneTree not available");
    return e;
  }
  std::string group_name = gn->GetString();
  int notification = ni->GetInt();
  tree->notify_group(godot::StringName(group_name.c_str()), notification);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_notify_group completed");
  return r;
}

JV handle_reload_current_scene(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_reload_current_scene called");
  auto *tree = get_tree();
  if (!tree) {
    JV e(JV::object_tag);
    e["error"] = JV("SceneTree not available");
    return e;
  }
  godot::Error err = tree->reload_current_scene();
  JV r(JV::object_tag);
  r["result"] = JV(static_cast<int64_t>(err));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_reload_current_scene completed");
  return r;
}

JV handle_set_debug_collisions(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_set_debug_collisions called");
  auto *ep = args.Find("enabled");
  if (!ep || !ep->IsBool()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: enabled");
    return e;
  }
  auto *tree = get_tree();
  if (!tree) {
    JV e(JV::object_tag);
    e["error"] = JV("SceneTree not available");
    return e;
  }
  tree->set_debug_collisions_hint(ep->GetBool());
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_set_debug_collisions completed");
  return r;
}

JV handle_set_pause(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_set_pause called");
  auto *pp = args.Find("paused");
  if (!pp || !pp->IsBool()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: paused");
    return e;
  }
  auto *tree = get_tree();
  if (!tree) {
    JV e(JV::object_tag);
    e["error"] = JV("SceneTree not available");
    return e;
  }
  tree->set_pause(pp->GetBool());
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scene_tree_set_pause completed");
  return r;
}

} // namespace scene_tree_ops
} // namespace godot_self_driving
