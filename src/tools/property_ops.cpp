#include "property_ops.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/readback_util.hpp"
#include "util/scene_path.hpp"
#include "util/type_hint.hpp"
#include "util/variant_json.hpp"
#include <algorithm>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace godot_autopilot {
namespace property_ops {

namespace {

godot::Dictionary find_property_info(godot::Node *node,
                                     const std::string &prop_name) {
  godot::TypedArray<godot::Dictionary> props = node->get_property_list();
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (dict.has("name") &&
        util::to_std(dict["name"].operator godot::String()) == prop_name) {
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
    std::string name = util::to_std(dict["name"].operator godot::String());
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

std::string parse_hint_class(const std::string &hint_string) {
  const std::string prefix = "Type:";
  std::string s = hint_string;
  if (s.compare(0, prefix.size(), prefix) == 0) {
    s = s.substr(prefix.size());
  }
  size_t comma = s.find(',');
  if (comma != std::string::npos) {
    s = s.substr(0, comma);
  }
  size_t start = s.find_first_not_of(" \t");
  if (start == std::string::npos) {
    return "";
  }
  size_t end = s.find_last_not_of(" \t");
  return s.substr(start, end - start + 1);
}

bool is_node_class(const std::string &class_name) {
  if (class_name.empty()) {
    return false;
  }
  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs) {
    return false;
  }
  godot::StringName cls(class_name.c_str());
  if (cls == godot::StringName("Node")) {
    return true;
  }
  return cdbs->is_parent_class(cls, godot::StringName("Node"));
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

struct NodePathValueConversion {
  bool converted = false;
  bool has_error = false;
  mcp::JsonValue error;
  godot::Variant value;
  std::string node_path;
};

std::string node_reference_expected_text() {
  godot::Node *scene_root = find_edited_scene_root();
  return "a valid path to a node inside the currently edited scene" +
         (scene_root ? " (root \"" + util::to_std(scene_root->get_name()) +
                           "\")"
                     : " (no edited scene root)");
}

const char *const NODE_REFERENCE_ACTION_TEXT =
    "pass a scene-resolvable node path such as \"Player/Camera2D\" or "
    "\"..\", or use code_execute to assign a node reference directly "
    "(e.g. get_node(\"Path/To/Node\"))";

bool resolve_scene_node_reference(const std::string &node_path_str,
                                  godot::Variant &out_value) {
  godot::Node *scene_root = find_edited_scene_root();
  if (!scene_root) {
    return false;
  }
  godot::Node *target_node = scene_root->get_node_or_null(
      godot::NodePath(godot::String(node_path_str.c_str())));
  if (!target_node) {
    return false;
  }
  out_value = godot::Variant(static_cast<godot::Object *>(target_node));
  return true;
}

NodePathValueConversion
convert_node_path_value(const godot::Dictionary &dict,
                        const std::string &prop_str,
                        const std::string &path_str,
                        const mcp::JsonValue &raw_value) {
  NodePathValueConversion result;
  bool node_typed_prop = false;
  if (!dict.is_empty() && dict.has("type") && dict.has("hint")) {
    int type_id = static_cast<int>(dict["type"]);
    int hint_val = static_cast<int>(dict["hint"]);
    if (static_cast<godot::Variant::Type>(type_id) == godot::Variant::OBJECT) {
      if (hint_val == godot::PROPERTY_HINT_NODE_TYPE) {
        node_typed_prop = true;
      } else if (hint_val == godot::PROPERTY_HINT_RESOURCE_TYPE &&
                 dict.has("hint_string")) {
        std::string hint_str =
            util::to_std(dict["hint_string"].operator godot::String());
        node_typed_prop = is_node_class(parse_hint_class(hint_str));
      }
    }
  }
  if (!node_typed_prop || !raw_value.IsString()) {
    return result;
  }
  std::string np_str = raw_value.GetString();
  if (!resolve_scene_node_reference(np_str, result.value)) {
    result.has_error = true;
    result.error = util::error_detail(
        "cannot assign node path '" + np_str + "' to node-typed property '" +
            prop_str + "' on " + path_str,
        path_str, node_reference_expected_text(), NODE_REFERENCE_ACTION_TEXT);
    return result;
  }
  result.converted = true;
  result.node_path = np_str;
  return result;
}

struct ArrayValueConversion {
  bool handled = false;
  bool has_error = false;
  mcp::JsonValue error;
  godot::Variant value;
};

bool convert_array_element(const mcp::JsonValue &element,
                           const util::ArrayElementHint *element_hint,
                           std::size_t index,
                           const std::string &prop_str,
                           const std::string &path_str,
                           godot::Variant &out_value,
                           mcp::JsonValue &out_error) {
  const std::string element_label = "element " + std::to_string(index) +
                                    " of array property '" + prop_str +
                                    "' on " + path_str;
  if (element_hint == nullptr) {
    if (element.IsArray() || element.IsObject()) {
      out_error = util::error_detail(
          "array element type cannot be determined; refusing to write " +
              element_label,
          path_str,
          "an array whose element type is declared (Array[T] / [Export] T[])",
          "declare the element type in the script or use code_execute to "
          "build the array manually");
      return false;
    }
    out_value = VariantJson::deserialize(element);
    return true;
  }

  std::string element_class = parse_hint_class(element_hint->hint_string);
  bool node_typed = element_hint->hint == godot::PROPERTY_HINT_NODE_TYPE ||
                    is_node_class(element_class);
  bool resource_typed =
      !node_typed &&
      (element_hint->hint == godot::PROPERTY_HINT_RESOURCE_TYPE ||
       (element_hint->type == godot::Variant::OBJECT &&
        !element_class.empty()));

  if (element.IsNull()) {
    out_value = godot::Variant();
    return true;
  }

  if (node_typed) {
    std::string np_str;
    if (element.IsString()) {
      np_str = element.GetString();
    } else if (element.IsObject()) {
      auto *ref = element.Find("__node_ref__");
      if (ref && ref->IsString()) {
        np_str = ref->GetString();
      }
    }
    if (np_str.empty()) {
      out_error = util::error_detail(
          "cannot resolve node reference for " + element_label, path_str,
          "a node path string or {\"__node_ref__\": \"...\"}",
          NODE_REFERENCE_ACTION_TEXT);
      return false;
    }
    if (!resolve_scene_node_reference(np_str, out_value)) {
      out_error = util::error_detail(
          "cannot assign node path '" + np_str + "' to " + element_label,
          path_str, node_reference_expected_text(),
          NODE_REFERENCE_ACTION_TEXT);
      return false;
    }
    return true;
  }

  if (resource_typed) {
    std::string resource_label =
        element_class.empty() ? std::string("Resource") : element_class;
    std::string resolve_error;
    if (resource_ops::try_resolve_resource_value(element, out_value,
                                                 resolve_error)) {
      if (resolve_error.empty()) {
        return true;
      }
      out_error = util::error_detail(
          "cannot convert " + element_label + ": " + resolve_error, path_str,
          "a " + resource_label + " resource reference",
          "pass a res:// path string, {\"path\": \"res://...\"} or "
          "{\"resource\": \"memory://...\"} (or null to clear the slot)");
      return false;
    }
    out_error = util::error_detail(
        "cannot convert " + element_label + " to a " + resource_label +
            " resource reference",
        path_str, "a res:// path string or a resource object",
        "pass a res:// path string, {\"path\": \"res://...\"} or "
        "{\"resource\": \"memory://...\"} (or null to clear the slot)");
    return false;
  }

  out_value = VariantJson::deserialize(element);
  return true;
}

ArrayValueConversion
convert_array_value(const godot::Dictionary &dict, const std::string &prop_str,
                    const std::string &path_str,
                    const mcp::JsonValue &raw_value) {
  ArrayValueConversion result;
  if (dict.is_empty() || !dict.has("type")) {
    return result;
  }
  if (static_cast<godot::Variant::Type>(static_cast<int>(dict["type"])) !=
      godot::Variant::ARRAY) {
    return result;
  }
  result.handled = true;
  if (!raw_value.IsArray()) {
    result.has_error = true;
    result.error = util::error_detail(
        "value for array property '" + prop_str + "' on " + path_str +
            " is not a JSON array; refusing to write (a non-array value "
            "would silently clear the array)",
        path_str, "a JSON array",
        "pass an array, e.g. [] to clear it explicitly, or a list of "
        "convertible elements");
    return result;
  }

  std::optional<util::ArrayElementHint> parsed =
      util::parse_array_element_hint(dict);
  const util::ArrayElementHint *element_hint =
      (parsed.has_value() && parsed->known) ? &parsed.value() : nullptr;

  godot::Array out_array;
  const mcp::JsonValue::Array &elements = raw_value.GetArray();
  for (std::size_t i = 0; i < elements.size(); i++) {
    godot::Variant converted;
    mcp::JsonValue element_error;
    if (!convert_array_element(elements[i], element_hint, i, prop_str,
                               path_str, converted, element_error)) {
      result.has_error = true;
      result.error = std::move(element_error);
      return result;
    }
    out_array.append(converted);
  }

  if (element_hint != nullptr) {
    godot::Variant::Type element_type = element_hint->type;
    godot::StringName element_class_name;
    std::string element_class_name_str;
    if (element_hint->hint == godot::PROPERTY_HINT_RESOURCE_TYPE ||
        element_hint->hint == godot::PROPERTY_HINT_NODE_TYPE) {
      element_class_name_str = parse_hint_class(element_hint->hint_string);
      if (!element_class_name_str.empty()) {
        element_type = godot::Variant::OBJECT;
        element_class_name = godot::StringName(element_class_name_str.c_str());
      }
    }
    godot::Variant array_script;
    godot::Array typed_array(out_array, element_type, element_class_name,
                             array_script);
    if (typed_array.size() != out_array.size()) {
      result.has_error = true;
      result.error = util::error_detail(
          "cannot build typed array for property '" + prop_str + "' on " +
              path_str +
              ": one or more elements do not match the declared element type" +
              (element_class_name_str.empty()
                   ? ""
                   : " '" + element_class_name_str + "'"),
          path_str, "elements assignable to the declared array element type",
          "pass elements matching the declared array element type, or use "
          "code_execute to build the array manually");
      return result;
    }
    out_array = typed_array;
  }

  result.value = godot::Variant(out_array);
  return result;
}

bool memory_resource_assignment_blocked(const godot::Variant &value,
                                        const std::string &prop_str,
                                        const std::string &path_str,
                                        mcp::JsonValue &out_error) {
  if (value.get_type() != godot::Variant::OBJECT) {
    return false;
  }
  godot::Ref<godot::Resource> res = value;
  if (!res.is_valid()) {
    return false;
  }
  std::string res_path = util::to_std(res->get_path());
  const std::string memory_prefix = "memory://";
  if (res_path.compare(0, memory_prefix.size(), memory_prefix) != 0) {
    return false;
  }
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
  out_error = std::move(e);
  return true;
}

void add_camera2d_serialization_note(mcp::JsonValue &result,
                                     const std::string &prop_str,
                                     godot::Node *node) {
  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs || !cdbs->is_parent_class(node->get_class(),
                                      godot::StringName("Camera2D"))) {
    return;
  }
  if (prop_str == "enabled") {
    result["serialization_note"] =
        mcp::JsonValue("Camera2D enabled defaults to true. If setting to "
                       "true (the default), it won't appear in .tscn. "
                       "If setting to false, it will serialize correctly. "
                       "To ensure initial enabled state, use code_execute: "
                       "get_node(\"Path/To/Camera2D\").set_enabled(true/"
                       "false) in _ready().");
  } else if (prop_str == "current") {
    result["serialization_note"] = mcp::JsonValue(
        "Camera2D current is not a registered property (no setter/getter). "
        "Cannot be set via property_set. "
        "Use code_execute: get_node(\"Path/To/Camera2D\").make_current()");
  }
}

// 与 util::check_readback 的值类型近似比较清单保持同步。
bool is_readback_value_type(godot::Variant::Type type) {
  switch (type) {
  case godot::Variant::VECTOR2:
  case godot::Variant::VECTOR2I:
  case godot::Variant::RECT2:
  case godot::Variant::RECT2I:
  case godot::Variant::VECTOR3:
  case godot::Variant::VECTOR3I:
  case godot::Variant::TRANSFORM2D:
  case godot::Variant::VECTOR4:
  case godot::Variant::VECTOR4I:
  case godot::Variant::PLANE:
  case godot::Variant::QUATERNION:
  case godot::Variant::AABB:
  case godot::Variant::BASIS:
  case godot::Variant::TRANSFORM3D:
  case godot::Variant::PROJECTION:
  case godot::Variant::COLOR:
    return true;
  default:
    return false;
  }
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
  type_hint = util::infer_type_hint(dict, std::move(type_hint));

  godot::StringName prop_name(prop_str.c_str());
  std::string value_node_path;
  godot::Variant value;
  bool converted_node_path = false;
  ArrayValueConversion array_conversion =
      convert_array_value(dict, prop_str, path_str, *it_val);
  if (array_conversion.has_error) {
    return array_conversion.error;
  }
  bool array_converted = array_conversion.handled;
  if (array_converted) {
    value = array_conversion.value;
  }
  if (!array_converted) {
    NodePathValueConversion node_path_conversion =
        convert_node_path_value(dict, prop_str, path_str, *it_val);
    if (node_path_conversion.has_error) {
      return node_path_conversion.error;
    }
    if (node_path_conversion.converted) {
      value = node_path_conversion.value;
      converted_node_path = true;
      value_node_path = node_path_conversion.node_path;
    }
  }
  bool resource_attached = false;
  if (!array_converted && !converted_node_path) {
    std::string resource_error;
    if (resource_ops::try_resolve_resource_value(*it_val, value,
                                                 resource_error)) {
      if (!resource_error.empty()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(resource_error);
        return e;
      }
      resource_attached = true;
    } else {
      value = VariantJson::deserialize_strict(*it_val, type_hint);
    }
  }

  mcp::JsonValue blocked_error;
  if (resource_attached && memory_resource_assignment_blocked(
                               value, prop_str, path_str, blocked_error)) {
    return blocked_error;
  }

  godot::Variant old_val = node->get(prop_name);
  node->set(prop_name, value);

  godot::Variant new_val = node->get(prop_name);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  if (resource_attached) {
    r["resource_attached"] = mcp::JsonValue(true);
  }
  if (converted_node_path) {
    r["converted_node_path"] = mcp::JsonValue(value_node_path);
  }

  std::string readback_detail;
  bool type_sensitive = false;
  if (dict.has("type")) {
    auto prop_type = static_cast<godot::Variant::Type>(
        static_cast<int>(dict["type"]));
    type_sensitive = prop_type == godot::Variant::OBJECT ||
                     prop_type == godot::Variant::ARRAY ||
                     is_readback_value_type(prop_type);
  }
  util::ReadbackStatus readback =
      util::check_readback(value, old_val, new_val, readback_detail,
                           type_sensitive);
  if (readback == util::ReadbackStatus::REJECTED) {
    node->set(prop_name, old_val);
    bool restored = node->get(prop_name) == old_val;
    return util::error_detail(
        "value not applied: '" + prop_str + "' on " + path_str, path_str,
        "readback equals set value",
        std::string("property rejected the assigned value (type mismatch or "
                    "read-only); ") +
            (restored ? "old value restored"
                      : "old value could not be restored"));
  }
  if (readback == util::ReadbackStatus::CONVERTED) {
    r["warning"] = mcp::JsonValue("set applied; " + readback_detail);
  }

  add_camera2d_serialization_note(r, prop_str, node);

  mcp::JsonValue undo_info(mcp::JsonValue::object_tag);
  undo_info["path"] = mcp::JsonValue(path_str);
  undo_info["property"] = mcp::JsonValue(prop_str);
  undo_info["old_value"] = VariantJson::serialize(old_val);
  r["undo"] = std::move(undo_info);

  bool object_typed =
      old_val.get_type() == godot::Variant::OBJECT ||
      new_val.get_type() == godot::Variant::OBJECT;
  bool undoable = false;
  std::string undo_skip_reason;
  if (object_typed) {
    undo_skip_reason =
        "object-typed value: the editor undo stack cannot restore object "
        "references reliably";
  } else {
    auto *editor = godot::EditorInterface::get_singleton();
    auto *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
    if (!undo_redo) {
      undo_skip_reason = "EditorUndoRedoManager not available";
    } else {
      undo_redo->create_action(godot::String(
          ("Set Property " + prop_str + " on " + path_str).c_str()));
      undo_redo->add_do_property(node, prop_name, new_val);
      undo_redo->add_undo_property(node, prop_name, old_val);
      undo_redo->commit_action(false);
      undoable = true;
    }
  }
  r["undoable"] = mcp::JsonValue(undoable);
  if (!undoable) {
    r["undo_skip_reason"] = mcp::JsonValue(undo_skip_reason);
  }
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

  bool only_script_variables = false;
  auto *it_only_script = args.Find("only_script_variables");
  if (it_only_script && it_only_script->IsBool()) {
    only_script_variables = it_only_script->GetBool();
  }
  std::string property_filter;
  auto *it_filter = args.Find("property_filter");
  if (it_filter && it_filter->IsString()) {
    property_filter = it_filter->GetString();
  }

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

    std::string name;
    if (dict.has("name")) {
      name = util::to_std(dict["name"].operator godot::String());
    }
    if (!property_filter.empty() &&
        name.find(property_filter) == std::string::npos) {
      continue;
    }
    if (only_script_variables) {
      int64_t usage_val = 0;
      if (dict.has("usage")) {
        usage_val = static_cast<int64_t>(dict["usage"]);
      }
      if (!(usage_val & 4096)) {
        continue;
      }
    }

    mcp::JsonValue item(mcp::JsonValue::object_tag);

    if (dict.has("name")) {
      item["name"] = mcp::JsonValue(name);
    }
    if (dict.has("type")) {
      int type_id = static_cast<int>(dict["type"]);
      item["type_id"] = mcp::JsonValue(static_cast<int64_t>(type_id));
      item["type"] = mcp::JsonValue(util::to_std(godot::Variant::get_type_name(
          static_cast<godot::Variant::Type>(type_id))));
    }
    if (dict.has("hint")) {
      int hint_val = static_cast<int>(dict["hint"]);
      item["hint"] = mcp::JsonValue(hint_name(hint_val));
    }
    if (dict.has("hint_string")) {
      item["hint_string"] = mcp::JsonValue(
          util::to_std(dict["hint_string"].operator godot::String()));
    }
    if (dict.has("usage")) {
      int usage_val = static_cast<int>(dict["usage"]);
      item["usage"] = mcp::JsonValue(usage_name(usage_val));
    }
    if (dict.has("class_name")) {
      item["class_name"] = mcp::JsonValue(
          util::to_std(dict["class_name"].operator godot::String()));
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
} // namespace godot_autopilot
