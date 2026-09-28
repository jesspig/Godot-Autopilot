#include "property_ops.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/inline_resource_json.hpp"
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
#include <godot_cpp/classes/object.hpp>
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

godot::Dictionary find_property_info(godot::Object *object,
                                     const std::string &prop_name) {
  godot::TypedArray<godot::Dictionary> props = object->get_property_list();
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

std::string find_object_candidates(godot::Object *object,
                                     const std::string &prop_name) {
  struct Candidate {
    int distance;
    std::string name;
  };
  godot::TypedArray<godot::Dictionary> props = object->get_property_list();
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

std::vector<std::string> split_property_path(const std::string &full) {
  std::vector<std::string> parts;
  std::string current;
  for (char c : full) {
    if (c == '.') {
      parts.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  parts.push_back(current);
  return parts;
}

bool is_valid_path_segment(const std::string &seg) {
  if (seg.empty())
    return false;
  for (char c : seg) {
    if (c == ':' || c == '(' || c == ')' || c == '[' || c == ']' ||
        c == ' ' || c == '\t' || c == '"' || c == '\'')
      return false;
  }
  return true;
}

std::string describe_property_owner(godot::Object *owner,
                                    const std::string &fallback_path) {
  godot::Resource *as_res = godot::Object::cast_to<godot::Resource>(owner);
  if (as_res) {
    std::string res_path = util::to_std(as_res->get_path());
    if (!res_path.empty())
      return util::to_std(as_res->get_class()) + " resource '" + res_path +
             "'";
    return util::to_std(as_res->get_class()) +
           " resource (pathless sub-resource)";
  }
  godot::Node *as_node = godot::Object::cast_to<godot::Node>(owner);
  if (as_node)
    return fallback_path;
  return util::to_std(owner->get_class()) + " object";
}

struct DottedTarget {
  bool ok = false;
  godot::Object *owner = nullptr;
  std::string leaf;
  godot::Dictionary info;
  mcp::JsonValue error;
};

DottedTarget resolve_dotted_target(godot::Object *root,
                                   const std::string &full,
                                   const std::string &path_str) {
  DottedTarget out;
  std::vector<std::string> parts = split_property_path(full);
  if (parts.size() == 1) {
    if (!is_valid_path_segment(full)) {
      out.error = util::error_detail(
          "invalid property path '" + full + "' on " + path_str, path_str,
          "a property name or dot-separated property path (a.b.c)",
          "use property_get_list to see available properties");
      return out;
    }
    godot::Dictionary info = find_property_info(root, full);
    if (info.is_empty()) {
      std::string owner_desc = describe_property_owner(root, path_str);
      std::string candidates;
      godot::Node *as_node = godot::Object::cast_to<godot::Node>(root);
      if (as_node)
        candidates = find_property_candidates(as_node, full);
      else
        candidates = find_object_candidates(root, full);
      std::string family_hint;
      std::vector<std::string> family = find_property_family(root, full);
      if (!family.empty())
        family_hint =
            "; the exact name is per item, e.g. '" + family[0] + "'";
      out.error = util::error_detail(
          "property '" + full + "' does not exist on " + owner_desc, path_str,
          "a valid property name from get_property_list",
          "use property_get_list to see available properties; candidate: " +
              candidates + family_hint);
      return out;
    }
    out.ok = true;
    out.owner = root;
    out.leaf = full;
    out.info = info;
    return out;
  }
  godot::Object *current = root;
  for (size_t i = 0; i + 1 < parts.size(); i++) {
    const std::string &seg = parts[i];
    if (!is_valid_path_segment(seg)) {
      out.error = util::error_detail(
          "invalid segment '" + seg + "' in property path '" + full + "' on " +
              path_str,
          path_str,
          "dot-separated property names without method calls or indexing",
          "use plain property names joined by '.', e.g. material.albedo_color");
      return out;
    }
    godot::Dictionary seg_info = find_property_info(current, seg);
    if (seg_info.is_empty()) {
      std::string owner_desc = describe_property_owner(current, path_str);
      std::string candidates;
      godot::Node *as_node = godot::Object::cast_to<godot::Node>(current);
      if (as_node)
        candidates = find_property_candidates(as_node, seg);
      else
        candidates = find_object_candidates(current, seg);
      out.error = util::error_detail(
          "property path '" + full + "' does not exist on " + path_str +
              ": segment '" + seg + "' not found on " + owner_desc,
          path_str, "a valid property path from get_property_list",
          "use property_get_list to see available properties; candidate: " +
              candidates);
      return out;
    }
    godot::Variant intermediate =
        current->get(godot::StringName(seg.c_str()));
    if (intermediate.get_type() != godot::Variant::OBJECT) {
      out.error = util::error_detail(
          "property path '" + full + "' does not exist on " + path_str +
              ": segment '" + seg + "' is not an object and cannot be traversed",
          path_str, "an object-typed intermediate property",
          "use property_get_list to see available properties");
      return out;
    }
    godot::Object *next = intermediate.operator godot::Object *();
    if (!next) {
      out.error = util::error_detail(
          "property path '" + full + "' does not exist on " + path_str +
              ": segment '" + seg + "' holds a null object",
          path_str, "a non-null object-typed intermediate property",
          "assign the intermediate property first, then set '" + full + "'");
      return out;
    }
    current = next;
  }
  const std::string &leaf = parts.back();
  if (!is_valid_path_segment(leaf)) {
    out.error = util::error_detail(
        "invalid segment '" + leaf + "' in property path '" + full + "' on " +
            path_str,
        path_str,
        "dot-separated property names without method calls or indexing",
        "use plain property names joined by '.', e.g. material.albedo_color");
    return out;
  }
  godot::Dictionary leaf_info = find_property_info(current, leaf);
  if (leaf_info.is_empty()) {
    std::string owner_desc = describe_property_owner(current, path_str);
    std::string candidates;
    godot::Node *as_node = godot::Object::cast_to<godot::Node>(current);
    if (as_node)
      candidates = find_property_candidates(as_node, leaf);
    else
      candidates = find_object_candidates(current, leaf);
    std::string family_hint;
    std::vector<std::string> family = find_property_family(current, leaf);
    if (!family.empty())
      family_hint = "; the exact name is per item, e.g. '" + family[0] + "'";
    out.error = util::error_detail(
        "property path '" + full + "' does not exist on " + path_str +
            ": leaf '" + leaf + "' not found on " + owner_desc,
        path_str, "a valid property path from get_property_list",
        "use property_get_list to see available properties; candidate: " +
            candidates + family_hint);
    return out;
  }
  out.ok = true;
  out.owner = current;
  out.leaf = leaf;
  out.info = leaf_info;
  return out;
}

mcp::JsonValue try_expand_property_family(godot::Object *node,
                                          const std::string &prop,
                                          const mcp::JsonValue &value,
                                          const mcp::JsonValue &args,
                                          const std::string &path_str) {
  if (prop.find('.') != std::string::npos || !value.IsObject() ||
      value.GetObject().empty())
    return mcp::JsonValue();
  std::vector<std::string> family = find_property_family(node, prop);
  if (family.empty())
    return mcp::JsonValue();
  std::vector<std::string> available;
  for (const std::string &full_name : family)
    available.push_back(full_name.substr(prop.size() + 1));
  mcp::JsonValue expanded_props(mcp::JsonValue::object_tag);
  std::string invalid_item;
  for (const auto &entry : value.GetObject()) {
    std::string name = prop + "/" + entry.first;
    godot::Dictionary info = find_property_info(node, name);
    if (info.is_empty()) {
      invalid_item = entry.first;
      break;
    }
    expanded_props[name] = entry.second;
  }
  if (!invalid_item.empty()) {
    std::string available_list;
    for (size_t i = 0; i < available.size(); i++) {
      if (i > 0)
        available_list += ", ";
      available_list += "'" + available[i] + "'";
    }
    return util::error_detail(
        "property '" + prop + "' on " +
            describe_property_owner(node, path_str) +
            " is a property family and must be expanded per item; item '" +
            invalid_item + "' is not a valid family item",
        path_str, "an object mapping family item names to values",
        "available items: " + available_list +
            "; pass the exact names (e.g. '" + prop + "/" + available[0] +
            "') or an object with these suffixes");
  }
  mcp::JsonValue new_args(mcp::JsonValue::object_tag);
  const mcp::JsonValue *original_path = args.Find("path");
  if (original_path && original_path->IsString())
    new_args["path"] = *original_path;
  else
    new_args["path"] = mcp::JsonValue(path_str);
  new_args["properties"] = std::move(expanded_props);
  mcp::JsonValue result = handle_set(new_args);
  if (result.IsObject() && !result.Find("error"))
    result["expanded_from"] = mcp::JsonValue(prop);
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

struct DictionaryValueConversion {
  bool handled = false;
  bool has_error = false;
  mcp::JsonValue error;
  godot::Variant value;
};

DictionaryValueConversion
convert_dictionary_value(const godot::Dictionary &dict,
                         const std::string &prop_str,
                         const std::string &path_str,
                         const mcp::JsonValue &raw_value) {
  DictionaryValueConversion result;
  if (dict.is_empty() || !dict.has("type")) {
    return result;
  }
  if (static_cast<godot::Variant::Type>(static_cast<int>(dict["type"])) !=
      godot::Variant::DICTIONARY) {
    return result;
  }
  result.handled = true;
  if (!raw_value.IsObject()) {
    result.has_error = true;
    result.error = util::error_detail(
        "value for dictionary property '" + prop_str + "' on " + path_str +
            " is not a JSON object; refusing to write",
        path_str, "a JSON object",
        "pass an object mapping keys to values");
    return result;
  }
  std::string hint = util::infer_type_hint(dict, std::string());
  result.value = VariantJson::deserialize(raw_value, hint);
  return result;
}

double animations_number(const mcp::JsonValue &v, double fallback) {
  if (v.IsInt())
    return static_cast<double>(v.GetInt());
  if (v.IsDouble())
    return v.GetDouble();
  return fallback;
}

bool convert_animation_frame(const mcp::JsonValue &frame,
                             const std::string &anim_name,
                             const std::string &full_prop,
                             const std::string &path_str,
                             const std::string &owner_desc,
                             godot::Variant &out_value,
                             mcp::JsonValue &out_error) {
  godot::Variant texture_value;
  double duration = 1.0;
  bool direct_ref = frame.IsString();
  if (!direct_ref && frame.IsObject()) {
    direct_ref = frame.Find("path") != nullptr || frame.Find("resource") != nullptr;
  }
  if (direct_ref) {
    std::string resolve_error;
    godot::Variant resolved;
    bool attempted = resource_ops::try_resolve_resource_value(frame, resolved,
                                                              resolve_error);
    if (!attempted || !resolve_error.empty()) {
      std::string detail = resolve_error.empty()
                               ? "unsupported texture reference"
                               : resolve_error;
      out_error = util::error_detail(
          "cannot convert frame of animation '" + anim_name +
              "' in property '" + full_prop + "' of " + owner_desc + ": " +
              detail,
          path_str, "a Texture2D resource reference",
          "pass a res:// path string or {\"path\": \"res://...\"}");
      return false;
    }
    texture_value = resolved;
  } else if (frame.IsObject()) {
    auto *tex_field = frame.Find("texture");
    if (!tex_field) {
      out_error = util::error_detail(
          "cannot convert frame of animation '" + anim_name +
              "' in property '" + full_prop + "' of " + owner_desc +
              ": missing 'texture'",
          path_str, "a frame with a Texture2D 'texture' and optional 'duration'",
          "pass {\"texture\": \"res://...\", \"duration\": 1.0}");
      return false;
    }
    std::string resolve_error;
    godot::Variant resolved;
    bool attempted = resource_ops::try_resolve_resource_value(
        *tex_field, resolved, resolve_error);
    if (!attempted || !resolve_error.empty()) {
      std::string detail = resolve_error.empty()
                               ? "unsupported texture reference"
                               : resolve_error;
      out_error = util::error_detail(
          "cannot convert frame texture of animation '" + anim_name +
              "' in property '" + full_prop + "' of " + owner_desc + ": " +
              detail,
          path_str, "a Texture2D resource reference",
          "pass a res:// path string or {\"path\": \"res://...\"}");
      return false;
    }
    texture_value = resolved;
    auto *dur_field = frame.Find("duration");
    if (dur_field) {
      if (!dur_field->IsNumber()) {
        out_error = util::error_detail(
            "cannot convert frame of animation '" + anim_name +
                "' in property '" + full_prop + "' of " + owner_desc +
                ": 'duration' must be a number",
            path_str, "a numeric 'duration'", "pass a number, e.g. 1.0");
        return false;
      }
      duration = animations_number(*dur_field, 1.0);
    }
  } else {
    out_error = util::error_detail(
        "cannot convert frame of animation '" + anim_name +
            "' in property '" + full_prop + "' of " + owner_desc,
        path_str, "a Texture2D resource reference or a frame object",
        "pass a res:// path string or {\"texture\": \"res://...\", "
        "\"duration\": 1.0}");
    return false;
  }
  godot::Dictionary frame_dict;
  frame_dict[godot::String("texture")] = texture_value;
  frame_dict[godot::String("duration")] = duration;
  out_value = godot::Variant(frame_dict);
  return true;
}

bool convert_single_animation(const std::string &anim_name,
                              const mcp::JsonValue &def,
                              const std::string &full_prop,
                              const std::string &path_str,
                              const std::string &owner_desc,
                              godot::Dictionary &out_dict,
                              mcp::JsonValue &out_error) {
  if (anim_name.empty()) {
    out_error = util::error_detail(
        "cannot convert property '" + full_prop + "' of " + owner_desc +
            ": animation name must be non-empty",
        path_str, "non-empty animation names",
        "use the animation name as the object key");
    return false;
  }
  const mcp::JsonValue *frames_field = nullptr;
  double speed = 5.0;
  bool loop = true;
  if (def.IsArray()) {
    frames_field = &def;
  } else if (def.IsObject()) {
    auto *name_field = def.Find("name");
    if (name_field && name_field->IsString() &&
        name_field->GetString() != anim_name) {
      out_error = util::error_detail(
          "cannot convert animation '" + anim_name + "' in property '" +
              full_prop + "' of " + owner_desc +
              ": 'name' field does not match the object key",
          path_str, "a matching 'name' or no 'name' field",
          "remove 'name' or make it equal to the object key");
      return false;
    }
    auto *f = def.Find("frames");
    if (!f || !f->IsArray()) {
      out_error = util::error_detail(
          "cannot convert animation '" + anim_name + "' in property '" +
              full_prop + "' of " + owner_desc + ": missing 'frames' array",
          path_str, "an object with a 'frames' array",
          "pass {\"frames\": [...], \"speed\": 5.0, \"loop\": true}");
      return false;
    }
    frames_field = f;
    auto *speed_field = def.Find("speed");
    if (!speed_field)
      speed_field = def.Find("fps");
    if (speed_field) {
      if (!speed_field->IsNumber()) {
        out_error = util::error_detail(
            "cannot convert animation '" + anim_name + "' in property '" +
                full_prop + "' of " + owner_desc + ": 'speed' must be a number",
            path_str, "a numeric 'speed'", "pass a number, e.g. 5.0");
        return false;
      }
      speed = animations_number(*speed_field, 5.0);
    }
    auto *loop_field = def.Find("loop");
    if (loop_field) {
      if (!loop_field->IsBool()) {
        out_error = util::error_detail(
            "cannot convert animation '" + anim_name + "' in property '" +
                full_prop + "' of " + owner_desc + ": 'loop' must be a boolean",
            path_str, "a boolean 'loop'", "pass true or false");
        return false;
      }
      loop = loop_field->GetBool();
    }
  } else {
    out_error = util::error_detail(
        "cannot convert animation '" + anim_name + "' in property '" +
            full_prop + "' of " + owner_desc + ": expected an array or object",
        path_str, "a frames array or an animation object",
        "pass {\"frames\": [...]} or [...]");
    return false;
  }
  godot::Array frames_array;
  const mcp::JsonValue::Array &frames = frames_field->GetArray();
  for (size_t i = 0; i < frames.size(); i++) {
    godot::Variant converted;
    mcp::JsonValue frame_error;
    if (!convert_animation_frame(frames[i], anim_name, full_prop, path_str,
                                 owner_desc, converted, frame_error)) {
      out_error = std::move(frame_error);
      return false;
    }
    frames_array.append(converted);
  }
  out_dict[godot::String("name")] = godot::String::utf8(anim_name.c_str());
  out_dict[godot::String("frames")] = frames_array;
  out_dict[godot::String("speed")] = speed;
  out_dict[godot::String("loop")] = loop;
  return true;
}

struct SpriteFramesAnimationsConversion {
  bool handled = false;
  bool has_error = false;
  mcp::JsonValue error;
  godot::Variant value;
};

SpriteFramesAnimationsConversion convert_spriteframes_animations(
    godot::Object *owner, const std::string &leaf,
    const std::string &full_prop, const std::string &path_str,
    const mcp::JsonValue &raw_value) {
  SpriteFramesAnimationsConversion result;
  if (leaf != "animations") {
    return result;
  }
  if (util::to_std(owner->get_class()) != "SpriteFrames") {
    return result;
  }
  result.handled = true;
  std::string owner_desc = describe_property_owner(owner, path_str);
  godot::Array out_array;
  if (raw_value.IsObject()) {
    for (const auto &entry : raw_value.GetObject()) {
      godot::Dictionary anim_dict;
      mcp::JsonValue item_error;
      if (!convert_single_animation(entry.first, entry.second, full_prop,
                                    path_str, owner_desc, anim_dict,
                                    item_error)) {
        result.has_error = true;
        result.error = std::move(item_error);
        return result;
      }
      out_array.append(anim_dict);
    }
    result.value = godot::Variant(out_array);
    return result;
  }
  if (raw_value.IsArray()) {
    const mcp::JsonValue::Array &items = raw_value.GetArray();
    for (size_t i = 0; i < items.size(); i++) {
      const mcp::JsonValue &item = items[i];
      if (!item.IsObject()) {
        result.has_error = true;
        result.error = util::error_detail(
            "cannot convert element " + std::to_string(i) + " of property '" +
                full_prop + "' of " + owner_desc +
                ": expected an object with name/frames/speed/loop",
            path_str, "animation objects",
            "pass {\"name\": \"idle\", \"frames\": [...], \"speed\": 5.0, "
            "\"loop\": true}");
        return result;
      }
      auto *name_field = item.Find("name");
      if (!name_field || !name_field->IsString() ||
          name_field->GetString().empty()) {
        result.has_error = true;
        result.error = util::error_detail(
            "cannot convert element " + std::to_string(i) + " of property '" +
                full_prop + "' of " + owner_desc + ": missing 'name'",
            path_str, "an animation object with a non-empty 'name'",
            "pass {\"name\": \"idle\", \"frames\": [...]}");
        return result;
      }
      std::string anim_name = name_field->GetString();
      godot::Dictionary anim_dict;
      mcp::JsonValue item_error;
      if (!convert_single_animation(anim_name, item, full_prop, path_str,
                                    owner_desc, anim_dict, item_error)) {
        result.has_error = true;
        result.error = std::move(item_error);
        return result;
      }
      out_array.append(anim_dict);
    }
    result.value = godot::Variant(out_array);
    return result;
  }
  result.has_error = true;
  result.error = util::error_detail(
      "value for property '" + full_prop + "' of " + owner_desc +
          " is not an array or object; refusing to write",
      path_str, "a JSON array or an object mapping animation names to definitions",
      "pass [{\"name\": \"idle\", ...}] or {\"idle\": {\"frames\": [...]}}");
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
      " — memory:// resources are not persistent and will corrupt the scene "
      "file if saved. To create a pathless sub-resource in one step pass "
      "{\"type\": \"RectangleShape2D\", \"properties\": {...}} as the value "
      "(it is written as a sub_resource when the scene is saved). "
      "Alternatively save it first with save_resource and pass {\"path\": "
      "\"res://...\"}, or edit the memory resource itself with "
      "set_resource_property.");
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

bool is_type_sensitive_property(const godot::Dictionary &info) {
  if (!info.has("type")) {
    return false;
  }
  auto prop_type =
      static_cast<godot::Variant::Type>(static_cast<int>(info["type"]));
  return prop_type == godot::Variant::OBJECT ||
         prop_type == godot::Variant::ARRAY ||
         is_readback_value_type(prop_type);
}

const char *const INLINE_RESOURCE_ACTION_TEXT =
    "create the resource inline with {\"type\": \"RectangleShape2D\", "
    "\"properties\": {...}}, pass a res:// path in {\"path\": \"res://...\"}, "
    "or save the resource to disk first and pass its res:// path";

} // namespace

bool apply_inline_resource_property(godot::Resource *res,
                                    const std::string &sub_prop,
                                    const std::string &node_property,
                                    const std::string &path_str,
                                    const mcp::JsonValue &raw_value, int depth,
                                    mcp::JsonValue &out_error) {
  DottedTarget target = resolve_dotted_target(res, sub_prop, path_str);
  if (!target.ok) {
    out_error = std::move(target.error);
    return false;
  }
  godot::Object *owner = target.owner;
  const std::string &leaf = target.leaf;
  godot::Dictionary info = target.info;
  std::string owner_resource_desc = describe_property_owner(owner, path_str);
  const std::string owner_label =
      owner == static_cast<godot::Object *>(res)
          ? "inline " + util::to_std(res->get_class()) +
                " resource for property '" + node_property + "' on " + path_str
          : owner_resource_desc + " for property '" + node_property + "' on " +
                path_str;
  godot::Variant value;
  bool value_ready = false;
  SpriteFramesAnimationsConversion sprite_conversion =
      convert_spriteframes_animations(owner, leaf, sub_prop, path_str,
                                      raw_value);
  if (sprite_conversion.has_error) {
    out_error = std::move(sprite_conversion.error);
    return false;
  }
  if (sprite_conversion.handled) {
    value = sprite_conversion.value;
    value_ready = true;
  }
  ArrayValueConversion array_conversion;
  DictionaryValueConversion dictionary_conversion;
  if (!value_ready) {
    array_conversion =
        convert_array_value(info, sub_prop, path_str, raw_value);
    if (array_conversion.has_error) {
      out_error = std::move(array_conversion.error);
      return false;
    }
    if (array_conversion.handled) {
      value = array_conversion.value;
      value_ready = true;
    }
  }
  if (!value_ready) {
    dictionary_conversion =
        convert_dictionary_value(info, sub_prop, path_str, raw_value);
    if (dictionary_conversion.has_error) {
      out_error = std::move(dictionary_conversion.error);
      return false;
    }
    if (dictionary_conversion.handled) {
      value = dictionary_conversion.value;
      value_ready = true;
    }
  }
  if (!value_ready) {
    NodePathValueConversion node_path_conversion =
        convert_node_path_value(info, sub_prop, path_str, raw_value);
    if (node_path_conversion.has_error) {
      out_error = std::move(node_path_conversion.error);
      return false;
    }
    if (node_path_conversion.converted) {
      value = node_path_conversion.value;
      value_ready = true;
    } else {
      std::string resource_error;
      bool resource_attached = false;
      if (resource_ops::try_resolve_resource_value(raw_value, value,
                                                   resource_error)) {
        if (!resource_error.empty()) {
          out_error = util::error_detail(
              "cannot convert property '" + sub_prop + "' of " + owner_label +
                  ": " + resource_error,
              path_str,
              "a " + util::to_std(owner->get_class()) + " resource reference",
              INLINE_RESOURCE_ACTION_TEXT);
          return false;
        }
        resource_attached = true;
        value_ready = true;
      } else {
        InlineResourceBuild nested = build_inline_resource_value(
            info, raw_value, node_property, path_str, depth);
        if (nested.has_error) {
          out_error = std::move(nested.error);
          return false;
        }
        if (nested.active) {
          value = nested.value;
          value_ready = true;
        } else {
          value = VariantJson::deserialize_strict(
              raw_value, util::infer_type_hint(info, std::string()));
          value_ready = true;
        }
      }
      if (resource_attached) {
        mcp::JsonValue blocked_error;
        if (memory_resource_assignment_blocked(value, node_property, path_str,
                                               blocked_error)) {
          out_error = std::move(blocked_error);
          return false;
        }
      }
    }
  }

  godot::StringName prop_name(leaf.c_str());
  godot::Variant old_val = owner->get(prop_name);
  owner->set(prop_name, value);
  godot::Variant new_val = owner->get(prop_name);

  std::string readback_detail;
  util::ReadbackStatus readback =
      util::check_readback(value, old_val, new_val, readback_detail,
                           is_type_sensitive_property(info));
  if (readback == util::ReadbackStatus::REJECTED) {
    owner->set(prop_name, old_val);
    out_error = util::error_detail(
        "value not applied: property '" + sub_prop + "' of " + owner_label,
        path_str, "readback equals set value",
        "the resource rejected the assigned value (type mismatch or "
        "read-only): " +
            readback_detail);
    return false;
  }
  return true;
}

InlineResourceBuild
build_inline_resource_value(const godot::Dictionary &prop_info,
                            const mcp::JsonValue &raw_value,
                            const std::string &node_property,
                            const std::string &path_str, int depth) {
  InlineResourceBuild result;
  if (!prop_info.has("type") ||
      static_cast<godot::Variant::Type>(static_cast<int>(prop_info["type"])) !=
          godot::Variant::OBJECT) {
    return result;
  }
  const util::inline_resource::Description description =
      util::inline_resource::inspect(raw_value);
  if (description.shape == util::inline_resource::Shape::not_inline) {
    return result;
  }
  result.active = true;
  result.type = description.type;
  if (description.shape == util::inline_resource::Shape::invalid_properties) {
    result.has_error = true;
    result.error = util::error_detail(
        "inline resource description for property '" + node_property +
            "' on " + path_str + " has a non-object 'properties' value",
        path_str,
        "an object mapping property names to values, e.g. {\"type\": "
        "\"RectangleShape2D\", \"properties\": {\"size\": {\"x\": 20, "
        "\"y\": 28}}}",
        "pass an object, or omit 'properties' to create the resource with "
        "default values");
    return result;
  }
  if (depth == 1) {
    const int declared_depth = util::inline_resource::nested_depth(raw_value);
    if (declared_depth > util::inline_resource::kMaxDepth) {
      result.has_error = true;
      result.error = util::error_detail(
          "inline resource description for property '" + node_property +
              "' on " + path_str + " nests " +
              std::to_string(declared_depth) + " levels deep (limit " +
              std::to_string(util::inline_resource::kMaxDepth) + ")",
          path_str,
          "an inline resource hierarchy at most " +
              std::to_string(util::inline_resource::kMaxDepth) +
              " levels deep",
          "flatten the description, or save the inner resource to disk and "
          "reference it with {\"path\": \"res://...\"}");
      return result;
    }
  }
  if (depth > util::inline_resource::kMaxDepth) {
    result.has_error = true;
    result.error = util::error_detail(
        "inline resource for property '" + node_property + "' on " + path_str +
            " exceeds the " +
            std::to_string(util::inline_resource::kMaxDepth) +
            "-level nesting limit (reached at type " + description.type + ")",
        path_str,
        "an inline resource hierarchy at most " +
            std::to_string(util::inline_resource::kMaxDepth) + " levels deep",
        "flatten the description, or save the inner resource to disk and "
        "reference it with {\"path\": \"res://...\"}");
    return result;
  }
  std::string class_error;
  godot::Ref<godot::Resource> res =
      resource_ops::instantiate_resource_class(description.type, class_error);
  if (res.is_null()) {
    result.has_error = true;
    result.error = util::error_detail(
        "cannot create inline resource '" + description.type +
            "' for property '" + node_property + "' on " + path_str + ": " +
            class_error,
        path_str,
        "an instantiable Resource subclass (engine class or global script "
        "class)",
        INLINE_RESOURCE_ACTION_TEXT);
    return result;
  }
  const mcp::JsonValue *properties = raw_value.Find("properties");
  if (properties != nullptr && properties->IsObject()) {
    for (const auto &entry : *properties) {
      mcp::JsonValue property_error;
      if (!apply_inline_resource_property(res.ptr(), entry.first, node_property,
                                          path_str, entry.second, depth + 1,
                                          property_error)) {
        result.has_error = true;
        result.error = std::move(property_error);
        return result;
      }
    }
  }
  result.value = godot::Variant(res.ptr());
  return result;
}

namespace {

constexpr int64_t kPropertyBatchMax = 32;

bool batch_property_is_object_typed(godot::Node *node,
                                    const std::string &prop_name,
                                    const mcp::JsonValue &raw_value) {
  std::string root_name = prop_name;
  size_t dot = prop_name.find('.');
  if (dot != std::string::npos)
    root_name = prop_name.substr(0, dot);
  godot::TypedArray<godot::Dictionary> props = node->get_property_list();
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (!dict.has("name") || !dict.has("type"))
      continue;
    if (util::to_std(dict["name"].operator godot::String()) != root_name)
      continue;
    if (static_cast<godot::Variant::Type>(static_cast<int>(dict["type"])) ==
        godot::Variant::OBJECT)
      return true;
    break;
  }
  if (raw_value.IsString()) {
    const std::string str = raw_value.GetString();
    return str.rfind("res://", 0) == 0 || str.rfind("memory://", 0) == 0;
  }
  if (raw_value.IsObject()) {
    const mcp::JsonValue *ref = raw_value.Find("path");
    if (!ref)
      ref = raw_value.Find("resource");
    if (ref && ref->IsString())
      return true;
    const mcp::JsonValue *type_field = raw_value.Find("type");
    if (type_field && type_field->IsString() &&
        !type_field->GetString().empty())
      return true;
  }
  return false;
}

mcp::JsonValue read_single_property(godot::Node *node,
                                    const std::string &path_str,
                                    const std::string &prop_str,
                                    godot::Variant &out_value) {
  godot::Dictionary prop_info = find_property_info(node, prop_str);
  if (prop_info.is_empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "property not found: " + prop_str + " on node " + path_str +
        " — use property_get_list to see available properties");
    return e;
  }
  out_value =
      node->get(godot::StringName(godot::String::utf8(prop_str.c_str())));
  return mcp::JsonValue();
}

} // namespace

std::vector<std::string> find_property_family(godot::Object *object,
                                              const std::string &prefix,
                                              size_t limit) {
  std::vector<std::string> names;
  if (!object || prefix.empty() || limit == 0)
    return names;
  const std::string needle = prefix + "/";
  godot::TypedArray<godot::Dictionary> props = object->get_property_list();
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (!dict.has("name"))
      continue;
    std::string name = util::to_std(dict["name"].operator godot::String());
    if (name.size() <= needle.size() ||
        name.compare(0, needle.size(), needle) != 0)
      continue;
    names.push_back(name);
    if (names.size() >= limit)
      break;
  }
  return names;
}

mcp::JsonValue handle_get(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  auto *it_prop = args.Find("property");
  auto *it_props = args.Find("properties");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  if (args.IsObject()) {
    for (const auto &entry : args.GetObject()) {
      if (entry.first != "path" && entry.first != "property" &&
          entry.first != "properties") {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("unknown parameter for property_get: " +
                                    entry.first);
        return e;
      }
    }
  }
  const bool has_single = it_prop != nullptr;
  const bool has_batch = it_props != nullptr;
  if (has_single && has_batch) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "property and properties are mutually exclusive — pass exactly one");
    return e;
  }
  std::vector<std::string> batch_names;
  if (has_batch) {
    if (!it_props->IsArray() || it_props->GetArray().empty()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "properties must be a non-empty array of property name strings");
      return e;
    }
    for (const auto &item : it_props->GetArray()) {
      if (!item.IsString() || item.GetString().empty()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(
            "properties must contain non-empty property name strings");
        return e;
      }
      batch_names.push_back(item.GetString());
    }
    if (batch_names.size() > static_cast<size_t>(kPropertyBatchMax)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("properties exceeds maximum of " +
                                  std::to_string(kPropertyBatchMax) +
                                  " entries — split into smaller batches");
      return e;
    }
  } else if (!has_single || !it_prop->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: property");
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

  if (has_batch) {
    mcp::JsonValue values(mcp::JsonValue::object_tag);
    mcp::JsonValue missing(mcp::JsonValue::array_tag);
    for (const std::string &name : batch_names) {
      godot::Variant value;
      mcp::JsonValue item_error =
          read_single_property(node, path_str, name, value);
      if (!item_error.IsNull()) {
        missing.PushBack(mcp::JsonValue(name));
        continue;
      }
      values[name] = VariantJson::serialize(value);
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(values);
    if (missing.Size() > 0)
      r["missing"] = std::move(missing);
    return r;
  }

  std::string prop_str = it_prop->GetString();
  godot::Variant value;
  if (mcp::JsonValue item_error =
          read_single_property(node, path_str, prop_str, value);
      !item_error.IsNull())
    return item_error;

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = VariantJson::serialize(value);
  return r;
}

mcp::JsonValue handle_set(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  auto *it_prop = args.Find("property");
  auto *it_val = args.Find("value");
  auto *it_batch = args.Find("properties");
  auto *it_hint_early = args.Find("type_hint");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path_str = it_path->GetString();
  bool has_single_prop = it_prop != nullptr;
  bool has_single_val = it_val != nullptr;
  bool has_batch = it_batch != nullptr;
  if (has_batch && (has_single_prop || has_single_val)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "property/value and properties are mutually exclusive — pass exactly one form");
    return e;
  }
  if (has_batch && it_hint_early) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "type_hint cannot be used with properties — omit it to infer each property type");
    return e;
  }
  if (!has_batch) {
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
    if (args.IsObject()) {
      for (const auto &entry : args.GetObject()) {
        if (entry.first != "path" && entry.first != "property" &&
            entry.first != "value" && entry.first != "type_hint") {
          mcp::JsonValue e(mcp::JsonValue::object_tag);
          e["error"] = mcp::JsonValue("unknown parameter for property_set: " +
                                      entry.first);
          return e;
        }
      }
    }
  } else {
    if (args.IsObject()) {
      for (const auto &entry : args.GetObject()) {
        if (entry.first != "path" && entry.first != "properties") {
          mcp::JsonValue e(mcp::JsonValue::object_tag);
          e["error"] = mcp::JsonValue("unknown parameter for property_set: " +
                                      entry.first);
          return e;
        }
      }
    }
    if (!it_batch->IsObject() || it_batch->GetObject().empty()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "properties must be a non-empty object mapping property names to values");
      return e;
    }
    if (it_batch->GetObject().size() > static_cast<size_t>(kPropertyBatchMax)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("properties exceeds maximum of " +
                                  std::to_string(kPropertyBatchMax) +
                                  " entries — split into smaller batches");
      return e;
    }
  }

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
  if (has_batch) {
    std::vector<std::string> object_props;
    for (const auto &entry : it_batch->GetObject()) {
      if (entry.first.empty()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(
            "properties must contain non-empty property name strings");
        return e;
      }
      if (batch_property_is_object_typed(node, entry.first, entry.second))
        object_props.push_back(entry.first);
    }
    std::vector<std::string> applied;
    std::vector<mcp::JsonValue> warnings;
    std::vector<mcp::JsonValue> inlines;
    mcp::JsonValue converted(mcp::JsonValue::object_tag);
    size_t converted_count = 0;
    mcp::JsonValue undo_entries(mcp::JsonValue::array_tag);
    bool all_undoable = true;
    std::string undo_skip;
    for (int pass = 0; pass < 2; pass++) {
      for (const auto &entry : it_batch->GetObject()) {
        bool is_object = std::find(object_props.begin(), object_props.end(),
                                   entry.first) != object_props.end();
        if (is_object != (pass == 0))
          continue;
        mcp::JsonValue sub_args(mcp::JsonValue::object_tag);
        sub_args["path"] = mcp::JsonValue(path_str);
        sub_args["property"] = mcp::JsonValue(entry.first);
        mcp::JsonValue value_copy = entry.second;
        sub_args["value"] = std::move(value_copy);
        mcp::JsonValue sub = handle_set(sub_args);
        auto *sub_err = sub.Find("error");
        if (sub_err) {
          std::string sub_msg =
              sub_err->IsString() ? sub_err->GetString() : sub_err->Dump();
          mcp::JsonValue e(mcp::JsonValue::object_tag);
          e["error"] = mcp::JsonValue("failed to apply property '" +
                                      entry.first + "' on " + path_str + ": " +
                                      sub_msg);
          e["failed_property"] = mcp::JsonValue(entry.first);
          if (!applied.empty()) {
            mcp::JsonValue ap(mcp::JsonValue::array_tag);
            for (const std::string &name : applied)
              ap.PushBack(mcp::JsonValue(name));
            e["applied_properties"] = std::move(ap);
          }
          return e;
        }
        applied.push_back(entry.first);
        auto *warn = sub.Find("warning");
        if (warn && warn->IsString()) {
          mcp::JsonValue w(mcp::JsonValue::object_tag);
          w["path"] = mcp::JsonValue(path_str);
          w["property"] = mcp::JsonValue(entry.first);
          w["warning"] = mcp::JsonValue(warn->GetString());
          warnings.push_back(std::move(w));
        }
        auto *inline_rec = sub.Find("inline_resources");
        if (inline_rec && inline_rec->IsArray()) {
          for (const auto &inline_entry : inline_rec->GetArray()) {
            mcp::JsonValue inline_copy = inline_entry;
            inlines.push_back(std::move(inline_copy));
          }
        }
        auto *conv = sub.Find("converted_node_path");
        if (conv && conv->IsString()) {
          converted[entry.first] = mcp::JsonValue(conv->GetString());
          converted_count++;
        }
        auto *undo_entry = sub.Find("undo");
        if (undo_entry) {
          mcp::JsonValue undo_copy = *undo_entry;
          undo_entries.PushBack(std::move(undo_copy));
        }
        auto *undoable_flag = sub.Find("undoable");
        if (undoable_flag && undoable_flag->IsBool() &&
            !undoable_flag->GetBool()) {
          all_undoable = false;
          auto *reason = sub.Find("undo_skip_reason");
          if (reason && reason->IsString())
            undo_skip = reason->GetString();
        }
      }
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("ok");
    mcp::JsonValue ap(mcp::JsonValue::array_tag);
    for (const std::string &name : applied)
      ap.PushBack(mcp::JsonValue(name));
    r["applied_properties"] = std::move(ap);
    if (!warnings.empty()) {
      mcp::JsonValue warr(mcp::JsonValue::array_tag);
      for (auto &w : warnings)
        warr.PushBack(std::move(w));
      r["property_warnings"] = std::move(warr);
    }
    if (!inlines.empty()) {
      mcp::JsonValue iarr(mcp::JsonValue::array_tag);
      for (auto &item : inlines)
        iarr.PushBack(std::move(item));
      r["inline_resources"] = std::move(iarr);
    }
    if (converted_count > 0)
      r["converted_node_paths"] = std::move(converted);
    r["undo"] = std::move(undo_entries);
    r["undoable"] = mcp::JsonValue(all_undoable);
    if (!all_undoable && !undo_skip.empty())
      r["undo_skip_reason"] = mcp::JsonValue(undo_skip);
    util::add_scene_info_fields(r, util::edited_scene_info());
    scene_dirty_tracker::mark_scene_modified();
    return r;
  }
  std::string prop_str = it_prop->GetString();

  DottedTarget target = resolve_dotted_target(node, prop_str, path_str);
  if (!target.ok) {
    mcp::JsonValue expanded =
        try_expand_property_family(node, prop_str, *it_val, args, path_str);
    if (!expanded.IsNull())
      return expanded;
    return target.error;
  }
  godot::Object *target_owner = target.owner;
  std::string leaf_name = target.leaf;
  godot::Dictionary dict = target.info;

  std::string type_hint;
  auto *it_hint = args.Find("type_hint");
  if (it_hint && it_hint->IsString()) {
    type_hint = it_hint->GetString();
  }
  type_hint = util::infer_type_hint(dict, std::move(type_hint));

  godot::StringName prop_name(leaf_name.c_str());
  std::string value_node_path;
  godot::Variant value;
  bool converted_node_path = false;
  bool value_ready = false;
  SpriteFramesAnimationsConversion sprite_conversion =
      convert_spriteframes_animations(target_owner, leaf_name, prop_str,
                                      path_str, *it_val);
  if (sprite_conversion.has_error) {
    return sprite_conversion.error;
  }
  if (sprite_conversion.handled) {
    value = sprite_conversion.value;
    value_ready = true;
  }
  ArrayValueConversion array_conversion;
  DictionaryValueConversion dictionary_conversion;
  if (!value_ready) {
    array_conversion = convert_array_value(dict, prop_str, path_str, *it_val);
    if (array_conversion.has_error) {
      return array_conversion.error;
    }
    if (array_conversion.handled) {
      value = array_conversion.value;
      value_ready = true;
    }
  }
  if (!value_ready) {
    dictionary_conversion =
        convert_dictionary_value(dict, prop_str, path_str, *it_val);
    if (dictionary_conversion.has_error) {
      return dictionary_conversion.error;
    }
    if (dictionary_conversion.handled) {
      value = dictionary_conversion.value;
      value_ready = true;
    }
  }
  if (!value_ready) {
    NodePathValueConversion node_path_conversion =
        convert_node_path_value(dict, prop_str, path_str, *it_val);
    if (node_path_conversion.has_error) {
      return node_path_conversion.error;
    }
    if (node_path_conversion.converted) {
      value = node_path_conversion.value;
      converted_node_path = true;
      value_node_path = node_path_conversion.node_path;
      value_ready = true;
    }
  }
  bool resource_attached = false;
  bool inline_attached = false;
  std::string inline_type;
  if (!value_ready) {
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
      InlineResourceBuild inline_build =
          build_inline_resource_value(dict, *it_val, prop_str, path_str, 1);
      if (inline_build.has_error) {
        return inline_build.error;
      }
      if (inline_build.active) {
        value = inline_build.value;
        inline_attached = true;
        inline_type = inline_build.type;
      } else {
        value = VariantJson::deserialize_strict(*it_val, type_hint);
      }
    }
  }

  mcp::JsonValue blocked_error;
  if (resource_attached && memory_resource_assignment_blocked(
                               value, prop_str, path_str, blocked_error)) {
    return blocked_error;
  }

  godot::Variant old_val = target_owner->get(prop_name);
  target_owner->set(prop_name, value);

  godot::Variant new_val = target_owner->get(prop_name);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  if (resource_attached) {
    r["resource_attached"] = mcp::JsonValue(true);
  }
  if (inline_attached) {
    mcp::JsonValue inline_array(mcp::JsonValue::array_tag);
    mcp::JsonValue inline_entry(mcp::JsonValue::object_tag);
    inline_entry["property"] = mcp::JsonValue(prop_str);
    inline_entry["type"] = mcp::JsonValue(inline_type);
    inline_array.PushBack(std::move(inline_entry));
    r["inline_resources"] = std::move(inline_array);
  }
  if (converted_node_path) {
    r["converted_node_path"] = mcp::JsonValue(value_node_path);
  }

  std::string readback_detail;
  bool type_sensitive = is_type_sensitive_property(dict);
  util::ReadbackStatus readback =
      util::check_readback(value, old_val, new_val, readback_detail,
                           type_sensitive);
  if (readback == util::ReadbackStatus::REJECTED) {
    target_owner->set(prop_name, old_val);
    bool restored = target_owner->get(prop_name) == old_val;
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
  util::add_scene_info_fields(r, util::edited_scene_info());

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
      undo_redo->add_do_property(target_owner, prop_name, new_val);
      undo_redo->add_undo_property(target_owner, prop_name, old_val);
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
