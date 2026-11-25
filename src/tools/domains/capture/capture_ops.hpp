#pragma once
#include <cstdint>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include "util/json_value.hpp"
#include <string>

namespace godot_autopilot {
namespace capture_ops {

inline constexpr int kCaptureKeepFiles = 20;

struct CaptureSaveOptions {
  bool save = false;
  std::string save_dir;
};

godot_autopilot::util::JsonValue handle_capture_viewport(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_review_scene(const godot_autopilot::util::JsonValue &args);

bool viewport_capture_is_degenerate(int image_width, int image_height,
                                    int viewport_width, int viewport_height);

std::string base64_encode(const uint8_t *data, size_t len);

void prune_capture_files(const godot::String &dir_path, int keep);

bool read_capture_save_options(const godot_autopilot::util::JsonValue &args,
                               CaptureSaveOptions *out,
                               godot_autopilot::util::JsonValue *error_out);

bool resolve_capture_save_dir(const std::string &requested,
                              std::string *absolute_out,
                              std::string *error_out);

godot_autopilot::util::JsonValue save_capture_image(const godot::Ref<godot::Image> &image,
                                  const CaptureSaveOptions &options);

godot_autopilot::util::JsonValue save_capture_bytes(const godot::PackedByteArray &bytes,
                                  const CaptureSaveOptions &options);

godot_autopilot::util::JsonValue with_capture_save_options(godot_autopilot::util::JsonValue pending,
                                         const CaptureSaveOptions &options);

bool is_pending_subcall_response(const godot_autopilot::util::JsonValue &response,
                                 int64_t *out_request_id,
                                 int64_t *out_timeout_ms);

godot_autopilot::util::JsonValue unwrap_subcall_section(const godot_autopilot::util::JsonValue &response,
                                      bool await_pending);

} // namespace capture_ops
} // namespace godot_autopilot
