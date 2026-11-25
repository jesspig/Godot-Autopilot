#ifndef GODOT_AUTOPILOT_TEXT_TOOLS_HPP
#define GODOT_AUTOPILOT_TEXT_TOOLS_HPP

#include "util/json_value.hpp"
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/text/text_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace text_tools {

namespace {

const std::vector<ParamSpec> kFontParams = {
    {"font_rid", "integer", "Only with action=set_data, action=set_antialiasing or action=set_hinting: RID id returned by action=create", false},
    {"data", "string", "Only with action=set_data: path to a font file (.ttf, .otf, .woff2); action=system_path returns a usable path", false},
    {"antialiasing", "integer", "Only with action=set_antialiasing: font antialiasing mode: 0=none, 1=gray, 2=grayscale, 3=subpixel", false},
    {"hinting", "integer", "Only with action=set_hinting: font hinting mode: 0=none, 1=light, 2=normal", false},
    {"font_name", "string", "Only with action=system_path: system font name (e.g. 'Arial', 'Segoe UI', 'Noto Sans')", false},
    {"weight", "integer", "Only with action=system_path: font weight, 100-900 (default: 400)", false},
    {"stretch", "integer", "Only with action=system_path: font stretch percentage (default: 100)", false},
    {"italic", "boolean", "Only with action=system_path: request italic variant (default: false)", false},
};

const std::vector<ParamSpec> kShapedTextParams = {
    {"direction", "integer", "Only with action=create: text layout direction 0=auto, 1=ltr, 2=rtl (default: 0)", false},
    {"orientation", "integer", "Only with action=create: text orientation 0=horizontal, 1=vertical (default: 0)", false},
    {"shaped_rid", "integer", "Only with action=add_string or action=get_size: RID id returned by action=create", false},
    {"text", "string", "Only with action=add_string: text string to add (may contain Unicode)", false},
    {"font_rid", "integer", "Only with action=add_string: RID id returned by font action=create (configured with action=set_data first)", false},
    {"size", "integer", "Only with action=add_string: font size in pixels", false},
    {"language", "string", "Only with action=add_string: language code (e.g. 'en', 'ar') to improve shaping; omit for auto detection", false},
};

const std::vector<ParamSpec> kTextQueryParams = {
    {"feature", "integer", "Only with action=has_feature: TextServer feature flag (TextServer.Feature enum: 1=simple_layout, 2=bidi_layout, 4=shaped, 8=kerning, 16=ligatures, 32=font_lcd_subpixel, 64=font_autohinter, 128=font_subpixel_positioning, 256=font_system, 512=font_variable, 1024=context_sensitive_cleartype, 2048=fast_path, 4096=shaping_fallback, 8192=unicode_security, 16384=has_rid)", false},
    {"locale", "string", "Only with action=locale_right_to_left: BCP-47 locale code (e.g. 'ar', 'he', 'fa' for RTL; 'en', 'de' for LTR)", false},
};

const std::vector<std::string> kFontActions = {
    "create", "set_data", "set_antialiasing", "set_hinting", "system_path"};
const std::vector<std::string> kShapedTextActions = {
    "create", "add_string", "get_size"};
const std::vector<std::string> kTextQueryActions = {
    "has_feature", "locale_right_to_left"};

inline godot_autopilot::util::JsonValue text_action_args(const godot_autopilot::util::JsonValue &args,
                                       const char *action) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  out["action"] = godot_autopilot::util::JsonValue(action);
  if (!args.IsObject()) {
    return out;
  }
  for (const auto &entry : args.GetObject()) {
    if (entry.first != "action" && entry.first != "confirm") {
      out[entry.first] = entry.second;
    }
  }
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(3);
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "font",
      "Create and configure TextServer font objects in one tool. action selects the operation: 'create' returns a font RID id as result and must run first, every other action requires that id as font_rid; 'set_data' loads a font file (.ttf, .otf, .woff2) from data and assigns its bytes (action=system_path returns a path usable here); 'set_antialiasing' sets antialiasing 0=none, 1=gray, 2=grayscale, 3=subpixel (use 0 for pixel-art fonts); 'set_hinting' sets hinting 0=none, 1=light, 2=normal (apply after set_data); 'system_path' resolves a system-installed font to an absolute file path with optional weight (default 400), stretch (default 100) and italic. Returns the RID id, the resolved path, or 'ok' depending on the action. Example: {\"action\": \"create\"} then {\"action\": \"set_data\", \"font_rid\": 1, \"data\": \"res://fonts/ui.ttf\"}.",
      "Text", {"text", "font", "rid", "antialiasing", "hinting"},
      SideEffect::None, tool_flags::kNone,
      kFontParams, text_ops::handle_font,
      schema::build_action_schema(
          kFontActions, kFontParams,
          "create: new TextServer font, returns the RID id; set_data: load a font file into a font; set_antialiasing: set the antialiasing mode; set_hinting: set the hinting mode; system_path: resolve a system font name to a file path"),
      {},
      {"font action=create",
       "font action=system_path font_name=Arial"}},
      {
          ToolAlias{
              "create_text_font", "font",
              [](const godot_autopilot::util::JsonValue &args) {
                return text_action_args(args, "create");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_text_font_data", "font",
              [](const godot_autopilot::util::JsonValue &args) {
                return text_action_args(args, "set_data");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_text_font_antialiasing", "font",
              [](const godot_autopilot::util::JsonValue &args) {
                return text_action_args(args, "set_antialiasing");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_text_font_hinting", "font",
              [](const godot_autopilot::util::JsonValue &args) {
                return text_action_args(args, "set_hinting");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_text_font_system_path", "font",
              [](const godot_autopilot::util::JsonValue &args) {
                return text_action_args(args, "system_path");
              },
              {}, "0.4.0", "0.6.0"},
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "shaped_text",
      "Create and measure shaped text objects in one tool. action selects the operation: 'create' returns a shaped text RID id as result, with optional direction (0=auto, 1=ltr, 2=rtl) and orientation (0=horizontal, 1=vertical); 'add_string' adds a text string to that shaped text (requires shaped_rid, text, font_rid from font action=create configured with action=set_data, and size in pixels; optional language improves shaping) and returns true on success; 'get_size' measures the rendered size of the shaped text as {x, y} pixels and is only meaningful after add_string. Errors when the TextServer is unavailable. Example: {\"action\": \"create\", \"direction\": 1} then {\"action\": \"get_size\", \"shaped_rid\": 1}.",
      "Text", {"text", "shaped", "rid", "measure", "size"},
      SideEffect::None, tool_flags::kNone,
      kShapedTextParams, text_ops::handle_shaped_text,
      schema::build_action_schema(
          kShapedTextActions, kShapedTextParams,
          "create: new shaped text object, returns the RID id; add_string: add a string with a font and size; get_size: measure the rendered size"),
      {},
      {"shaped_text action=create direction=1",
       "shaped_text action=get_size shaped_rid=1"}},
      {
          ToolAlias{
              "create_shaped_text", "shaped_text",
              [](const godot_autopilot::util::JsonValue &args) {
                return text_action_args(args, "create");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "add_shaped_text_string", "shaped_text",
              [](const godot_autopilot::util::JsonValue &args) {
                return text_action_args(args, "add_string");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_shaped_text_size", "shaped_text",
              [](const godot_autopilot::util::JsonValue &args) {
                return text_action_args(args, "get_size");
              },
              {}, "0.4.0", "0.6.0"},
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "text_query",
      "Query the active TextServer in one tool. action selects the question: 'has_feature' checks whether a layout or shaping feature is supported and requires feature, a TextServer.Feature enum value (1=simple_layout, 2=bidi_layout, 4=shaped, 8=kerning, 16=ligatures, and more listed in the schema); 'locale_right_to_left' checks whether a locale uses right-to-left text direction and requires a BCP-47 locale code (e.g. 'ar', 'he', 'en'). Both return true or false as result. Example: {\"action\": \"has_feature\", \"feature\": 4} or {\"action\": \"locale_right_to_left\", \"locale\": \"ar\"}.",
      "Text", {"text", "feature", "locale", "query"},
      SideEffect::None, tool_flags::kNone,
      kTextQueryParams, text_ops::handle_text_query,
      schema::build_action_schema(
          kTextQueryActions, kTextQueryParams,
          "has_feature: whether the TextServer supports a feature flag; locale_right_to_left: whether a locale is right-to-left"),
      {},
      {"text_query action=has_feature feature=4",
       "text_query action=locale_right_to_left locale=ar"}},
      {
          ToolAlias{
              "has_text_feature", "text_query",
              [](const godot_autopilot::util::JsonValue &args) {
                return text_action_args(args, "has_feature");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "is_text_locale_right_to_left", "text_query",
              [](const godot_autopilot::util::JsonValue &args) {
                return text_action_args(args, "locale_right_to_left");
              },
              {}, "0.4.0", "0.6.0"},
      })));
  return v;
}

} // namespace text_tools
} // namespace godot_autopilot

#endif
