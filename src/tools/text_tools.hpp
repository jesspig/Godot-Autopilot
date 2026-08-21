#ifndef GODOT_AUTOPILOT_TEXT_TOOLS_HPP
#define GODOT_AUTOPILOT_TEXT_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/text_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace text_tools {

GDA_TOOL_CLASS(CreateTextFontTool, "create_text_font",
               "Create a font object in the TextServer and return its RID id as 'result'. Use it before the other text tools: the id is passed as font_rid to set_text_font_data, set_text_font_antialiasing, set_text_font_hinting and add_shaped_text_string. Every font tool requires a valid font_rid obtained here. Errors if the TextServer is unavailable.",
               "Text", std::vector<std::string>({"text", "font"}), text_ops::handle_create_font, false)

GDA_TOOL_CLASS(CreateShapedTextTool, "create_shaped_text",
               "Create a shaped text object in the TextServer and return its RID id as 'result'. Optional 'direction' (0=auto, 1=ltr, 2=rtl) and 'orientation' (0=horizontal, 1=vertical) select layout behavior, both default to auto/horizontal. Pass the id as shaped_rid to add_shaped_text_string to add text, then to get_shaped_text_size for the measured size. Errors if the TextServer is unavailable.",
               "Text", std::vector<std::string>({"text", "shaped"}), text_ops::handle_create_shaped_text, false)

GDA_TOOL_CLASS(SetTextFontAntialiasingTool, "set_text_font_antialiasing",
               "Set the antialiasing mode of a font created by create_text_font. Requires font_rid (the RID id returned by create_text_font) and antialiasing (0=none, 1=gray, 2=grayscale, 3=subpixel). Call after set_text_font_data so the mode applies to the loaded font. Use 0 for pixel-art fonts. Returns 'ok'.",
               "Text", std::vector<std::string>({"text", "font"}), text_ops::handle_font_set_antialiasing, false)

GDA_TOOL_CLASS(SetTextFontDataTool, "set_text_font_data",
               "Load a font file and assign its bytes to a font created by create_text_font. 'data' is a path to a font file (.ttf, .otf, .woff2); get_text_font_system_path returns an absolute path that can be passed here directly. Requires font_rid from create_text_font. Returns 'ok'; errors when the file cannot be read.",
               "Text", std::vector<std::string>({"text", "font"}), text_ops::handle_font_set_data, false)

GDA_TOOL_CLASS(SetTextFontHintingTool, "set_text_font_hinting",
               "Set the hinting mode of a font created by create_text_font. Requires font_rid (the RID id returned by create_text_font) and hinting (0=none, 1=light, 2=normal). Apply after set_text_font_data so the setting affects the loaded font. Light hinting is the typical default for UI text. Returns 'ok'.",
               "Text", std::vector<std::string>({"text", "font"}), text_ops::handle_font_set_hinting, false)

GDA_TOOL_CLASS(GetTextFontSystemPathTool, "get_text_font_system_path",
               "Resolve a system-installed font to an absolute file path. Requires font_name; optional weight (default 400), stretch (default 100) and italic refine the match. The returned path can be used directly as the 'data' argument of set_text_font_data. Returns the path as 'result'.",
               "Text", std::vector<std::string>({"text", "font"}), text_ops::handle_get_system_font_path, false)

GDA_TOOL_CLASS(HasTextFeatureTool, "has_text_feature",
               "Check whether the active TextServer supports a layout or shaping feature. Requires feature, a TextServer.Feature enum value (1=simple_layout, 2=bidi_layout, 4=shaped, 8=kerning, 16=ligatures, and more listed in the schema). Returns true or false as 'result'. Query it before relying on a feature such as shaped text or ligatures.",
               "Text", std::vector<std::string>({"text", "feature"}), text_ops::handle_has_feature, false)

GDA_TOOL_CLASS(IsTextLocaleRightToLeftTool, "is_text_locale_right_to_left",
               "Check whether a locale uses right-to-left text direction. Requires a BCP-47 locale code (e.g. 'ar', 'he', 'en'); returns true or false as 'result'. A false result means left-to-right. Use it to pick text alignment or font variants before shaping text for that locale.",
               "Text", std::vector<std::string>({"text", "locale"}), text_ops::handle_is_locale_right_to_left, false)

GDA_TOOL_CLASS(AddShapedTextStringTool, "add_shaped_text_string",
               "Add a text string to a shaped text object created by create_shaped_text. Requires shaped_rid (the RID id from create_shaped_text), text, font_rid (the RID id of a font configured with set_text_font_data) and size in pixels; optional language code improves shaping. Returns true on success as 'result'; run get_shaped_text_size afterwards to measure.",
               "Text", std::vector<std::string>({"text", "shaped"}), text_ops::handle_shaped_text_add_string, false)

GDA_TOOL_CLASS(GetShapedTextSizeTool, "get_shaped_text_size",
               "Measure the rendered size of a shaped text object after adding strings with add_shaped_text_string. Requires shaped_rid (the RID id from create_shaped_text). Returns {x, y} pixel dimensions as 'result'. Call it only after add_shaped_text_string, otherwise the reported size is zero. Use it to size labels before adding them.",
               "Text", std::vector<std::string>({"text", "shaped"}), text_ops::handle_shaped_text_get_size, false)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(10);
  v.push_back(std::make_unique<CreateTextFontTool>());
  v.push_back(std::make_unique<CreateShapedTextTool>());
  v.push_back(std::make_unique<SetTextFontAntialiasingTool>());
  v.push_back(std::make_unique<SetTextFontDataTool>());
  v.push_back(std::make_unique<SetTextFontHintingTool>());
  v.push_back(std::make_unique<GetTextFontSystemPathTool>());
  v.push_back(std::make_unique<HasTextFeatureTool>());
  v.push_back(std::make_unique<IsTextLocaleRightToLeftTool>());
  v.push_back(std::make_unique<AddShapedTextStringTool>());
  v.push_back(std::make_unique<GetShapedTextSizeTool>());
  return v;
}

} // namespace text_tools
} // namespace godot_autopilot

#endif