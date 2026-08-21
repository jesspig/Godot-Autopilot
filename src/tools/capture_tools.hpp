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

GDA_TOOL_CLASS(CaptureEditorViewportTool, "capture_editor_viewport", "Capture the editor's 2D viewport as a PNG image and return it as base64. Use it to visually verify the scene while editing, e.g. after placing nodes or changing properties. The 2D viewport is preferred and falls back to the 3D viewport when unavailable. target only supports 'editor'; 'game' is not implemented, so use capture_game_viewport for the running game. Returns result with data, format, width and height fields.", "Capture", std::vector<std::string>({"capture","screenshot","viewport"}), ::godot_autopilot::capture_ops::handle_capture_viewport, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> tools;
  tools.push_back(std::make_unique<CaptureEditorViewportTool>());
  return tools;
}

} // namespace capture_tools
} // namespace godot_autopilot

#endif