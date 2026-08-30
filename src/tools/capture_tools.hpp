#ifndef GODOT_AUTOPILOT_CAPTURE_TOOLS_HPP
#define GODOT_AUTOPILOT_CAPTURE_TOOLS_HPP

#include "tools/tool_decl.hpp"
#include "tools/capture_ops.hpp"

#include <memory>
#include <string>
#include <vector>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace capture_tools {

GDA_TOOL_CLASS(CaptureEditorViewportTool, "capture_editor_viewport", "Capture a viewport as a PNG image and return it as base64. target='editor' (default) grabs the editor 2D viewport with a fallback to the 3D viewport; use it to visually verify the scene while editing, e.g. after placing nodes or changing properties. target='game' captures the running game's root window over the runtime channel and returns the same result shape (data, format, width, height plus the cache file path); it requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension. Optional 'timeout_ms' applies to target='game' only (default: 5000, max: 30000).", "Capture", std::vector<std::string>({"capture","screenshot","viewport"}), ::godot_autopilot::capture_ops::handle_capture_viewport, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> tools;
  tools.push_back(std::make_unique<CaptureEditorViewportTool>());
  return tools;
}

} // namespace capture_tools
} // namespace godot_autopilot

#endif