#ifndef GODOT_AUTOPILOT_THEME_TOOLS_HPP
#define GODOT_AUTOPILOT_THEME_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/theme/theme_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace theme_tools {

namespace {

const std::vector<ParamSpec> kCreateThemeResourceParams = {
    {"path", "string", "Destination .tres path inside res:// (res:// only, user:// rejected), e.g. 'res://themes/main.tres'; missing directories are created and an existing file is never overwritten", true},
    {"base_type", "string", "Base type stored as resource metadata for reference, e.g. 'Control'; Godot themes carry no global base type", false},
};

const std::vector<ParamSpec> kGetThemeInfoParams = {
    {"path", "string", "Path of a saved .tres theme file to inspect", true},
};

const std::vector<ParamSpec> kApplyThemeToControlParams = {
    {"theme_path", "string", "Path of the .tres theme file to assign", true},
    {"control_path", "string", "Path of the Control node in the edited scene receiving the theme; only the in-memory scene changes — call save_editor_scene afterwards to persist", true},
};

const std::vector<ParamSpec> kSetControlLayoutParams = {
    {"path", "string", "Path of the Control node in the edited scene; only the in-memory scene changes — call save_editor_scene to persist", true},
    {"preset", "integer", "Control LayoutPreset integer 0..15, same numbering as set_control_anchor_preset: top_left=0, top_right=1, bottom_left=2, bottom_right=3, center_left=4, center_top=5, center_right=6, center_bottom=7, center=8, left_wide=9, top_wide=10, right_wide=11, bottom_wide=12, vcenter_wide=13, hcenter_wide=14, full_rect=15; mutually exclusive with anchors", false},
    {"keep_offsets", "boolean", "Keep current offsets when applying preset (boolean, default false); only used with preset", false},
    {"anchors", "object", "Anchor overrides as object with optional left, top, right, bottom floats; mutually exclusive with preset; partial objects apply only the given sides", false},
    {"offsets", "object", "Offset overrides as object with optional left, top, right, bottom numbers in pixels; applied after preset and anchors; partial objects apply only the given sides", false},
    {"size_flags_h", "integer", "Horizontal size flags integer to assign", false},
    {"size_flags_v", "integer", "Vertical size flags integer to assign", false},
    {"grow_h", "integer", "Horizontal grow direction integer to assign", false},
    {"grow_v", "integer", "Vertical grow direction integer to assign", false},
    {"min_size", "object", "Custom minimum size as object with optional x, y numbers; partial objects keep the other axis", false},
};

const std::vector<ParamSpec> kSetThemeItemBundleParams = {
    {"path", "string", "Path of a saved .tres theme file created with create_theme_resource, e.g. 'res://themes/main.tres'", true},
    {"colors", "object", "Color items as object mapping theme_type to {varname: color} objects, e.g. {'Button': {'font_color': '#ff0000'}}; each color accepts a hex string like '#rrggbb' or '#rrggbbaa' or an object {r,g,b,a} with floats in 0..1", false},
    {"constants", "object", "Constant items as object mapping theme_type to {varname: integer} objects, e.g. {'Button': {'separation': 4}}", false},
    {"font_sizes", "object", "Font size items as object mapping theme_type to {varname: integer} objects, e.g. {'Label': {'font_size': 16}}", false},
    {"styleboxes", "object", "Stylebox items as object mapping theme_type to {varname: stylebox} objects, e.g. {'Button': {'normal': {'bg_color': '#202020'}}}; each stylebox holds optional bg_color and border_color (hex string or {r,g,b,a}), border_width {left,top,right,bottom}, corner_radius {top_left,top_right,bottom_right,bottom_left} and content_margin {left,top,right,bottom} — every field is optional, and border_width/content_margin also accept a single number applied to all sides", false},
};

inline mcp::JsonValue theme_item_bundle_args(const mcp::JsonValue &args,
                                             const char *kind,
                                             const char *value_field) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *path = args.Find("path");
  if (path != nullptr) {
    out["path"] = *path;
  }
  mcp::JsonValue type_map(mcp::JsonValue::object_tag);
  mcp::JsonValue item_map(mcp::JsonValue::object_tag);
  const mcp::JsonValue *varname = args.Find("varname");
  const mcp::JsonValue *value = args.Find(value_field);
  if (varname != nullptr && value != nullptr) {
    item_map[varname->GetString()] = *value;
  }
  const mcp::JsonValue *theme_type = args.Find("theme_type");
  if (theme_type != nullptr) {
    type_map[theme_type->GetString()] = std::move(item_map);
  }
  out[kind] = std::move(type_map);
  return out;
}

inline mcp::JsonValue validate_theme_item_args(const mcp::JsonValue &args,
                                               const char *value_field,
                                               bool require_int,
                                               bool require_object,
                                               const char *value_error) {
  const mcp::JsonValue *path = args.Find("path");
  const mcp::JsonValue *theme_type = args.Find("theme_type");
  const mcp::JsonValue *varname = args.Find("varname");
  if (path == nullptr || !path->IsString()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: path");
    return error;
  }
  if (theme_type == nullptr || !theme_type->IsString()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: theme_type");
    return error;
  }
  if (varname == nullptr || !varname->IsString()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: varname");
    return error;
  }
  const mcp::JsonValue *value = args.Find(value_field);
  if (value == nullptr || (require_int && !value->IsInt()) ||
      (require_object && !value->IsObject())) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue(value_error);
    return error;
  }
  return mcp::JsonValue();
}

inline mcp::JsonValue project_theme_item_result(
    const mcp::JsonValue &result, const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["result"] = mcp::JsonValue("ok");
  const mcp::JsonValue *path = result.Find("path");
  if (path == nullptr) {
    path = args.Find("path");
  }
  if (path != nullptr) {
    out["path"] = *path;
  }
  const mcp::JsonValue *theme_type = args.Find("theme_type");
  if (theme_type != nullptr) {
    out["theme_type"] = *theme_type;
  }
  const mcp::JsonValue *varname = args.Find("varname");
  if (varname != nullptr) {
    out["varname"] = *varname;
  }
  const mcp::JsonValue *saved = result.Find("saved");
  out["saved"] = saved != nullptr ? *saved : mcp::JsonValue(true);
  const mcp::JsonValue *verified = result.Find("verified");
  out["verified"] =
      verified != nullptr ? *verified : mcp::JsonValue(false);
  return out;
}

inline mcp::JsonValue control_anchor_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *control_path = args.Find("control_path");
  if (control_path != nullptr) {
    out["path"] = *control_path;
  }
  const mcp::JsonValue *preset = args.Find("preset");
  if (preset != nullptr) {
    out["preset"] = *preset;
  }
  const mcp::JsonValue *keep_offsets = args.Find("keep_offsets");
  if (keep_offsets != nullptr) {
    out["keep_offsets"] = *keep_offsets;
  }
  return out;
}

inline mcp::JsonValue validate_control_anchor_args(
    const mcp::JsonValue &args) {
  const mcp::JsonValue *control_path = args.Find("control_path");
  const mcp::JsonValue *preset = args.Find("preset");
  if (control_path == nullptr || !control_path->IsString()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: control_path");
    return error;
  }
  if (preset == nullptr || !preset->IsInt()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue(
        "missing or invalid required parameter: preset (Control LayoutPreset "
        "integer)");
    return error;
  }
  return mcp::JsonValue();
}

inline mcp::JsonValue project_control_anchor_result(
    const mcp::JsonValue &result, const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["result"] = mcp::JsonValue("ok");
  const mcp::JsonValue *control_path = args.Find("control_path");
  if (control_path != nullptr) {
    out["control_path"] = *control_path;
  }
  const mcp::JsonValue *preset = result.Find("preset");
  if (preset == nullptr) {
    preset = args.Find("preset");
  }
  if (preset != nullptr) {
    out["preset"] = *preset;
  }
  const mcp::JsonValue *keep_offsets = result.Find("keep_offsets");
  if (keep_offsets == nullptr) {
    keep_offsets = args.Find("keep_offsets");
  }
  out["keep_offsets"] =
      keep_offsets != nullptr ? *keep_offsets : mcp::JsonValue(false);
  const mcp::JsonValue *note = result.Find("note");
  if (note != nullptr) {
    out["note"] = *note;
  }
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(5);
  v.push_back(make_spec_tool(ToolSpec{
      "create_theme_resource",
      "Create an empty Theme resource on disk at path (a res:// .tres path — res:// only; user:// rejected). Missing directories are created and the editor file system is refreshed after saving; an existing file is never overwritten. Optional base_type (e.g. Control) is stored as resource metadata for reference — Godot themes carry no global base type, so actual items are added with set_theme_color, set_theme_constant, set_theme_font_size and set_theme_stylebox_flat. Returns ok with the saved path.",
      "Theme", {"theme", "create"}, SideEffect::WritesFile, tool_flags::kMutating,
      kCreateThemeResourceParams, theme_ops::handle_create_theme_resource}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_theme_info",
      "Inspect a saved .tres theme: lists every theme type it defines and the item names registered under each type, grouped into colors, constants, font_sizes and styleboxes. Use it before the set_theme_* tools to discover valid theme_type/varname pairs. Requires path. Returns path, type_count and the types array.",
      "Theme", {"theme", "inspect"}, SideEffect::None, tool_flags::kNone,
      kGetThemeInfoParams, theme_ops::handle_get_theme_info}));
  v.push_back(make_spec_tool(ToolSpec{
      "apply_theme_to_control",
      "Assign a .tres theme file to a Control node in the edited scene. Requires theme_path (the .tres to load) and control_path (node path inside the current scene). Only the in-memory scene changes — call save_editor_scene afterwards to persist the assignment. Returns ok with control_path and theme_path.",
      "Theme", {"theme", "apply", "control"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget,
      kApplyThemeToControlParams, theme_ops::handle_apply_theme_to_control}));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "set_control_layout",
      "Set combined Control layout in the edited scene in one undo step. Optional preset applies a LayoutPreset first, then anchors, offsets, size flags, grow directions and custom minimum size are applied in fixed order. preset and anchors are mutually exclusive. Only the in-memory scene changes — call save_editor_scene to persist. Returns ok with the final anchors, offsets, size flags, grow and minimum size.",
      "Theme", {"control", "layout"}, SideEffect::WritesScene, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kSetControlLayoutParams, theme_ops::handle_set_control_layout},
      {
          ToolAlias{
              "set_control_anchor_preset", "set_control_layout",
              control_anchor_args, {}, "0.4.0", "0.6.0",
              validate_control_anchor_args, project_control_anchor_result},
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "set_theme_item_bundle",
      "Set many theme items on a saved .tres theme in one save. Requires path plus at least one of colors, constants, font_sizes or styleboxes, each mapping theme_type to {varname: value} objects. Colors accept hex strings like \"#rrggbb\" or \"#rrggbbaa\" or objects {r,g,b,a} with floats in 0..1; constants and font_sizes take integers; styleboxes take StyleBoxFlat definitions with the same fields as the former set_theme_stylebox_flat. The theme is loaded, all items are modified in memory and saved back to disk once with a reload verification. Use get_theme_info to discover valid type and name pairs. Returns ok with per-kind applied counts plus saved and verified.",
      "Theme", {"theme", "bundle"}, SideEffect::WritesFile, tool_flags::kMutating,
      kSetThemeItemBundleParams, theme_ops::handle_set_theme_item_bundle},
      {
          ToolAlias{
              "set_theme_color", "set_theme_item_bundle",
              [](const mcp::JsonValue &args) {
                return theme_item_bundle_args(args, "colors", "color");
              },
              {}, "0.4.0", "0.6.0",
              [](const mcp::JsonValue &args) {
                return validate_theme_item_args(
                    args, "color", false, false,
                    "missing required parameter: color");
              },
              project_theme_item_result},
          ToolAlias{
              "set_theme_constant", "set_theme_item_bundle",
              [](const mcp::JsonValue &args) {
                return theme_item_bundle_args(args, "constants", "constant");
              },
              {}, "0.4.0", "0.6.0",
              [](const mcp::JsonValue &args) {
                return validate_theme_item_args(
                    args, "constant", true, false,
                    "missing or invalid required parameter: constant (integer)");
              },
              project_theme_item_result},
          ToolAlias{
              "set_theme_font_size", "set_theme_item_bundle",
              [](const mcp::JsonValue &args) {
                return theme_item_bundle_args(args, "font_sizes", "font_size");
              },
              {}, "0.4.0", "0.6.0",
              [](const mcp::JsonValue &args) {
                return validate_theme_item_args(
                    args, "font_size", true, false,
                    "missing or invalid required parameter: font_size (integer)");
              },
              project_theme_item_result},
          ToolAlias{
              "set_theme_stylebox_flat", "set_theme_item_bundle",
              [](const mcp::JsonValue &args) {
                return theme_item_bundle_args(args, "styleboxes", "stylebox");
              },
              {}, "0.4.0", "0.6.0",
              [](const mcp::JsonValue &args) {
                return validate_theme_item_args(
                    args, "stylebox", false, true,
                    "missing or invalid required parameter: stylebox (object "
                    "with optional bg_color, border_color, border_width, "
                    "corner_radius, content_margin)");
              },
              project_theme_item_result},
      })));
  return v;
}

} // namespace theme_tools
} // namespace godot_autopilot

#endif
