#include "editor_readiness.hpp"

#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>

namespace godot_autopilot {

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

mcp::JsonValue busy_error() {
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue(
      "editor is currently importing/scanning resources; retry shortly");
  e["retryable"] = mcp::JsonValue(true);
  e["retry_after_ms"] = mcp::JsonValue(static_cast<int64_t>(500));
  return e;
}

} // namespace godot_autopilot
