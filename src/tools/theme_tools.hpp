#ifndef GODOT_AUTOPILOT_THEME_TOOLS_HPP
#define GODOT_AUTOPILOT_THEME_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/theme_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace theme_tools {

GDA_TOOL_CLASS_SIDE(CreateThemeResourceTool, "create_theme_resource",
               "Create an empty Theme resource on disk at path (a res:// .tres path — res:// only; user:// rejected). Missing directories are created and the editor file system is refreshed after saving; an existing file is never overwritten. Optional base_type (e.g. Control) is stored as resource metadata for reference — Godot themes carry no global base type, so actual items are added with set_theme_color, set_theme_constant, set_theme_font_size and set_theme_stylebox_flat. Returns ok with the saved path.",
               "Theme", std::vector<std::string>({"theme", "create"}), theme_ops::handle_create_theme_resource, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS_SIDE(SetThemeColorTool, "set_theme_color",
               "Set a color item on a saved .tres theme. Requires path, theme_type (the widget class the item belongs to, e.g. Button or Label) and varname (the item name, e.g. font_color); color accepts a hex string like \"#rrggbb\" or \"#rrggbbaa\" or an object {r,g,b,a} with floats in 0..1. The theme is loaded, modified and saved back to disk. Use get_theme_info to discover valid type/name pairs. Returns ok with saved:true.",
               "Theme", std::vector<std::string>({"theme", "color"}), theme_ops::handle_set_theme_color, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS_SIDE(SetThemeConstantTool, "set_theme_constant",
               "Set an integer constant item (e.g. separation, outline_size) on a saved .tres theme. Requires path, theme_type and varname plus the integer constant. The theme is loaded, modified and saved back to disk. Returns ok with saved:true.",
               "Theme", std::vector<std::string>({"theme", "constant"}), theme_ops::handle_set_theme_constant, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS_SIDE(SetThemeFontSizeTool, "set_theme_font_size",
               "Set a font size item on a saved .tres theme. Requires path, theme_type and varname plus the integer font_size. The theme is loaded, modified and saved back to disk; use set_theme_color or set_theme_stylebox_flat for other item kinds. Returns ok with saved:true.",
               "Theme", std::vector<std::string>({"theme", "font_size"}), theme_ops::handle_set_theme_font_size, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS_SIDE(SetThemeStyleboxFlatTool, "set_theme_stylebox_flat",
               "Build a StyleBoxFlat and register it as the stylebox item varname under theme_type of a saved .tres theme. The stylebox object holds optional bg_color and border_color (hex string or {r,g,b,a}), border_width {left,top,right,bottom}, corner_radius {top_left,top_right,bottom_right,bottom_left} and content_margin {left,top,right,bottom} — every field is optional, and border_width/content_margin also accept a single number applied to all sides. The theme is loaded, modified and saved back to disk. Returns ok with saved:true.",
               "Theme", std::vector<std::string>({"theme", "stylebox"}), theme_ops::handle_set_theme_stylebox_flat, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS(GetThemeInfoTool, "get_theme_info",
               "Inspect a saved .tres theme: lists every theme type it defines and the item names registered under each type, grouped into colors, constants, font_sizes and styleboxes. Use it before the set_theme_* tools to discover valid theme_type/varname pairs. Requires path. Returns path, type_count and the types array.",
               "Theme", std::vector<std::string>({"theme", "inspect"}), theme_ops::handle_get_theme_info, true)

GDA_TOOL_CLASS(ApplyThemeToControlTool, "apply_theme_to_control",
               "Assign a .tres theme file to a Control node in the edited scene. Requires theme_path (the .tres to load) and control_path (node path inside the current scene). Only the in-memory scene changes — call save_editor_scene afterwards to persist the assignment. Returns ok with control_path and theme_path.",
               "Theme", std::vector<std::string>({"theme", "apply", "control"}), theme_ops::handle_apply_theme_to_control, true)

GDA_TOOL_CLASS(SetControlAnchorPresetTool, "set_control_anchor_preset",
               "Apply one of the 16 Control layout presets (LayoutPreset integer) to a node in the edited scene: top_left=0, top_right=1, bottom_left=2, bottom_right=3, center_left=4..center=8, left_wide=9, top_wide=10, right_wide=11, bottom_wide=12, vcenter_wide=13, hcenter_wide=14, full_rect=15. keep_offsets (default false) keeps the node's current offsets instead of recomputing them for the new anchors. Only the in-memory scene changes — call save_editor_scene to persist. Returns ok with the applied preset.",
               "Theme", std::vector<std::string>({"control", "anchor", "layout"}), theme_ops::handle_set_control_anchor_preset, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(8);
  v.push_back(std::make_unique<CreateThemeResourceTool>());
  v.push_back(std::make_unique<SetThemeColorTool>());
  v.push_back(std::make_unique<SetThemeConstantTool>());
  v.push_back(std::make_unique<SetThemeFontSizeTool>());
  v.push_back(std::make_unique<SetThemeStyleboxFlatTool>());
  v.push_back(std::make_unique<GetThemeInfoTool>());
  v.push_back(std::make_unique<ApplyThemeToControlTool>());
  v.push_back(std::make_unique<SetControlAnchorPresetTool>());
  return v;
}

} // namespace theme_tools
} // namespace godot_autopilot

#endif
