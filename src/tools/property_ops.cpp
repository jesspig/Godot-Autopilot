#include "property_ops.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/readback_util.hpp"
#include "util/scene_path.hpp"
#include "util/variant_json.hpp"
#include <algorithm>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace godot_self_driving {
namespace property_ops {

namespace {

std::string to_std_string(const godot::String &s) {
  godot::CharString utf8 = s.utf8();
  return std::string(utf8.ptr());
}

godot::Dictionary find_property_info(godot::Node *node,
                                     const std::string &prop_name) {
  godot::TypedArray<godot::Dictionary> props = node->get_property_list();
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (dict.has("name") &&
        to_std_string(dict["name"].operator godot::String()) == prop_name) {
      return dict;
    }
  }
  return godot::Dictionary();
}

int levenshtein_distance(const std::string &a, const std::string &b) {
  std::vector<size_t> prev(b.size() + 1), curr(b.size() + 1);
  for (size_t j = 0; j <= b.size(); j++)
    prev[j] = j;
  for (size_t i = 1; i <= a.size(); i++) {
    curr[0] = i;
    for (size_t j = 1; j <= b.size(); j++) {
      size_t cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
      curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
    }
    prev.swap(curr);
  }
  return static_cast<int>(prev[b.size()]);
}

struct PropertyRenameHint {
  const char *old_name;
  const char *new_name;
};

const PropertyRenameHint PROPERTY_RENAME_HINTS[] = {
    {"frames", "sprite_frames"},
    {"cast_to", "target_position"},
    {"rect_position", "position"},
    {"rect_global_position", "global_position"},
    {"rect_size", "size"},
    {"rect_min_size", "custom_minimum_size"},
    {"rect_rotation", "rotation"},
    {"rect_scale", "scale"},
    {"rect_pivot_offset", "pivot_offset"},
    {"translation", "position"},
};

std::string find_property_candidates(godot::Node *node,
                                     const std::string &prop_name) {
  struct Candidate {
    int distance;
    std::string name;
  };
  godot::TypedArray<godot::Dictionary> props = node->get_property_list();
  std::vector<Candidate> candidates;
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (!dict.has("name"))
      continue;
    std::string name = to_std_string(dict["name"].operator godot::String());
    candidates.push_back({levenshtein_distance(prop_name, name), name});
  }

  for (const PropertyRenameHint &hint : PROPERTY_RENAME_HINTS) {
    if (prop_name == hint.old_name) {
      candidates.push_back({-1, hint.new_name});
      break;
    }
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate &x, const Candidate &y) {
              if (x.distance != y.distance)
                return x.distance < y.distance;
              return x.name < y.name;
            });
  size_t unique_count = 0;
  for (size_t k = 0; k < candidates.size(); k++) {
    if (unique_count > 0 &&
        candidates[unique_count - 1].name == candidates[k].name)
      continue;
    candidates[unique_count++] = candidates[k];
  }
  candidates.resize(unique_count);
  std::string result;
  for (size_t k = 0; k < candidates.size() && k < 4; k++) {
    if (!result.empty())
      result += ", ";
    result += candidates[k].name;
  }
  return result;
}

godot::Node *find_edited_scene_root() {
  auto editor = godot::EditorInterface::get_singleton();
  if (editor) {
    return editor->get_edited_scene_root();
  }
  return nullptr;
}

std::string hint_name(int hint) {
  static const std::unordered_map<int, std::string> names = {
      {0, "None"},
      {1, "Range"},
      {2, "Enum"},
      {3, "EnumSuggestion"},
      {4, "ExpEasing"},
      {5, "Link"},
      {6, "Flags"},
      {7, "Layers2DRender"},
      {8, "Layers2DPhysics"},
      {9, "Layers2DNavigation"},
      {10, "Layers3DRender"},
      {11, "Layers3DPhysics"},
      {12, "Layers3DNavigation"},
      {13, "File"},
      {14, "Dir"},
      {15, "GlobalFile"},
      {16, "GlobalDir"},
      {17, "ResourceType"},
      {18, "MultilineText"},
      {19, "Expression"},
      {20, "PlaceholderText"},
      {21, "ColorNoAlpha"},
      {22, "ObjectID"},
      {23, "TypeString"},
      {24, "NodePathToEditedNode"},
      {25, "ObjectTooBig"},
      {26, "NodePathValidTypes"},
      {27, "SaveFile"},
      {28, "GlobalSaveFile"},
      {29, "IntIsObjectID"},
      {30, "IntIsPointer"},
      {31, "ArrayType"},
      {32, "LocaleID"},
      {33, "LocalizableString"},
      {34, "NodeType"},
      {35, "HideQuaternionEdit"},
      {36, "Password"},
      {37, "LayersAvoidance"},
      {38, "DictionaryType"},
      {39, "ToolButton"},
      {40, "OneShot"},
      {42, "GroupEnable"},
      {43, "InputName"},
      {44, "FilePath"},
  };
  auto it = names.find(hint);
  return it != names.end() ? it->second : "Unknown";
}

std::string usage_name(int usage) {
  std::string result;
  if (usage == 0)
    return "None";
  if (usage & 2)
    result = "Storage";
  if (usage & 4)
    result = result.empty() ? "Editor" : result + ",Editor";
  if (usage & 8)
    result = result.empty() ? "Internal" : result + ",Internal";
  if (usage & 32)
    result = result.empty() ? "Checked" : result + ",Checked";
  if (usage & 64)
    result = result.empty() ? "Group" : result + ",Group";
  if (usage & 128)
    result = result.empty() ? "Category" : result + ",Category";
  if (usage & 4096)
    result = result.empty() ? "ScriptVariable" : result + ",ScriptVariable";
  if (usage & 268435456)
    result = result.empty() ? "ReadOnly" : result + ",ReadOnly";
  if (result.empty())
    result = "Default";
  return result;
}

} // namespace

mcp::JsonValue handle_get(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  auto *it_prop = args.Find("property");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  if (!it_prop || !it_prop->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: property");
    return e;
  }

  std::string path_str = it_path->GetString();
  std::string prop_str = it_prop->GetString();

  std::string hint;
  godot::Node *node =
      util::resolve_scene_node(path_str, find_edited_scene_root(), &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + path_str + " — " + hint);
    return e;
  }

  godot::StringName prop_name(prop_str.c_str());

  godot::Dictionary prop_info = find_property_info(node, prop_str);
  if (prop_info.is_empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "property not found: " + prop_str + " on node " + path_str +
        " — use property_get_list to see available properties");
    return e;
  }

  godot::Variant value = node->get(prop_name);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = VariantJson::serialize(value);
  return r;
}

mcp::JsonValue handle_set(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  auto *it_prop = args.Find("property");
  auto *it_val = args.Find("value");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  if (!it_prop || !it_prop->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: property");
    return e;
  }
  if (!it_val) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: value");
    return e;
  }

  std::string path_str = it_path->GetString();
  std::string prop_str = it_prop->GetString();

  std::string hint;
  godot::Node *node =
      util::resolve_scene_node(path_str, find_edited_scene_root(), &hint);
  if (!node) {
    std::string err = "node not found: " + path_str + " — " + hint;
    const std::string memory_prefix = "memory://";
    if (path_str.compare(0, memory_prefix.size(), memory_prefix) == 0) {
      err += " memory:// is a resource namespace, not a node path; to reference a memory resource pass it as the value (e.g. {\"resource\": \"memory://name\"}) or create the resource inline with code_execute";
    }
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(err);
    return e;
  }

  godot::Dictionary dict = find_property_info(node, prop_str);
  if (dict.is_empty()) {
    return util::error_detail(
        "property '" + prop_str + "' does not exist on " + path_str, path_str,
        "a valid property name from get_property_list",
        "use property_get_list to see available properties; candidate: " +
            find_property_candidates(node, prop_str));
  }

  std::string type_hint;
  auto *it_hint = args.Find("type_hint");
  if (it_hint && it_hint->IsString()) {
    type_hint = it_hint->GetString();
  }

  if (type_hint.empty()) {
    if (!dict.is_empty() && dict.has("type")) {
      int type_id = static_cast<int>(dict["type"]);
      int hint_val = 0;
      if (dict.has("hint")) {
        hint_val = static_cast<int>(dict["hint"]);
      }
      bool is_object_type =
          static_cast<godot::Variant::Type>(type_id) == godot::Variant::OBJECT;
      bool is_resource_hint = hint_val == godot::PROPERTY_HINT_RESOURCE_TYPE;
      if ((is_object_type || is_resource_hint) && dict.has("hint_string")) {
        std::string hint_str =
            to_std_string(dict["hint_string"].operator godot::String());
        if (!hint_str.empty()) {
          type_hint = hint_str;
        }
      }
      if (type_hint.empty()) {
        type_hint = to_std_string(godot::Variant::get_type_name(
            static_cast<godot::Variant::Type>(type_id)));
      }
    }
  }

  godot::StringName prop_name(prop_str.c_str());
  godot::Variant value;
  std::string resource_error;
  bool resource_attached = false;
  if (resource_ops::try_resolve_resource_value(*it_val, value,
                                               resource_error)) {
    if (!resource_error.empty()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(resource_error);
      return e;
    }
    resource_attached = true;
  } else {
    value = VariantJson::deserialize(*it_val, type_hint);
  }

  if (resource_attached && value.get_type() == godot::Variant::OBJECT) {
    godot::Ref<godot::Resource> res = value;
    if (res.is_valid()) {
      std::string res_path = to_std_string(res->get_path());
      const std::string memory_prefix = "memory://";
      if (res_path.compare(0, memory_prefix.size(), memory_prefix) == 0) {
        std::string res_name = res_path.substr(memory_prefix.size());
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(
            "cannot assign memory resource '" + res_name +
            "' to node property '" + prop_str + "' on " + path_str +
            " — memory:// resources are not persistent and will corrupt the scene file if saved. "
            "Use resource_save to save it to disk first, then pass {\"path\": "
            "\"res://...\"}"
            " Alternatively create the resource inline via code_execute (e.g. "
            "node.shape = RectangleShape2D.new()) or use resource_set_property "
            "to edit the memory resource itself.");
        return e;
      }
    }
  }

  godot::Variant old_val = node->get(prop_name);
  node->set(prop_name, value);

  godot::Variant new_val = node->get(prop_name);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  if (resource_attached) {
    r["resource_attached"] = mcp::JsonValue(true);
  }

  std::string readback_detail;
  util::ReadbackStatus readback =
      util::check_readback(value, old_val, new_val, readback_detail);
  if (readback == util::ReadbackStatus::REJECTED) {
    return util::error_detail("property rejected: '" + prop_str + "' on " +
                                  path_str,
                              path_str, "readback equals set value",
                              "property may not exist, be read-only, or "
                              "require a type hint; use property_get_list");
  }
  if (readback == util::ReadbackStatus::CONVERTED) {
    r["warning"] = mcp::JsonValue("set applied; " + readback_detail);
  }

  bool is_camera2d = false;
  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (cdbs) {
    is_camera2d =
        cdbs->is_parent_class(node->get_class(), godot::StringName("Camera2D"));
  }
  if (is_camera2d) {
    if (prop_str == "enabled") {
      r["serialization_note"] =
          mcp::JsonValue("Camera2D enabled defaults to true. If setting to "
                         "true (the default), it won't appear in .tscn. "
                         "If setting to false, it will serialize correctly. "
                         "To ensure initial enabled state, use code_execute: "
                         "get_node(\"Path/To/Camera2D\").set_enabled(true/"
                         "false) in _ready().");
    } else if (prop_str == "current") {
      r["serialization_note"] = mcp::JsonValue(
          "Camera2D current is not a registered property (no setter/getter). "
          "Cannot be set via property_set. "
          "Use code_execute: get_node(\"Path/To/Camera2D\").make_current()");
    }
  }

  mcp::JsonValue undo_info(mcp::JsonValue::object_tag);
  undo_info["path"] = mcp::JsonValue(path_str);
  undo_info["property"] = mcp::JsonValue(prop_str);
  undo_info["old_value"] = VariantJson::serialize(old_val);
  r["undo"] = std::move(undo_info);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_get_list(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }

  std::string path_str = it_path->GetString();

  std::string hint;
  godot::Node *node =
      util::resolve_scene_node(path_str, find_edited_scene_root(), &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + path_str + " — " + hint);
    return e;
  }

  godot::TypedArray<godot::Dictionary> props = node->get_property_list();
  mcp::JsonValue result(mcp::JsonValue::array_tag);

  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    mcp::JsonValue item(mcp::JsonValue::object_tag);

    if (dict.has("name")) {
      item["name"] =
          mcp::JsonValue(to_std_string(dict["name"].operator godot::String()));
    }
    if (dict.has("type")) {
      int type_id = static_cast<int>(dict["type"]);
      item["type_id"] = mcp::JsonValue(static_cast<int64_t>(type_id));
      item["type"] = mcp::JsonValue(to_std_string(godot::Variant::get_type_name(
          static_cast<godot::Variant::Type>(type_id))));
    }
    if (dict.has("hint")) {
      int hint_val = static_cast<int>(dict["hint"]);
      item["hint"] = mcp::JsonValue(hint_name(hint_val));
    }
    if (dict.has("hint_string")) {
      item["hint_string"] = mcp::JsonValue(
          to_std_string(dict["hint_string"].operator godot::String()));
    }
    if (dict.has("usage")) {
      int usage_val = static_cast<int>(dict["usage"]);
      item["usage"] = mcp::JsonValue(usage_name(usage_val));
    }
    if (dict.has("class_name")) {
      item["class_name"] = mcp::JsonValue(
          to_std_string(dict["class_name"].operator godot::String()));
    }

    result.PushBack(std::move(item));
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(result);
  return r;
}

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
    return r;
  }
  source->disconnect(sig_name, callable);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("disconnected");
  return r;
}

} // namespace property_ops
} // namespace godot_self_driving
