#ifndef GODOT_AUTOPILOT_EDITOR_READINESS_HPP
#define GODOT_AUTOPILOT_EDITOR_READINESS_HPP

#include <cstdint>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {

constexpr int64_t kEditorStartupWindowMs = 5000;

bool is_import_in_progress();

mcp::JsonValue busy_error();

void note_editor_startup();

int64_t editor_startup_elapsed_ms();

inline bool is_startup_window(bool has_edited_root, int64_t startup_elapsed_ms) {
  return !has_edited_root && startup_elapsed_ms >= 0 &&
         startup_elapsed_ms < kEditorStartupWindowMs;
}

bool editor_in_startup_window();

mcp::JsonValue startup_in_progress_error();

inline mcp::JsonValue startup_gated_error(bool in_startup_window,
                                          const mcp::JsonValue &normal_error) {
  return in_startup_window ? startup_in_progress_error() : normal_error;
}

} // namespace godot_autopilot

#endif
