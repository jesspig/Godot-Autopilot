#ifndef GODOT_AUTOPILOT_MCP_IMAGE_CONTENT_HPP
#define GODOT_AUTOPILOT_MCP_IMAGE_CONTENT_HPP

#include <mcp/Content.hpp>
#include <mcp/JsonValue.hpp>

#include <string>
#include <vector>

namespace godot_autopilot {
namespace util {

inline bool is_image_capture_tool(const std::string &tool_name) {
  return tool_name == "capture_editor_viewport" ||
         tool_name == "capture_game_viewport" ||
         tool_name == "capture_display_screen" ||
         tool_name == "click_input_mouse" || tool_name == "scroll_input_mouse" ||
         tool_name == "drag_input_mouse" || tool_name == "type_input_text" ||
         tool_name == "click_editor_element" ||
         tool_name == "type_editor_element_text";
}

inline bool try_attach_image_data(
    mcp::JsonValue &result_json,
    std::vector<mcp::ContentVariant> &content_out,
    std::string *reason = nullptr) {
  if (!result_json.IsObject()) {
    if (reason)
      *reason = "result JSON is not an object";
    return false;
  }

  mcp::JsonValue *container = nullptr;
  mcp::JsonValue *data = nullptr;
  auto *top_data = result_json.Find("data");
  if (top_data && top_data->IsString() && !top_data->GetString().empty()) {
    container = &result_json;
    data = top_data;
  } else if (auto *result = result_json.Find("result");
             result && result->IsObject()) {
    auto *nested_data = result->Find("data");
    if (nested_data && nested_data->IsString() &&
        !nested_data->GetString().empty()) {
      container = result;
      data = nested_data;
    }
  }
  if (!data) {
    if (reason)
      *reason = "no non-empty string data field";
    return false;
  }

  auto *format = container->Find("format");
  if (format && !(format->IsString() && format->GetString() == "png")) {
    if (reason)
      *reason = "format is not png";
    return false;
  }

  std::string b64 = data->GetString();
  (*container)["data"] = mcp::JsonValue("<attached-as-image-content>");
  (*container)["image_attached"] = mcp::JsonValue(true);
  content_out.push_back(mcp::ImageContent{"image", std::move(b64), "image/png"});
  return true;
}

inline bool try_attach_image_content(
    const std::string &tool_name, mcp::JsonValue &result_json,
    std::vector<mcp::ContentVariant> &content_out,
    std::string *reason = nullptr) {
  if (!is_image_capture_tool(tool_name)) {
    if (reason)
      *reason = "tool not in image capture whitelist";
    return false;
  }
  return try_attach_image_data(result_json, content_out, reason);
}

} // namespace util
} // namespace godot_autopilot

#endif
