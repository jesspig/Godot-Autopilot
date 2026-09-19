#ifndef GODOT_AUTOPILOT_TEXT_TOOLS_HPP
#define GODOT_AUTOPILOT_TEXT_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/text_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace text_tools {

namespace {

const std::vector<ParamSpec> kCreateTextFontParams = {};

const std::vector<ParamSpec> kCreateShapedTextParams = {
    {"direction", "integer", "Text layout direction: 0=auto, 1=ltr, 2=rtl (default: 0)", false},
    {"orientation", "integer", "Text orientation: 0=horizontal, 1=vertical (default: 0)", false},
};

const std::vector<ParamSpec> kSetTextFontAntialiasingParams = {
    {"font_rid", "integer", "RID id returned by create_text_font", true},
    {"antialiasing", "integer", "Font antialiasing mode: 0=none, 1=gray, 2=grayscale, 3=subpixel", true},
};

const std::vector<ParamSpec> kSetTextFontDataParams = {
    {"font_rid", "integer", "RID id returned by create_text_font", true},
    {"data", "string", "Path to a font file (.ttf, .otf, .woff2); get_text_font_system_path returns a usable path", true},
};

const std::vector<ParamSpec> kSetTextFontHintingParams = {
    {"font_rid", "integer", "RID id returned by create_text_font", true},
    {"hinting", "integer", "Font hinting mode: 0=none, 1=light, 2=normal", true},
};

const std::vector<ParamSpec> kGetTextFontSystemPathParams = {
    {"font_name", "string", "System font name (e.g. 'Arial', 'Segoe UI', 'Noto Sans')", true},
    {"weight", "integer", "Font weight, 100-900 (default: 400)", false},
    {"stretch", "integer", "Font stretch percentage (default: 100)", false},
    {"italic", "boolean", "Request italic variant (default: false)", false},
};

const std::vector<ParamSpec> kHasTextFeatureParams = {
    {"feature", "integer", "TextServer feature flag (TextServer.Feature enum: 1=simple_layout, 2=bidi_layout, 4=shaped, 8=kerning, 16=ligatures, 32=font_lcd_subpixel, 64=font_autohinter, 128=font_subpixel_positioning, 256=font_system, 512=font_variable, 1024=context_sensitive_cleartype, 2048=fast_path, 4096=shaping_fallback, 8192=unicode_security, 16384=has_rid)", true},
};

const std::vector<ParamSpec> kIsTextLocaleRightToLeftParams = {
    {"locale", "string", "BCP-47 locale code (e.g. 'ar', 'he', 'fa' for RTL; 'en', 'de' for LTR)", true},
};

const std::vector<ParamSpec> kAddShapedTextStringParams = {
    {"shaped_rid", "integer", "RID id returned by create_shaped_text", true},
    {"text", "string", "Text string to add (may contain Unicode)", true},
    {"font_rid", "integer", "RID id returned by create_text_font (configured with set_text_font_data first)", true},
    {"size", "integer", "Font size in pixels", true},
    {"language", "string", "Language code (e.g. 'en', 'ar') to improve shaping; omit for auto detection", false},
};

const std::vector<ParamSpec> kGetShapedTextSizeParams = {
    {"shaped_rid", "integer", "RID id returned by create_shaped_text", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(10);
  v.push_back(make_spec_tool(ToolSpec{
      "create_text_font",
      "Create a font object in the TextServer and return its RID id as 'result'. Use it before the other text tools: the id is passed as font_rid to set_text_font_data, set_text_font_antialiasing, set_text_font_hinting and add_shaped_text_string. Every font tool requires a valid font_rid obtained here. Errors if the TextServer is unavailable.",
      "Text", {"text", "font"}, SideEffect::None, tool_flags::kNone,
      kCreateTextFontParams, text_ops::handle_create_font}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_shaped_text",
      "Create a shaped text object in the TextServer and return its RID id as 'result'. Optional 'direction' (0=auto, 1=ltr, 2=rtl) and 'orientation' (0=horizontal, 1=vertical) select layout behavior, both default to auto/horizontal. Pass the id as shaped_rid to add_shaped_text_string to add text, then to get_shaped_text_size for the measured size. Errors if the TextServer is unavailable.",
      "Text", {"text", "shaped"}, SideEffect::None, tool_flags::kNone,
      kCreateShapedTextParams, text_ops::handle_create_shaped_text}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_text_font_antialiasing",
      "Set the antialiasing mode of a font created by create_text_font. Requires font_rid (the RID id returned by create_text_font) and antialiasing (0=none, 1=gray, 2=grayscale, 3=subpixel). Call after set_text_font_data so the mode applies to the loaded font. Use 0 for pixel-art fonts. Returns 'ok'.",
      "Text", {"text", "font"}, SideEffect::None, tool_flags::kNone,
      kSetTextFontAntialiasingParams, text_ops::handle_font_set_antialiasing}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_text_font_data",
      "Load a font file and assign its bytes to a font created by create_text_font. 'data' is a path to a font file (.ttf, .otf, .woff2); get_text_font_system_path returns an absolute path that can be passed here directly. Requires font_rid from create_text_font. Returns 'ok'; errors when the file cannot be read.",
      "Text", {"text", "font"}, SideEffect::None, tool_flags::kNone,
      kSetTextFontDataParams, text_ops::handle_font_set_data}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_text_font_hinting",
      "Set the hinting mode of a font created by create_text_font. Requires font_rid (the RID id returned by create_text_font) and hinting (0=none, 1=light, 2=normal). Apply after set_text_font_data so the setting affects the loaded font. Light hinting is the typical default for UI text. Returns 'ok'.",
      "Text", {"text", "font"}, SideEffect::None, tool_flags::kNone,
      kSetTextFontHintingParams, text_ops::handle_font_set_hinting}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_text_font_system_path",
      "Resolve a system-installed font to an absolute file path. Requires font_name; optional weight (default 400), stretch (default 100) and italic refine the match. The returned path can be used directly as the 'data' argument of set_text_font_data. Returns the path as 'result'.",
      "Text", {"text", "font"}, SideEffect::None, tool_flags::kNone,
      kGetTextFontSystemPathParams, text_ops::handle_get_system_font_path}));
  v.push_back(make_spec_tool(ToolSpec{
      "has_text_feature",
      "Check whether the active TextServer supports a layout or shaping feature. Requires feature, a TextServer.Feature enum value (1=simple_layout, 2=bidi_layout, 4=shaped, 8=kerning, 16=ligatures, and more listed in the schema). Returns true or false as 'result'. Query it before relying on a feature such as shaped text or ligatures.",
      "Text", {"text", "feature"}, SideEffect::None, tool_flags::kNone,
      kHasTextFeatureParams, text_ops::handle_has_feature}));
  v.push_back(make_spec_tool(ToolSpec{
      "is_text_locale_right_to_left",
      "Check whether a locale uses right-to-left text direction. Requires a BCP-47 locale code (e.g. 'ar', 'he', 'en'); returns true or false as 'result'. A false result means left-to-right. Use it to pick text alignment or font variants before shaping text for that locale.",
      "Text", {"text", "locale"}, SideEffect::None, tool_flags::kNone,
      kIsTextLocaleRightToLeftParams, text_ops::handle_is_locale_right_to_left}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_shaped_text_string",
      "Add a text string to a shaped text object created by create_shaped_text. Requires shaped_rid (the RID id from create_shaped_text), text, font_rid (the RID id of a font configured with set_text_font_data) and size in pixels; optional language code improves shaping. Returns true on success as 'result'; run get_shaped_text_size afterwards to measure.",
      "Text", {"text", "shaped"}, SideEffect::None, tool_flags::kNone,
      kAddShapedTextStringParams, text_ops::handle_shaped_text_add_string}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_shaped_text_size",
      "Measure the rendered size of a shaped text object after adding strings with add_shaped_text_string. Requires shaped_rid (the RID id from create_shaped_text). Returns {x, y} pixel dimensions as 'result'. Call it only after add_shaped_text_string, otherwise the reported size is zero. Use it to size labels before adding them.",
      "Text", {"text", "shaped"}, SideEffect::None, tool_flags::kNone,
      kGetShapedTextSizeParams, text_ops::handle_shaped_text_get_size}));
  return v;
}

} // namespace text_tools
} // namespace godot_autopilot

#endif