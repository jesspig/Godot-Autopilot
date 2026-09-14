#include "capture_ops.hpp"
#include "../util/error_util.hpp"
#include "core/config.hpp"
#include "core/log_system.hpp"
#include "tools/runtime_ops.hpp"
#include <algorithm>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace capture_ops {

namespace {

mcp::JsonValue capture_limit_error(const std::string &code,
                                   const std::string &message) {
  mcp::JsonValue error = util::error_json(message);
  mcp::JsonValue details(mcp::JsonValue::object_tag);
  details["code"] = mcp::JsonValue(code);
  error["structured_error"] = std::move(details);
  return error;
}

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

void prune_capture_files(const godot::String &dir_path, int keep) {
  if (keep <= 0)
    return;
  godot::Ref<godot::DirAccess> dir = godot::DirAccess::open(dir_path);
  if (dir.is_null())
    return;
  if (dir->list_dir_begin() != godot::OK)
    return;

  std::vector<std::pair<uint64_t, godot::String>> files;
  godot::String entry = dir->get_next();
  while (entry != godot::String()) {
    if (entry != "." && entry != ".." && !dir->current_is_dir() &&
        entry.begins_with("gda_capture") && entry.ends_with(".png")) {
      godot::String full_path = dir_path.path_join(entry);
      files.emplace_back(godot::FileAccess::get_modified_time(full_path), entry);
    }
    entry = dir->get_next();
  }
  dir->list_dir_end();

  if (files.size() <= static_cast<size_t>(keep))
    return;

  std::sort(files.begin(), files.end(),
            [](const std::pair<uint64_t, godot::String> &a,
               const std::pair<uint64_t, godot::String> &b) {
              if (a.first != b.first)
                return a.first > b.first;
              return a.second > b.second;
            });

  for (size_t i = static_cast<size_t>(keep); i < files.size(); ++i) {
    godot::Error err =
        godot::DirAccess::remove_absolute(dir_path.path_join(files[i].second));
    if (err != godot::OK) {
      LogSystem::instance().log(
          LogLevel::Warning, LogCategory::Tools,
          "prune_capture_files: failed to remove " +
              util::to_std(files[i].second));
    }
  }
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

  bool save = false;
  if (auto *save_p = args.Find("save")) {
    if (!save_p->IsBool()) {
      return util::error_json("invalid parameter: save must be a boolean");
    }
    save = save_p->GetBool();
  }

  if (target == "game") {
    int64_t timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
    if (auto *tp = args.Find("timeout_ms")) {
      if (tp->IsInt() && tp->GetInt() > 0)
        timeout_ms = tp->GetInt();
    }
    if (timeout_ms > GDA_MAX_TIMEOUT_MS)
      timeout_ms = GDA_MAX_TIMEOUT_MS;
    return runtime_ops::handle_gda_send(
        "capture", mcp::JsonValue(mcp::JsonValue::object_tag), timeout_ms);
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

  if (img->get_width() > GDA_CAPTURE_MAX_DIMENSION ||
      img->get_height() > GDA_CAPTURE_MAX_DIMENSION) {
    return capture_limit_error(
        "capture_dimensions_exceeded",
        "viewport dimensions exceed the capture limit of " +
            std::to_string(GDA_CAPTURE_MAX_DIMENSION) + " pixels per side");
  }

  godot::PackedByteArray png_buffer = img->save_png_to_buffer();
  if (png_buffer.size() == 0)
    return util::error_json("PNG encoding returned empty buffer");
  if (static_cast<size_t>(png_buffer.size()) > GDA_CAPTURE_MAX_PNG_BYTES) {
    return capture_limit_error(
        "capture_bytes_exceeded",
        "encoded PNG exceeds the capture limit of " +
            std::to_string(GDA_CAPTURE_MAX_PNG_BYTES) + " bytes");
  }

  std::string saved_path;
  if (save) {
    const godot::String capture_dir = "user://godot_autopilot/captures";
    godot::DirAccess::make_dir_recursive_absolute(capture_dir);
    const godot::String capture_path =
        capture_dir + godot::String("/gda_capture_editor_") +
        godot::String(std::to_string(
                          godot::Time::get_singleton()->get_ticks_msec())
                          .c_str()) +
        godot::String(".png");
    godot::Error save_err = img->save_png(capture_path);
    if (save_err != godot::OK) {
      return util::error_detail("failed to save editor capture",
                                "capture_ops.cpp handle_capture_viewport",
                                "writable user:// path",
                                "check that the user:// directory is writable");
    }
    prune_capture_files(capture_dir, 20);
    saved_path = util::to_std(capture_path);
  }

  std::string b64 =
      base64_encode(png_buffer.ptr(), static_cast<size_t>(png_buffer.size()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["data"] = mcp::JsonValue(b64);
  inner["format"] = mcp::JsonValue("png");
  inner["width"] = mcp::JsonValue(img->get_width());
  inner["height"] = mcp::JsonValue(img->get_height());
  if (save)
    inner["path"] = mcp::JsonValue(saved_path);
  r["result"] = std::move(inner);
  if (r.Dump().size() > GDA_MAX_JSON_RESPONSE_BYTES)
    return capture_limit_error(
        "response_too_large",
        "capture response exceeds the JSON response limit of " +
            std::to_string(GDA_MAX_JSON_RESPONSE_BYTES) + " bytes");

  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Tools,
      "capture_editor_viewport completed: " + std::to_string(img->get_width()) + "x" +
          std::to_string(img->get_height()));
  return r;
}

} // namespace capture_ops
} // namespace godot_autopilot
