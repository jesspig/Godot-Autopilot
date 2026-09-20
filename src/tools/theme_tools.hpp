#ifndef GODOT_AUTOPILOT_THEME_TOOLS_HPP
#define GODOT_AUTOPILOT_THEME_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/theme_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace theme_tools {

namespace {

const std::vector<ParamSpec> kCreateThemeResourceParams = {
    {"path", "string", "Destination .tres path inside res:// (res:// only, user:// rejected), e.g. 'res://themes/main.tres'; missing directories are created and an existing file is never overwritten", true},
    {"base_type", "string", "Base type stored as resource metadata for reference, e.g. 'Control'; Godot themes carry no global base type", false},
};

const std::vector<ParamSpec> kSetThemeColorParams = {
    {"path", "string", "Path of a saved .tres theme file created with create_theme_resource", true},
    {"theme_type", "string", "Widget class the color item belongs to, e.g. 'Button' or 'Label'", true},
    {"varname", "string", "Color item name, e.g. 'font_color'", true},
    {"color", "object", "Color value: hex string like '#rrggbb' or '#rrggbbaa', or object {r,g,b,a} with floats in 0..1", true},
};

const std::vector<ParamSpec> kSetThemeConstantParams = {
    {"path", "string", "Path of a saved .tres theme file created with create_theme_resource", true},
    {"theme_type", "string", "Widget class the constant item belongs to, e.g. 'Button'", true},
    {"varname", "string", "Constant item name, e.g. 'separation' or 'outline_size'", true},
    {"constant", "integer", "Integer constant value to set", true},
};

const std::vector<ParamSpec> kSetThemeFontSizeParams = {
    {"path", "string", "Path of a saved .tres theme file created with create_theme_resource", true},
    {"theme_type", "string", "Widget class the font size item belongs to, e.g. 'Label'", true},
    {"varname", "string", "Font size item name, e.g. 'font_size'", true},
    {"font_size", "integer", "Integer font size value to set", true},
};

const std::vector<ParamSpec> kSetThemeStyleboxFlatParams = {
    {"path", "string", "Path of a saved .tres theme file created with create_theme_resource", true},
    {"theme_type", "string", "Widget class the stylebox item belongs to, e.g. 'Button'", true},
    {"varname", "string", "Stylebox item name, e.g. 'normal' or 'hover'", true},
    {"stylebox", "object", "StyleBoxFlat definition: optional bg_color/border_color (hex string or {r,g,b,a}), border_width {left,top,right,bottom}, corner_radius {top_left,top_right,bottom_right,bottom_left}, content_margin {left,top,right,bottom}; border_width/content_margin also accept a single number applied to all sides", true},
};

const std::vector<ParamSpec> kGetThemeInfoParams = {
    {"path", "string", "Path of a saved .tres theme file to inspect", true},
};

const std::vector<ParamSpec> kApplyThemeToControlParams = {
    {"theme_path", "string", "Path of the .tres theme file to assign", true},
    {"control_path", "string", "Path of the Control node in the edited scene receiving the theme; only the in-memory scene changes — call save_editor_scene afterwards to persist", true},
};

const std::vector<ParamSpec> kSetControlAnchorPresetParams = {
    {"control_path", "string", "Path of the Control node in the edited scene; only the in-memory scene changes — call save_editor_scene to persist", true},
    {"preset", "integer", "Control LayoutPreset integer 0..15: top_left=0, top_right=1, bottom_left=2, bottom_right=3, center_left=4..center=8, left_wide=9, top_wide=10, right_wide=11, bottom_wide=12, vcenter_wide=13, hcenter_wide=14, full_rect=15", true},
    {"keep_offsets", "boolean", "Keep the node's current offsets instead of recomputing them for the new anchors (boolean, default: false)", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(8);
  v.push_back(make_spec_tool(ToolSpec{
      "create_theme_resource",
      "Create an empty Theme resource on disk at path (a res:// .tres path — res:// only; user:// rejected). Missing directories are created and the editor file system is refreshed after saving; an existing file is never overwritten. Optional base_type (e.g. Control) is stored as resource metadata for reference — Godot themes carry no global base type, so actual items are added with set_theme_color, set_theme_constant, set_theme_font_size and set_theme_stylebox_flat. Returns ok with the saved path.",
      "Theme", {"theme", "create"}, SideEffect::WritesFile, tool_flags::kMutating,
      kCreateThemeResourceParams, theme_ops::handle_create_theme_resource}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_theme_color",
      "Set a color item on a saved .tres theme. Requires path, theme_type (the widget class the item belongs to, e.g. Button or Label) and varname (the item name, e.g. font_color); color accepts a hex string like \"#rrggbb\" or \"#rrggbbaa\" or an object {r,g,b,a} with floats in 0..1. The theme is loaded, modified and saved back to disk. Use get_theme_info to discover valid type/name pairs. Returns ok with saved:true.",
      "Theme", {"theme", "color"}, SideEffect::WritesFile, tool_flags::kMutating,
      kSetThemeColorParams, theme_ops::handle_set_theme_color}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_theme_constant",
      "Set an integer constant item (e.g. separation, outline_size) on a saved .tres theme. Requires path, theme_type and varname plus the integer constant. The theme is loaded, modified and saved back to disk. Returns ok with saved:true.",
      "Theme", {"theme", "constant"}, SideEffect::WritesFile, tool_flags::kMutating,
      kSetThemeConstantParams, theme_ops::handle_set_theme_constant}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_theme_font_size",
      "Set a font size item on a saved .tres theme. Requires path, theme_type and varname plus the integer font_size. The theme is loaded, modified and saved back to disk; use set_theme_color or set_theme_stylebox_flat for other item kinds. Returns ok with saved:true.",
      "Theme", {"theme", "font_size"}, SideEffect::WritesFile, tool_flags::kMutating,
      kSetThemeFontSizeParams, theme_ops::handle_set_theme_font_size}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_theme_stylebox_flat",
      "Build a StyleBoxFlat and register it as the stylebox item varname under theme_type of a saved .tres theme. The stylebox object holds optional bg_color and border_color (hex string or {r,g,b,a}), border_width {left,top,right,bottom}, corner_radius {top_left,top_right,bottom_right,bottom_left} and content_margin {left,top,right,bottom} — every field is optional, and border_width/content_margin also accept a single number applied to all sides. The theme is loaded, modified and saved back to disk. Returns ok with saved:true.",
      "Theme", {"theme", "stylebox"}, SideEffect::WritesFile, tool_flags::kMutating,
      kSetThemeStyleboxFlatParams, theme_ops::handle_set_theme_stylebox_flat}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_theme_info",
      "Inspect a saved .tres theme: lists every theme type it defines and the item names registered under each type, grouped into colors, constants, font_sizes and styleboxes. Use it before the set_theme_* tools to discover valid theme_type/varname pairs. Requires path. Returns path, type_count and the types array.",
      "Theme", {"theme", "inspect"}, SideEffect::None, tool_flags::kNone,
      kGetThemeInfoParams, theme_ops::handle_get_theme_info}));
  v.push_back(make_spec_tool(ToolSpec{
      "apply_theme_to_control",
      "Assign a .tres theme file to a Control node in the edited scene. Requires theme_path (the .tres to load) and control_path (node path inside the current scene). Only the in-memory scene changes — call save_editor_scene afterwards to persist the assignment. Returns ok with control_path and theme_path.",
      "Theme", {"theme", "apply", "control"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kApplyThemeToControlParams, theme_ops::handle_apply_theme_to_control}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_control_anchor_preset",
      "Apply one of the 16 Control layout presets (LayoutPreset integer) to a node in the edited scene: top_left=0, top_right=1, bottom_left=2, bottom_right=3, center_left=4..center=8, left_wide=9, top_wide=10, right_wide=11, bottom_wide=12, vcenter_wide=13, hcenter_wide=14, full_rect=15. keep_offsets (default false) keeps the node's current offsets instead of recomputing them for the new anchors. Only the in-memory scene changes — call save_editor_scene to persist. Returns ok with the applied preset.",
      "Theme", {"control", "anchor", "layout"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kSetControlAnchorPresetParams, theme_ops::handle_set_control_anchor_preset}));
  return v;
}

} // namespace theme_tools
} // namespace godot_autopilot

#endif
