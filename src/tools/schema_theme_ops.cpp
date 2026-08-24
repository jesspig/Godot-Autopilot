#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_theme(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["create_theme_resource"] = schema::build_schema({
            {"path", "string", "Destination .tres path inside res:// (res:// only, user:// rejected), e.g. 'res://themes/main.tres'; missing directories are created and an existing file is never overwritten", true},
            {"base_type", "string", "Base type stored as resource metadata for reference, e.g. 'Control'; Godot themes carry no global base type", false},
        });
        m["set_theme_color"] = schema::build_schema({
            {"path", "string", "Path of a saved .tres theme file created with create_theme_resource", true},
            {"theme_type", "string", "Widget class the color item belongs to, e.g. 'Button' or 'Label'", true},
            {"varname", "string", "Color item name, e.g. 'font_color'", true},
            {"color", "object", "Color value: hex string like '#rrggbb' or '#rrggbbaa', or object {r,g,b,a} with floats in 0..1", true},
        });
        m["set_theme_constant"] = schema::build_schema({
            {"path", "string", "Path of a saved .tres theme file created with create_theme_resource", true},
            {"theme_type", "string", "Widget class the constant item belongs to, e.g. 'Button'", true},
            {"varname", "string", "Constant item name, e.g. 'separation' or 'outline_size'", true},
            {"constant", "integer", "Integer constant value to set", true},
        });
        m["set_theme_font_size"] = schema::build_schema({
            {"path", "string", "Path of a saved .tres theme file created with create_theme_resource", true},
            {"theme_type", "string", "Widget class the font size item belongs to, e.g. 'Label'", true},
            {"varname", "string", "Font size item name, e.g. 'font_size'", true},
            {"font_size", "integer", "Integer font size value to set", true},
        });
        m["set_theme_stylebox_flat"] = schema::build_schema({
            {"path", "string", "Path of a saved .tres theme file created with create_theme_resource", true},
            {"theme_type", "string", "Widget class the stylebox item belongs to, e.g. 'Button'", true},
            {"varname", "string", "Stylebox item name, e.g. 'normal' or 'hover'", true},
            {"stylebox", "object", "StyleBoxFlat definition: optional bg_color/border_color (hex string or {r,g,b,a}), border_width {left,top,right,bottom}, corner_radius {top_left,top_right,bottom_right,bottom_left}, content_margin {left,top,right,bottom}; border_width/content_margin also accept a single number applied to all sides", true},
        });
        m["get_theme_info"] = schema::build_schema({
            {"path", "string", "Path of a saved .tres theme file to inspect", true},
        });
        m["apply_theme_to_control"] = schema::build_schema({
            {"theme_path", "string", "Path of the .tres theme file to assign", true},
            {"control_path", "string", "Path of the Control node in the edited scene receiving the theme; only the in-memory scene changes — call save_editor_scene afterwards to persist", true},
        });
        m["set_control_anchor_preset"] = schema::build_schema({
            {"control_path", "string", "Path of the Control node in the edited scene; only the in-memory scene changes — call save_editor_scene to persist", true},
            {"preset", "integer", "Control LayoutPreset integer 0..15: top_left=0, top_right=1, bottom_left=2, bottom_right=3, center_left=4..center=8, left_wide=9, top_wide=10, right_wide=11, bottom_wide=12, vcenter_wide=13, hcenter_wide=14, full_rect=15", true},
            {"keep_offsets", "boolean", "Keep the node's current offsets instead of recomputing them for the new anchors (boolean, default: false)", false},
        });
}

} // namespace godot_autopilot
