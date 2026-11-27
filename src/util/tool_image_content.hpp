#ifndef GODOT_AUTOPILOT_TOOL_IMAGE_CONTENT_HPP
#define GODOT_AUTOPILOT_TOOL_IMAGE_CONTENT_HPP

#include "util/json_value.hpp"

#include <string>
#include <vector>

namespace godot_autopilot {
namespace util {

using JsonValue = godot_autopilot::util::JsonValue;

inline bool is_image_capture_tool(const std::string &tool_name) {
  return tool_name == "capture_editor_viewport" ||
         tool_name == "capture_game_viewport" ||
         tool_name == "capture_display_screen" ||
         tool_name == "click_input_mouse" || tool_name == "scroll_input_mouse" ||
         tool_name == "drag_input_mouse" || tool_name == "type_input_text" ||
         tool_name == "click_editor_element" ||
         tool_name == "type_editor_element_text";
}

inline bool try_attach_image_data(JsonValue &result_json,
                                  std::vector<JsonValue> &content_out,
                                  std::string *reason = nullptr) {
  if (!result_json.IsObject()) {
    if (reason != nullptr)
      *reason = "result JSON is not an object";
    return false;
  }
  JsonValue *container = nullptr;
  JsonValue *data = nullptr;
  if (JsonValue *top = result_json.Find("data");
      top != nullptr && top->IsString() && !top->GetString().empty()) {
    container = &result_json;
    data = top;
  } else if (JsonValue *inner = result_json.Find("result");
             inner != nullptr && inner->IsObject()) {
    if (JsonValue *nested = inner->Find("data");
        nested != nullptr && nested->IsString() && !nested->GetString().empty()) {
      container = inner;
      data = nested;
    }
  }
  if (data == nullptr) {
    if (reason != nullptr)
      *reason = "no non-empty string data field";
    return false;
  }
  if (const JsonValue *format = container->Find("format");
      format != nullptr && !(format->IsString() && format->GetString() == "png")) {
    if (reason != nullptr)
      *reason = "format is not png";
    return false;
  }
  std::string encoded = data->GetString();
  (*container)["data"] = JsonValue("<attached-as-image-content>");
  (*container)["image_attached"] = JsonValue(true);
  JsonValue block(JsonValue::object_tag);
  block["type"] = JsonValue("image");
  block["data"] = JsonValue(std::move(encoded));
  block["mimeType"] = JsonValue("image/png");
  content_out.push_back(std::move(block));
  return true;
}

inline bool try_attach_image_content(const std::string &tool_name,
                                     JsonValue &result_json,
                                     std::vector<JsonValue> &content_out,
                                     std::string *reason = nullptr) {
  if (!is_image_capture_tool(tool_name)) {
    if (reason != nullptr)
      *reason = "tool not in image capture whitelist";
    return false;
  }
  return try_attach_image_data(result_json, content_out, reason);
}

} // namespace util
} // namespace godot_autopilot

#endif
