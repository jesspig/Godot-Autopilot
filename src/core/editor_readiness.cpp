#include "editor_readiness.hpp"

#include <atomic>
#include <chrono>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>

namespace godot_autopilot {

namespace {

std::atomic<int64_t> &startup_mark_ms() {
  static std::atomic<int64_t> mark{0};
  return mark;
}

int64_t readiness_now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

} // namespace

void note_editor_startup() { startup_mark_ms().store(readiness_now_ms()); }

int64_t editor_startup_elapsed_ms() {
  const int64_t mark = startup_mark_ms().load();
  if (mark <= 0)
    return -1;
  return readiness_now_ms() - mark;
}

bool editor_in_startup_window() {
  auto *editor = godot::EditorInterface::get_singleton();
  const bool has_edited_root =
      editor != nullptr && editor->get_edited_scene_root() != nullptr;
  return is_startup_window(has_edited_root, editor_startup_elapsed_ms());
}

godot_autopilot::util::JsonValue startup_in_progress_error() {
  godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
  e["error"] = godot_autopilot::util::JsonValue(
      "editor is still starting up and has not restored an editable scene "
      "yet; retry shortly");
  e["error_code"] = godot_autopilot::util::JsonValue("startup_in_progress");
  e["code"] = godot_autopilot::util::JsonValue("startup_in_progress");
  e["retryable"] = godot_autopilot::util::JsonValue(true);
  e["retry_after_ms"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(1000));
  e["hint"] = godot_autopilot::util::JsonValue(
      "the editor startup window is still open; wait for the scene to finish "
      "restoring, then call the tool again");
  return e;
}

bool is_import_in_progress() {
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return false;
  auto *efs = editor->get_resource_filesystem();
  if (!efs)
    return false;
  if (efs->is_scanning())
    return true;
  if (efs->has_method("is_importing"))
    return efs->call("is_importing");
  return false;
}

godot_autopilot::util::JsonValue busy_error() {
  godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
  e["error"] = godot_autopilot::util::JsonValue(
      "editor is currently importing/scanning resources; retry shortly");
  e["retryable"] = godot_autopilot::util::JsonValue(true);
  e["retry_after_ms"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(500));
  return e;
}

} // namespace godot_autopilot
