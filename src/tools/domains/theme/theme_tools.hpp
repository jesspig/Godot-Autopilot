#ifndef GODOT_AUTOPILOT_THEME_TOOLS_HPP
#define GODOT_AUTOPILOT_THEME_TOOLS_HPP

#include "util/json_value.hpp"
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/theme/theme_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace theme_tools {

namespace {

const std::vector<ParamSpec> kGetThemeInfoParams = {
    {"path", "string", "Path of a saved .tres theme file to inspect", true},
};

const std::vector<ParamSpec> kUpdateThemeParams = {
    {"path", "string", "Destination or existing .tres theme path inside res:// (res:// only, user:// rejected), e.g. 'res://themes/main.tres'", true},
    {"base_type", "string", "Only with action=create: base type stored as resource metadata for reference, e.g. 'Control'; Godot themes carry no global base type", false},
    {"colors", "object", "Only with action=bundle: color items as object mapping theme_type to {varname: color} objects, e.g. {'Button': {'font_color': '#ff0000'}}; each color accepts a hex string like '#rrggbb' or '#rrggbbaa' or an object {r,g,b,a} with floats in 0..1", false},
    {"constants", "object", "Only with action=bundle: constant items as object mapping theme_type to {varname: integer} objects, e.g. {'Button': {'separation': 4}}", false},
    {"font_sizes", "object", "Only with action=bundle: font size items as object mapping theme_type to {varname: integer} objects, e.g. {'Label': {'font_size': 16}}", false},
    {"styleboxes", "object", "Only with action=bundle: stylebox items as object mapping theme_type to {varname: stylebox} objects, e.g. {'Button': {'normal': {'bg_color': '#202020'}}}; each stylebox holds optional bg_color and border_color (hex string or {r,g,b,a}), border_width {left,top,right,bottom}, corner_radius {top_left,top_right,bottom_right,bottom_left} and content_margin {left,top,right,bottom} — every field is optional, and border_width/content_margin also accept a single number applied to all sides", false},
};

const std::vector<ParamSpec> kUpdateControlLayoutParams = {
    {"theme_path", "string", "Only with action=apply: path of the .tres theme file to assign", false},
    {"control_path", "string", "Only with action=apply: path of the Control node in the edited scene receiving the theme; only the in-memory scene changes — call save_editor_scene afterwards to persist", false},
    {"path", "string", "Only with action=layout: path of the Control node in the edited scene; only the in-memory scene changes — call save_editor_scene to persist", false},
    {"preset", "integer", "Only with action=layout: Control LayoutPreset integer 0..15, same numbering as set_control_anchor_preset: top_left=0, top_right=1, bottom_left=2, bottom_right=3, center_left=4, center_top=5, center_right=6, center_bottom=7, center=8, left_wide=9, top_wide=10, right_wide=11, bottom_wide=12, vcenter_wide=13, hcenter_wide=14, full_rect=15; mutually exclusive with anchors", false},
    {"keep_offsets", "boolean", "Only with action=layout: keep current offsets when applying preset (boolean, default false); only used with preset", false},
    {"anchors", "object", "Only with action=layout: anchor overrides as object with optional left, top, right, bottom floats; mutually exclusive with preset; partial objects apply only the given sides", false},
    {"offsets", "object", "Only with action=layout: offset overrides as object with optional left, top, right, bottom numbers in pixels; applied after preset and anchors; partial objects apply only the given sides", false},
    {"size_flags_h", "integer", "Only with action=layout: horizontal size flags integer to assign", false},
    {"size_flags_v", "integer", "Only with action=layout: vertical size flags integer to assign", false},
    {"grow_h", "integer", "Only with action=layout: horizontal grow direction integer to assign", false},
    {"grow_v", "integer", "Only with action=layout: vertical grow direction integer to assign", false},
    {"min_size", "object", "Only with action=layout: custom minimum size as object with optional x, y numbers; partial objects keep the other axis", false},
};

const std::vector<std::string> kUpdateThemeActions = {"create", "bundle"};
const std::vector<std::string> kUpdateControlLayoutActions = {"apply", "layout"};

inline godot_autopilot::util::JsonValue theme_item_bundle_args(const godot_autopilot::util::JsonValue &args,
                                             const char *kind,
                                             const char *value_field) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  out["action"] = godot_autopilot::util::JsonValue("bundle");
  const godot_autopilot::util::JsonValue *path = args.Find("path");
  if (path != nullptr) {
    out["path"] = *path;
  }
  godot_autopilot::util::JsonValue type_map(godot_autopilot::util::JsonValue::object_tag);
  godot_autopilot::util::JsonValue item_map(godot_autopilot::util::JsonValue::object_tag);
  const godot_autopilot::util::JsonValue *varname = args.Find("varname");
  const godot_autopilot::util::JsonValue *value = args.Find(value_field);
  if (varname != nullptr && value != nullptr) {
    item_map[varname->GetString()] = *value;
  }
  const godot_autopilot::util::JsonValue *theme_type = args.Find("theme_type");
  if (theme_type != nullptr) {
    type_map[theme_type->GetString()] = std::move(item_map);
  }
  out[kind] = std::move(type_map);
  return out;
}

inline godot_autopilot::util::JsonValue validate_theme_item_args(const godot_autopilot::util::JsonValue &args,
                                               const char *value_field,
                                               bool require_int,
                                               bool require_object,
                                               const char *value_error) {
  const godot_autopilot::util::JsonValue *path = args.Find("path");
  const godot_autopilot::util::JsonValue *theme_type = args.Find("theme_type");
  const godot_autopilot::util::JsonValue *varname = args.Find("varname");
  if (path == nullptr || !path->IsString()) {
    godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
    error["error"] = godot_autopilot::util::JsonValue("missing required parameter: path");
    return error;
  }
  if (theme_type == nullptr || !theme_type->IsString()) {
    godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
    error["error"] = godot_autopilot::util::JsonValue("missing required parameter: theme_type");
    return error;
  }
  if (varname == nullptr || !varname->IsString()) {
    godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
    error["error"] = godot_autopilot::util::JsonValue("missing required parameter: varname");
    return error;
  }
  const godot_autopilot::util::JsonValue *value = args.Find(value_field);
  if (value == nullptr || (require_int && !value->IsInt()) ||
      (require_object && !value->IsObject())) {
    godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
    error["error"] = godot_autopilot::util::JsonValue(value_error);
    return error;
  }
  return godot_autopilot::util::JsonValue();
}

inline godot_autopilot::util::JsonValue project_theme_item_result(
    const godot_autopilot::util::JsonValue &result, const godot_autopilot::util::JsonValue &args) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  out["result"] = godot_autopilot::util::JsonValue("ok");
  const godot_autopilot::util::JsonValue *path = result.Find("path");
  if (path == nullptr) {
    path = args.Find("path");
  }
  if (path != nullptr) {
    out["path"] = *path;
  }
  const godot_autopilot::util::JsonValue *theme_type = args.Find("theme_type");
  if (theme_type != nullptr) {
    out["theme_type"] = *theme_type;
  }
  const godot_autopilot::util::JsonValue *varname = args.Find("varname");
  if (varname != nullptr) {
    out["varname"] = *varname;
  }
  const godot_autopilot::util::JsonValue *saved = result.Find("saved");
  out["saved"] = saved != nullptr ? *saved : godot_autopilot::util::JsonValue(true);
  const godot_autopilot::util::JsonValue *verified = result.Find("verified");
  out["verified"] =
      verified != nullptr ? *verified : godot_autopilot::util::JsonValue(false);
  return out;
}

inline godot_autopilot::util::JsonValue create_theme_args(const godot_autopilot::util::JsonValue &args) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  out["action"] = godot_autopilot::util::JsonValue("create");
  const godot_autopilot::util::JsonValue *path = args.Find("path");
  if (path != nullptr) {
    out["path"] = *path;
  }
  const godot_autopilot::util::JsonValue *base_type = args.Find("base_type");
  if (base_type != nullptr) {
    out["base_type"] = *base_type;
  }
  return out;
}

inline godot_autopilot::util::JsonValue theme_bundle_args(const godot_autopilot::util::JsonValue &args) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  out["action"] = godot_autopilot::util::JsonValue("bundle");
  const char *keys[] = {"path", "colors", "constants", "font_sizes",
                        "styleboxes"};
  for (const char *key : keys) {
    const godot_autopilot::util::JsonValue *value = args.Find(key);
    if (value != nullptr) {
      out[key] = *value;
    }
  }
  return out;
}

inline godot_autopilot::util::JsonValue apply_theme_args(const godot_autopilot::util::JsonValue &args) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  out["action"] = godot_autopilot::util::JsonValue("apply");
  const char *keys[] = {"theme_path", "control_path"};
  for (const char *key : keys) {
    const godot_autopilot::util::JsonValue *value = args.Find(key);
    if (value != nullptr) {
      out[key] = *value;
    }
  }
  return out;
}

inline godot_autopilot::util::JsonValue layout_args(const godot_autopilot::util::JsonValue &args) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  out["action"] = godot_autopilot::util::JsonValue("layout");
  const char *keys[] = {"path",     "preset",      "keep_offsets",
                        "anchors",  "offsets",     "size_flags_h",
                        "size_flags_v", "grow_h",   "grow_v",
                        "min_size"};
  for (const char *key : keys) {
    const godot_autopilot::util::JsonValue *value = args.Find(key);
    if (value != nullptr) {
      out[key] = *value;
    }
  }
  return out;
}

inline godot_autopilot::util::JsonValue control_anchor_args(const godot_autopilot::util::JsonValue &args) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  out["action"] = godot_autopilot::util::JsonValue("layout");
  const godot_autopilot::util::JsonValue *control_path = args.Find("control_path");
  if (control_path != nullptr) {
    out["path"] = *control_path;
  }
  const godot_autopilot::util::JsonValue *preset = args.Find("preset");
  if (preset != nullptr) {
    out["preset"] = *preset;
  }
  const godot_autopilot::util::JsonValue *keep_offsets = args.Find("keep_offsets");
  if (keep_offsets != nullptr) {
    out["keep_offsets"] = *keep_offsets;
  }
  return out;
}

inline godot_autopilot::util::JsonValue validate_control_anchor_args(
    const godot_autopilot::util::JsonValue &args) {
  const godot_autopilot::util::JsonValue *control_path = args.Find("control_path");
  const godot_autopilot::util::JsonValue *preset = args.Find("preset");
  if (control_path == nullptr || !control_path->IsString()) {
    godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
    error["error"] = godot_autopilot::util::JsonValue("missing required parameter: control_path");
    return error;
  }
  if (preset == nullptr || !preset->IsInt()) {
    godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
    error["error"] = godot_autopilot::util::JsonValue(
        "missing or invalid required parameter: preset (Control LayoutPreset "
        "integer)");
    return error;
  }
  return godot_autopilot::util::JsonValue();
}

inline godot_autopilot::util::JsonValue project_control_anchor_result(
    const godot_autopilot::util::JsonValue &result, const godot_autopilot::util::JsonValue &args) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  out["result"] = godot_autopilot::util::JsonValue("ok");
  const godot_autopilot::util::JsonValue *control_path = args.Find("control_path");
  if (control_path != nullptr) {
    out["control_path"] = *control_path;
  }
  const godot_autopilot::util::JsonValue *preset = result.Find("preset");
  if (preset == nullptr) {
    preset = args.Find("preset");
  }
  if (preset != nullptr) {
    out["preset"] = *preset;
  }
  const godot_autopilot::util::JsonValue *keep_offsets = result.Find("keep_offsets");
  if (keep_offsets == nullptr) {
    keep_offsets = args.Find("keep_offsets");
  }
  out["keep_offsets"] =
      keep_offsets != nullptr ? *keep_offsets : godot_autopilot::util::JsonValue(false);
  const godot_autopilot::util::JsonValue *note = result.Find("note");
  if (note != nullptr) {
    out["note"] = *note;
  }
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(3);
  v.push_back(make_spec_tool(ToolSpec{
      "get_theme_info",
      "Inspect a saved .tres theme: lists every theme type it defines and the item names registered under each type, grouped into colors, constants, font_sizes and styleboxes. Use it before update_theme action=bundle to discover valid theme_type/varname pairs. Requires path. Returns path, type_count and the types array.",
      "Theme", {"theme", "inspect"}, SideEffect::None, tool_flags::kNone,
      kGetThemeInfoParams, theme_ops::handle_get_theme_info}));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "update_theme",
      "Create or update a Theme resource on disk (WritesFile, no confirmation). action selects the operation: 'create' writes a new empty Theme at path (res:// .tres only; user:// rejected; missing directories are created and an existing file is never overwritten) and optionally stores base_type as resource metadata; 'bundle' loads the theme at path, applies every requested item from colors, constants, font_sizes and styleboxes (mapping theme_type to {varname: value}), then saves back once with a reload verification. Colors accept hex strings like \"#rrggbb\" or \"#rrggbbaa\" or objects {r,g,b,a} with floats in 0..1; constants and font_sizes take integers; styleboxes take StyleBoxFlat definitions with the same fields as the former set_theme_stylebox_flat. Use get_theme_info to discover valid type and name pairs. Example: {\"action\": \"create\", \"path\": \"res://themes/main.tres\"} or {\"action\": \"bundle\", \"path\": \"res://themes/main.tres\", \"colors\": {\"Button\": {\"font_color\": \"#ff0000\"}}}.",
      "Theme", {"theme", "create", "bundle", "color", "constant", "font_size", "stylebox"},
      SideEffect::WritesFile, tool_flags::kMutating,
      kUpdateThemeParams, theme_ops::handle_update_theme,
      schema::build_action_schema(
          kUpdateThemeActions, kUpdateThemeParams,
          "create: write a new empty theme resource at path; bundle: apply colors, constants, font_sizes and styleboxes to an existing theme and save once"),
      {},
      {"update_theme action=create path=res://themes/main.tres",
       "update_theme action=bundle path=res://themes/main.tres colors={\"Button\":{\"font_color\":\"#ff0000\"}}"}},
      {
          ToolAlias{
              "create_theme_resource", "update_theme",
              create_theme_args, {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_theme_item_bundle", "update_theme",
              theme_bundle_args, {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_theme_color", "update_theme",
              [](const godot_autopilot::util::JsonValue &args) {
                return theme_item_bundle_args(args, "colors", "color");
              },
              {}, "0.4.0", "0.6.0",
              [](const godot_autopilot::util::JsonValue &args) {
                return validate_theme_item_args(
                    args, "color", false, false,
                    "missing required parameter: color");
              },
              project_theme_item_result},
          ToolAlias{
              "set_theme_constant", "update_theme",
              [](const godot_autopilot::util::JsonValue &args) {
                return theme_item_bundle_args(args, "constants", "constant");
              },
              {}, "0.4.0", "0.6.0",
              [](const godot_autopilot::util::JsonValue &args) {
                return validate_theme_item_args(
                    args, "constant", true, false,
                    "missing or invalid required parameter: constant (integer)");
              },
              project_theme_item_result},
          ToolAlias{
              "set_theme_font_size", "update_theme",
              [](const godot_autopilot::util::JsonValue &args) {
                return theme_item_bundle_args(args, "font_sizes", "font_size");
              },
              {}, "0.4.0", "0.6.0",
              [](const godot_autopilot::util::JsonValue &args) {
                return validate_theme_item_args(
                    args, "font_size", true, false,
                    "missing or invalid required parameter: font_size (integer)");
              },
              project_theme_item_result},
          ToolAlias{
              "set_theme_stylebox_flat", "update_theme",
              [](const godot_autopilot::util::JsonValue &args) {
                return theme_item_bundle_args(args, "styleboxes", "stylebox");
              },
              {}, "0.4.0", "0.6.0",
              [](const godot_autopilot::util::JsonValue &args) {
                return validate_theme_item_args(
                    args, "stylebox", false, true,
                    "missing or invalid required parameter: stylebox (object "
                    "with optional bg_color, border_color, border_width, "
                    "corner_radius, content_margin)");
              },
              project_theme_item_result},
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "update_control_layout",
      "Change a Control node in the edited scene in one undo step (WritesScene, no confirmation). action selects the operation: 'apply' requires theme_path and control_path and assigns that .tres theme to the Control — only the in-memory scene changes, call save_editor_scene afterwards; 'layout' requires path (the Control node) and applies an optional preset (LayoutPreset integer 0..15) first, then anchors, offsets, size flags, grow directions and custom minimum size in fixed order — preset and anchors are mutually exclusive, and only the in-memory scene changes, call save_editor_scene to persist. Example: {\"action\": \"apply\", \"theme_path\": \"res://themes/main.tres\", \"control_path\": \"UI/Panel\"} or {\"action\": \"layout\", \"path\": \"UI/Panel\", \"preset\": 8}.",
      "Theme", {"control", "layout", "anchor", "theme", "apply"},
      SideEffect::WritesScene, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kUpdateControlLayoutParams, theme_ops::handle_update_control_layout,
      schema::build_action_schema(
          kUpdateControlLayoutActions, kUpdateControlLayoutParams,
          "apply: assign a .tres theme to a Control node; layout: apply preset, anchors, offsets, size flags, grow and minimum size to a Control node"),
      {},
      {"update_control_layout action=apply theme_path=res://themes/main.tres control_path=UI/Panel",
       "update_control_layout action=layout path=UI/Panel preset=8"}},
      {
          ToolAlias{
              "apply_theme_to_control", "update_control_layout",
              apply_theme_args, {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_control_layout", "update_control_layout",
              layout_args, {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_control_anchor_preset", "update_control_layout",
              control_anchor_args, {}, "0.4.0", "0.6.0",
              validate_control_anchor_args, project_control_anchor_result},
      })));
  return v;
}

} // namespace theme_tools
} // namespace godot_autopilot

#endif
