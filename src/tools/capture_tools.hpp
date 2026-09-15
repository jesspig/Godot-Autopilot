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

GDA_TOOL_CLASS(CaptureEditorViewportTool, "capture_editor_viewport", "Capture a viewport as a PNG image and return it inline as base64 (image content). target='editor' (default) grabs the editor 2D viewport with a fallback to the 3D viewport; use it to visually verify the scene while editing, e.g. after placing nodes or changing properties. Optional 'space' picks what the editor target captures: 'viewport' (default) keeps the 2D viewport with 3D fallback behaviour, while 'window' grabs the editor main window root viewport exactly as shown on screen - docks, toolbars and the 2D grid/selection overlays included. The optional 'region' object {x, y, width, height} crops the captured image to that rectangle (width and height must be positive; the rect is clamped to the image, an empty intersection fails with code region_out_of_bounds), and optional 'max_dimension' (64-4096) downscales the result so its longest side is at most that many pixels; it never upscales. Both parameters apply to both targets: for target='game' the crop and downscale run inside the game process. They operate on the returned output image: the result reports the clamped 'region' when one was applied, source_width/source_height whenever the output was cropped or downscaled, and width/height (plus 'space' on the editor target) for the final image. Optional 'diff_against_last' (default false) adds a 'diff' object comparing the final image with the previous editor capture: comparable plus changed_ratio and, when anything changed, changed_bbox; when the two sizes differ it compares the shared top-left region instead and additionally reports size_mismatch=true and current_size {x, y} next to baseline_size; when the images cannot be compared it returns comparable=false with a reason (no_baseline, unsupported_format or baseline_too_large). Optional 'annotate' (default false) draws numbered red boxes around UI controls and adds an 'elements' array (id, path, type, text, position, size) in final-image pixels whose ids match the box numbers, plus 'elements_truncated' when the 200-element cap was hit. It applies to both targets: the editor target annotates editor UI controls (window-level controls with space='window'), while target='game' annotates Controls of the running game inside the game process and returns the same elements table. Editor captures are not written to disk by default; pass save=true to also write the PNG under user://godot_autopilot/captures/ and receive its 'path' in the result. target='game' captures the running game's root window over the runtime channel and returns data, format, width and height plus the absolute path of the PNG in the OS cache directory, along with region/source_width/source_height when the game applied them; it requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension. Both targets keep only the 20 most recent gda_capture PNG files. Optional 'timeout_ms' applies to target='game' only (default: 5000, max: 30000).", "Capture", std::vector<std::string>({"capture","screenshot","viewport"}), ::godot_autopilot::capture_ops::handle_capture_viewport, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> tools;
  tools.push_back(std::make_unique<CaptureEditorViewportTool>());
  return tools;
}

} // namespace capture_tools
} // namespace godot_autopilot

#endif