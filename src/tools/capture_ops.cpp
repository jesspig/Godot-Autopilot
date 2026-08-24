#include "capture_ops.hpp"
#include "../util/error_util.hpp"
#include "core/config.hpp"
#include "core/log_system.hpp"
#include "tools/runtime_ops.hpp"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <string>

namespace godot_autopilot {
namespace capture_ops {

namespace {

godot::Ref<godot::ViewportTexture>
usable_viewport_texture(godot::SubViewport *viewport) {
  godot::Ref<godot::ViewportTexture> texture;
  if (!viewport)
    return texture;
  texture = viewport->get_texture();
  if (texture.is_null() || texture->get_image().is_null()) {
    texture.unref();
  }
  return texture;
}

static const char b64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

} // namespace

std::string base64_encode(const uint8_t *data, size_t len) {
  std::string result;
  result.reserve(((len + 2) / 3) * 4);
  for (size_t i = 0; i < len; i += 3) {
    int n = (i + 2 < len) ? 3 : (len - i);
    unsigned int val = static_cast<unsigned int>(data[i]) << 16;
    if (n > 1)
      val |= static_cast<unsigned int>(data[i + 1]) << 8;
    if (n > 2)
      val |= static_cast<unsigned int>(data[i + 2]);
    result += b64_chars[(val >> 18) & 0x3F];
    result += b64_chars[(val >> 12) & 0x3F];
    if (n > 1)
      result += b64_chars[(val >> 6) & 0x3F];
    else
      result += '=';
    if (n > 2)
      result += b64_chars[val & 0x3F];
    else
      result += '=';
  }
  return result;
}

mcp::JsonValue handle_capture_viewport(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "capture_editor_viewport called");

  std::string target = "editor";
  if (auto *target_p = args.Find("target")) {
    if (!target_p->IsString()) {
      return util::error_detail(
          "invalid target type", "capture_ops.cpp handle_capture_viewport",
          "'editor' or 'game'", "pass target as a string");
    }
    target = target_p->GetString();
  }

  if (target == "game") {
    int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
    if (auto *tp = args.Find("timeout_ms")) {
      if (tp->IsInt() && tp->GetInt() > 0)
        timeout_ms = tp->GetInt();
    }
    return runtime_ops::game_capture_blocking(timeout_ms);
  }
  if (target != "editor") {
    return util::error_detail("invalid target '" + target + "'",
                              "capture_ops.cpp handle_capture_viewport",
                              "'editor' or 'game'",
                              "pass target='editor' or target='game'");
  }

  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return util::error_json("EditorInterface not available");

  godot::Ref<godot::ViewportTexture> texture =
      usable_viewport_texture(editor->get_editor_viewport_2d());
  if (texture.is_null()) {
    texture = usable_viewport_texture(editor->get_editor_viewport_3d());
  }
  if (texture.is_null()) {
    return util::error_detail("editor viewport texture unavailable",
                              "capture_ops.cpp handle_capture_viewport",
                              "2D/3D editor viewport rendered",
                              "open a scene with a visible viewport first");
  }

  auto img = texture->get_image();

  godot::PackedByteArray png_buffer = img->save_png_to_buffer();
  if (png_buffer.size() == 0)
    return util::error_json("PNG encoding returned empty buffer");

  std::string b64 =
      base64_encode(png_buffer.ptr(), static_cast<size_t>(png_buffer.size()));

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["data"] = mcp::JsonValue(b64);
  inner["format"] = mcp::JsonValue("png");
  inner["width"] = mcp::JsonValue(img->get_width());
  inner["height"] = mcp::JsonValue(img->get_height());
  r["result"] = std::move(inner);

  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Tools,
      "capture_editor_viewport completed: " + std::to_string(img->get_width()) + "x" +
          std::to_string(img->get_height()));
  return r;
}

} // namespace capture_ops
} // namespace godot_autopilot
