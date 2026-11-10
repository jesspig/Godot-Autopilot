#pragma once
#include <cstdint>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace capture_ops {

inline constexpr int kCaptureKeepFiles = 20;

struct CaptureSaveOptions {
  bool save = false;
  std::string save_dir;
};

mcp::JsonValue handle_capture_viewport(const mcp::JsonValue &args);
mcp::JsonValue handle_review_scene(const mcp::JsonValue &args);

std::string base64_encode(const uint8_t *data, size_t len);

void prune_capture_files(const godot::String &dir_path, int keep);

bool read_capture_save_options(const mcp::JsonValue &args,
                               CaptureSaveOptions *out,
                               mcp::JsonValue *error_out);

bool resolve_capture_save_dir(const std::string &requested,
                              std::string *absolute_out,
                              std::string *error_out);

mcp::JsonValue save_capture_image(const godot::Ref<godot::Image> &image,
                                  const CaptureSaveOptions &options);

mcp::JsonValue save_capture_bytes(const godot::PackedByteArray &bytes,
                                  const CaptureSaveOptions &options);

mcp::JsonValue with_capture_save_options(mcp::JsonValue pending,
                                         const CaptureSaveOptions &options);

} // namespace capture_ops
} // namespace godot_autopilot
