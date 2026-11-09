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


namespace {

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

mcp::JsonValue handle_set_transform(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  auto *it_space = args.Find("space");
  auto *it_position = args.Find("position");
  auto *it_rotation = args.Find("rotation");
  auto *it_scale = args.Find("scale");
  if (!it_path || !it_path->IsString()) {
    return util::error_json("missing required parameter: path");
  }
  if (args.IsObject()) {
    for (const auto &entry : args.GetObject()) {
      if (entry.first != "path" && entry.first != "space" &&
          entry.first != "position" && entry.first != "rotation" &&
          entry.first != "scale") {
        return util::error_json("unknown parameter for set_node_transform: " +
                                entry.first);
      }
    }
  }
  std::string space = "local";
  if (it_space) {
    if (!it_space->IsString() || (it_space->GetString() != "local" &&
                                  it_space->GetString() != "global")) {
      return util::error_json("space must be \"local\" or \"global\"");
    }
    space = it_space->GetString();
  }
  bool has_pos = it_position != nullptr;
  bool has_rot = it_rotation != nullptr;
  bool has_scale = it_scale != nullptr;
  if (!has_pos && !has_rot && !has_scale) {
    return util::error_json("set_node_transform requires at least one of: "
                            "position, rotation, scale");
  }
  std::string path_str = it_path->GetString();
  std::string hint;
  godot::Node *node =
      util::resolve_scene_node(path_str, find_edited_scene_root(), &hint);
  if (!node) {
    std::string err = "node not found: " + path_str + " — " + hint;
    const std::string memory_prefix = "memory://";
    if (path_str.compare(0, memory_prefix.size(), memory_prefix) == 0) {
      err += " memory:// is a resource namespace, not a node path; to reference a memory resource pass it as the value (e.g. {\"resource\": \"memory://name\"}) or create the resource inline with code_execute";
    }
    return util::error_json(err);
  }
  godot::Node3D *node_3d = godot::Object::cast_to<godot::Node3D>(node);
  godot::CanvasItem *item = godot::Object::cast_to<godot::CanvasItem>(node);
  bool is_3d = node_3d != nullptr;
  if (!is_3d && item == nullptr) {
    return util::error_detail(
        "node " + path_str + " of type " + util::to_std(node->get_class()) +
            " has no 2D/3D transform",
        path_str, "a Node2D, Control or Node3D inside the edited scene",
        "use property_get_list to see available properties, or property_set "
        "for other properties");
  }
  auto num_value = [](const mcp::JsonValue *v) -> double {
    return v->IsDouble() ? v->GetDouble() : static_cast<double>(v->GetInt());
  };
  auto parse_vector = [&](const mcp::JsonValue *raw, const char *field,
                          bool need_z,
                          std::vector<double> &out) -> mcp::JsonValue {
    mcp::JsonValue ok;
    std::string want =
        need_z ? "{\"x\": number, \"y\": number, \"z\": number}"
               : "{\"x\": number, \"y\": number}";
    if (!raw->IsObject()) {
      return util::error_detail(std::string("invalid '") + field +
                                    "': expected an object, e.g. " + want,
                                path_str, want,
                                std::string("pass an object with numeric "
                                            "fields, e.g. ") +
                                    want);
    }
    for (const auto &entry : raw->GetObject()) {
      bool known = entry.first == "x" || entry.first == "y" ||
                   (need_z && entry.first == "z");
      if (!known) {
        return util::error_detail(std::string("unknown field '") +
                                      entry.first + "' in '" + field +
                                      "': expected " + want,
                                  path_str, want,
                                  "remove the unknown field and retry");
      }
    }
    const char *axes[3] = {"x", "y", "z"};
    size_t count = need_z ? 3 : 2;
    for (size_t i = 0; i < count; i++) {
      const mcp::JsonValue *f = raw->Find(axes[i]);
      if (!f || !f->IsNumber()) {
        return util::error_detail(std::string("invalid '") + field + "': '" +
                                      axes[i] + "' must be a number, e.g. " +
                                      want,
                                  path_str, want,
                                  std::string("pass numeric values, e.g. ") +
                                      want);
      }
      out.push_back(num_value(f));
    }
    return ok;
  };
  std::vector<double> pos_vals;
  std::vector<double> scale_vals;
  std::vector<double> rot_vals;
  if (has_pos) {
    mcp::JsonValue err = parse_vector(it_position, "position", is_3d, pos_vals);
    if (!err.IsNull()) {
      return err;
    }
  }
  if (has_scale) {
    mcp::JsonValue err = parse_vector(it_scale, "scale", is_3d, scale_vals);
    if (!err.IsNull()) {
      return err;
    }
  }
  if (has_rot) {
    if (it_rotation->IsNumber()) {
      if (is_3d) {
        return util::error_detail(
            "invalid 'rotation' for Node3D: expected an object of euler "
            "angles in radians, e.g. {\"x\": 0, \"y\": 1.5707963, \"z\": 0}",
            path_str, "an object {\"x\": number, \"y\": number, \"z\": number}",
            "pass euler angles in radians for each axis");
      }
      rot_vals.push_back(num_value(it_rotation));
    } else if (it_rotation->IsObject()) {
      if (it_rotation->Find("degrees")) {
        return util::error_detail(
            "invalid 'rotation': rotation is always radians — convert degrees "
            "with radians = degrees * 3.1415926535 / 180 (e.g. 90 degrees = "
            "1.5707963 radians)",
            path_str,
            "a radians number or {\"radians\": number} (2D), or euler "
            "{\"x\", \"y\", \"z\"} in radians (3D)",
            "convert the value to radians and retry");
      }
      if (!is_3d) {
        const mcp::JsonValue *rad = it_rotation->Find("radians");
        if (!rad || !rad->IsNumber() || it_rotation->GetObject().size() != 1) {
          return util::error_detail(
              "invalid 'rotation': expected a radians number or {\"radians\": "
              "number}",
              path_str, "a radians number or {\"radians\": number}",
              "pass a number in radians, e.g. 1.5707963");
        }
        rot_vals.push_back(num_value(rad));
      } else {
        mcp::JsonValue err =
            parse_vector(it_rotation, "rotation", true, rot_vals);
        if (!err.IsNull()) {
          return err;
        }
      }
    } else {
      return util::error_detail(
          "invalid 'rotation': expected a radians number or an object",
          path_str,
          "a radians number, {\"radians\": number} (2D), or euler {\"x\", "
          "\"y\", \"z\"} in radians (3D)",
          "pass the rotation in radians and retry");
    }
  }
  auto vec2_json = [](double x, double y) {
    mcp::JsonValue o(mcp::JsonValue::object_tag);
    o["x"] = mcp::JsonValue(x);
    o["y"] = mcp::JsonValue(y);
    return o;
  };
  auto vec3_json = [](double x, double y, double z) {
    mcp::JsonValue o(mcp::JsonValue::object_tag);
    o["x"] = mcp::JsonValue(x);
    o["y"] = mcp::JsonValue(y);
    o["z"] = mcp::JsonValue(z);
    return o;
  };
  mcp::JsonValue batch_props(mcp::JsonValue::object_tag);
  if (space == "local") {
    if (has_pos) {
      batch_props["position"] =
          is_3d ? vec3_json(pos_vals[0], pos_vals[1], pos_vals[2])
                : vec2_json(pos_vals[0], pos_vals[1]);
    }
    if (has_rot) {
      batch_props["rotation"] =
          is_3d ? vec3_json(rot_vals[0], rot_vals[1], rot_vals[2])
                : mcp::JsonValue(rot_vals[0]);
    }
    if (has_scale) {
      batch_props["scale"] =
          is_3d ? vec3_json(scale_vals[0], scale_vals[1], scale_vals[2])
                : vec2_json(scale_vals[0], scale_vals[1]);
    }
  } else if (!is_3d) {
    godot::Transform2D parent_global;
    if (godot::Node *parent = node->get_parent()) {
      if (godot::CanvasItem *pitem =
              godot::Object::cast_to<godot::CanvasItem>(parent)) {
        parent_global = pitem->get_global_transform();
      }
    }
    godot::Transform2D want = item->get_global_transform();
    if (has_pos) {
      want.set_origin(godot::Vector2(pos_vals[0], pos_vals[1]));
    }
    if (has_rot) {
      want.set_rotation(rot_vals[0]);
    }
    if (has_scale) {
      want.set_scale(godot::Vector2(scale_vals[0], scale_vals[1]));
    }
    godot::Transform2D local = parent_global.affine_inverse() * want;
    if (has_pos) {
      godot::Vector2 p = local.get_origin();
      batch_props["position"] =
          vec2_json(static_cast<double>(p.x), static_cast<double>(p.y));
    }
    if (has_rot) {
      batch_props["rotation"] =
          mcp::JsonValue(static_cast<double>(local.get_rotation()));
    }
    if (has_scale) {
      godot::Vector2 s = local.get_scale();
      batch_props["scale"] =
          vec2_json(static_cast<double>(s.x), static_cast<double>(s.y));
    }
  } else {
    godot::Transform3D parent_global;
    if (godot::Node *parent = node->get_parent()) {
      if (godot::Node3D *p3d =
              godot::Object::cast_to<godot::Node3D>(parent)) {
        parent_global = p3d->get_global_transform();
      }
    }
    godot::Transform3D want = node_3d->get_global_transform();
    if (has_rot || has_scale) {
      godot::Vector3 euler =
          has_rot ? godot::Vector3(rot_vals[0], rot_vals[1], rot_vals[2])
                  : want.get_basis().get_euler();
      godot::Vector3 scl =
          has_scale
              ? godot::Vector3(scale_vals[0], scale_vals[1], scale_vals[2])
              : want.get_basis().get_scale();
      want.set_basis(godot::Basis::from_euler(euler).scaled(scl));
    }
    if (has_pos) {
      want.set_origin(
          godot::Vector3(pos_vals[0], pos_vals[1], pos_vals[2]));
    }
    godot::Transform3D local = parent_global.affine_inverse() * want;
    if (has_pos) {
      godot::Vector3 p = local.get_origin();
      batch_props["position"] = vec3_json(static_cast<double>(p.x),
                                          static_cast<double>(p.y),
                                          static_cast<double>(p.z));
    }
    if (has_rot) {
      godot::Vector3 e = local.get_basis().get_euler();
      batch_props["rotation"] = vec3_json(static_cast<double>(e.x),
                                          static_cast<double>(e.y),
                                          static_cast<double>(e.z));
    }
    if (has_scale) {
      godot::Vector3 s = local.get_basis().get_scale();
      batch_props["scale"] = vec3_json(static_cast<double>(s.x),
                                       static_cast<double>(s.y),
                                       static_cast<double>(s.z));
    }
  }
  mcp::JsonValue batch_args(mcp::JsonValue::object_tag);
  batch_args["path"] = mcp::JsonValue(path_str);
  batch_args["properties"] = std::move(batch_props);
  batch_args["atomic"] = mcp::JsonValue(true);
  mcp::JsonValue result = handle_set_batch(batch_args);
  if (result.IsObject() && !result.Find("error")) {
    result["space"] = mcp::JsonValue(space);
  }
  return result;
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


} // namespace property_ops
} // namespace godot_autopilot
