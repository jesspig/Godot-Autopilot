#include "tools/domains/property/property_ops.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "tools/domains/resource/resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/inline_resource_json.hpp"
#include "util/readback_util.hpp"
#include "util/scene_path.hpp"
#include "util/type_hint.hpp"
#include "util/variant_json.hpp"
#include <algorithm>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace godot_autopilot {
namespace property_ops {
mcp::JsonValue handle_signal_connect(const mcp::JsonValue &args) {
  auto *it_source = args.Find("source_path");
  auto *it_signal = args.Find("signal");
  auto *it_target = args.Find("target_path");
  auto *it_method = args.Find("method");
  auto *it_persist = args.Find("persist");
  if (!it_source || !it_source->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: source_path");
    return e;
  }
  if (!it_signal || !it_signal->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: signal");
    return e;
  }
  if (!it_target || !it_target->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: target_path");
    return e;
  }
  if (!it_method || !it_method->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: method");
    return e;
  }

  bool persist = true;
  if (it_persist && it_persist->IsBool()) {
    persist = it_persist->GetBool();
  }

  std::string source_path = it_source->GetString();
  std::string signal_name = it_signal->GetString();
  std::string target_path = it_target->GetString();
  std::string method_name = it_method->GetString();

  std::string hint;
  godot::Node *source =
      util::resolve_scene_node(source_path, find_edited_scene_root(), &hint);
  if (!source) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("source node not found: " + source_path + " — " + hint);
    return e;
  }

  godot::Node *target =
      util::resolve_scene_node(target_path, find_edited_scene_root(), &hint);
  if (!target) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("target node not found: " + target_path + " — " + hint);
    return e;
  }

  godot::StringName sig_name(signal_name.c_str());
  if (!source->has_signal(sig_name)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("signal not found: " + signal_name);
    return e;
  }

  godot::Callable callable(target, godot::StringName(method_name.c_str()));
  if (source->is_connected(sig_name, callable)) {
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("already_connected");
    r["info"] =
        mcp::JsonValue("signal is already connected to this callable on the "
                       "node; scene instances inherit persistent connections — "
                       "use signal_disconnect to remove it");
    util::add_scene_info_fields(r, util::edited_scene_info());
    return r;
  }
  godot::Error err = source->connect(
      sig_name, callable, persist ? godot::Object::CONNECT_PERSIST : 0);
  if (err != godot::OK) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("failed to connect signal: error code " +
                                std::to_string(static_cast<int>(err)));
    return e;
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("connected");
  r["persisted"] = mcp::JsonValue(persist);
  if (persist) {
    r["note"] =
        mcp::JsonValue("connection created with CONNECT_PERSIST — it is saved "
                       "with the scene and inherited by every instantiation; "
                       "avoid connecting the same signal+callable again");
  } else {
    r["note"] = mcp::JsonValue("connection created without CONNECT_PERSIST — "
                               "it is not saved with the scene");
  }
  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
  return r;
}

mcp::JsonValue handle_signal_disconnect(const mcp::JsonValue &args) {
  auto *it_source = args.Find("source_path");
  auto *it_signal = args.Find("signal");
  auto *it_target = args.Find("target_path");
  auto *it_method = args.Find("method");
  if (!it_source || !it_source->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: source_path");
    return e;
  }
  if (!it_signal || !it_signal->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: signal");
    return e;
  }
  if (!it_target || !it_target->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: target_path");
    return e;
  }
  if (!it_method || !it_method->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: method");
    return e;
  }

  std::string source_path = it_source->GetString();
  std::string signal_name = it_signal->GetString();
  std::string target_path = it_target->GetString();
  std::string method_name = it_method->GetString();

  std::string hint;
  godot::Node *source =
      util::resolve_scene_node(source_path, find_edited_scene_root(), &hint);
  if (!source) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("source node not found: " + source_path + " — " + hint);
    return e;
  }

  godot::Node *target =
      util::resolve_scene_node(target_path, find_edited_scene_root(), &hint);
  if (!target) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("target node not found: " + target_path + " — " + hint);
    return e;
  }

  godot::StringName sig_name(signal_name.c_str());
  godot::Callable callable(target, godot::StringName(method_name.c_str()));
  if (!source->is_connected(sig_name, callable)) {
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("not_connected");
    util::add_scene_info_fields(r, util::edited_scene_info());
    return r;
  }
  source->disconnect(sig_name, callable);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("disconnected");
  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
  return r;
}

} // namespace property_ops
} // namespace godot_autopilot
