#ifndef GODOT_AUTOPILOT_CAPTURE_TOOLS_HPP
#define GODOT_AUTOPILOT_CAPTURE_TOOLS_HPP

#include <tools/tool_spec.hpp>
#include "tools/capture_ops.hpp"

#include <memory>
#include <string>
#include <vector>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace capture_tools {

namespace {

const std::vector<ParamSpec> kCaptureEditorViewportParams = {
    {"target", "string", "Target to capture: 'editor' (default) grabs the editor 2D viewport with a fallback to the 3D viewport; 'game' captures the running game's root window over the runtime channel — requires a game launched from the editor whose project loads the godot-autopilot extension", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds for target='game' (default: 5000, max: 25000); ignored for target='editor'; when after_frames/when delay a game capture it also bounds the in-game wait and the game answers with a structured error (code when_timeout, plus frames_waited) on expiry. The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap — split long waits into shorter calls", false},
    {"save", "boolean", "Editor target only: when true the PNG is also written to user://godot_autopilot/captures/ and the result gains path; the captures folder keeps the 20 most recent files (default: false). Game-target captures do not need it: the game always writes the PNG to the OS cache as the transport mechanism before the editor reads it back and returns it inline", false},
    {"region", "object", "Applies to both targets: crop the captured image to {x, y, width, height} in pixels (width/height must be positive); the rect is clamped to the image and an empty intersection fails with region_out_of_bounds; with target='game' the crop is applied inside the game process; example: {x: 0, y: 0, width: 640, height: 480}", false},
    {"max_dimension", "integer", "Applies to both targets: downscale the output image so its longest side is at most this many pixels (64-4096); never upscales (default: no scaling); with target='game' the downscale is applied inside the game process. Applied last: region crop first, then scale, then this cap — when both are given max_dimension wins and width/height report the final size", false},
    {"scale", "integer", "Applies to both targets: integer nearest-neighbour upscale factor (1-8, default: 1) applied after the region crop and before max_dimension. The editor target returns the image inline and only writes a file when save=true; game-target captures are always written to the OS cache as the transport mechanism", false},
    {"after_frames", "integer", "target='game' only: wait this many rendered frames before capturing (integer >= 0, default: 0 = capture immediately); combined with when, the frame budget must be spent first. target='editor' rejects it because the editor capture runs synchronously on the main thread and cannot yield frames", false},
    {"when", "string", "target='game' only: GDScript expression evaluated once per rendered frame inside the game; the capture fires when it is true (empty string = no condition). It runs with the current scene as the base instance, so node lookups such as get_node(\"HUD/MessageLabel\").text != \"\" work directly. A malformed expression fails immediately with a structured error (code when_parse_error) carrying the Expression error text; a condition that never becomes true returns a structured error (code when_timeout) bounded by timeout_ms. target='editor' rejects it", false},
    {"space", "string", "Editor target only: 'viewport' (default) captures the editor 2D viewport with a 3D fallback; 'window' captures the editor main window root viewport exactly as shown on screen, including docks, toolbars and the 2D grid/selection overlays", false},
    {"diff_against_last", "boolean", "Editor target only: when true the result gains a diff object comparing the final image with the previous editor capture (comparable, changed_ratio, changed_bbox, or a reason when not comparable); when the two sizes differ the shared top-left region is compared instead and the diff additionally reports size_mismatch=true plus current_size {x, y} alongside baseline_size (default: false)", false},
    {"diff_image", "boolean", "Editor target only: when true with diff_against_last, the result additionally gains diff_image_data (base64 PNG) highlighting changed_bbox on a copy of the final image; requires diff_against_last, default false = numbers only; not returned when the diff is not comparable or has no changed_bbox; counts toward the PNG/JSON response limits (default: false)", false},
    {"annotate", "boolean", "Applies to both targets: when true the output image is annotated with numbered red boxes around UI controls and the result gains an elements array of {id, path, type, text, position, size} in final-image pixels whose ids match the box numbers (text is omitted when empty, elements_truncated marks a 200-element cap); the editor target marks editor UI controls (window-level controls with space='window'), while target='game' marks Controls of the running game, annotated inside the game process (default: false)", false},
    {"annotate_nodes", "array", "Applies to both targets: array of 1-50 scene node path strings to mark with numbered blue boxes (ids count independently from the red UI-control annotate boxes, starting at 1); the editor target resolves edited-scene paths while target='game' forwards the list for the game process to resolve absolute game paths; the result gains a node_elements array of {id, path, type, ok, position, size, visible, behind?, error?} in final-image pixels (one entry per input path, per-item failures carry ok:false plus error instead of failing the call; behind marks 3D nodes behind the camera; visible:false marks skipped boxes) plus node_truncated when the shared 200-box drawing budget with annotate is exceeded; single-item errors: node not found, unsupported node type, viewport incompatible (default: absent = disabled)", false},
    {"annotate_nodes_max", "integer", "Optional self-imposed tighter cap (1-50, default: 50) on annotate_nodes length; the call fails when annotate_nodes holds more entries (default: absent = 50-entry server cap)", false},
};

const std::vector<ParamSpec> kReviewSceneVisuallyParams = {
    {"region", "object", "Crop both captures to {x, y, width, height} in pixels (width/height must be positive); clamped per image, empty intersection fails the review call with region_out_of_bounds", false},
    {"max_dimension", "integer", "Downscale both captures so the longest side is at most this many pixels (64-4096); never upscales", false},
    {"scale", "integer", "Integer nearest-neighbour upscale factor applied before max_dimension (1-8, default: 1)", false},
    {"annotate", "boolean", "Draw numbered red UI-control boxes on both captures (default: false)", false},
    {"annotate_nodes", "array", "Array of 1-50 scene node paths drawn as blue boxes on both captures and reused as the nodes table paths; per-item failures are isolated", false},
    {"annotate_nodes_max", "integer", "Tighter self-imposed cap on annotate_nodes length (1-50)", false},
    {"include_editor", "boolean", "Include the editor capture section (default: true); false returns skipped", false},
    {"include_game", "boolean", "Include the game capture section over the runtime channel (default: true); false returns skipped; game not running returns a per-section error", false},
    {"timeout_ms", "integer", "Positive timeout bounding only the game capture (default: 5000)", false},
    {"space", "string", "Editor capture space: 'viewport' (default) or 'window'", false},
    {"viewport", "string", "Editor mapping viewport: '2d' (default) or '3d'", false},
    {"index", "integer", "Editor mapping 3D viewport index (default: 0)", false},
    {"node_viewport", "string", "Nodes table viewport: 'auto' (default), '2d' or '3d'", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(2);
  v.push_back(make_spec_tool(ToolSpec{
      "capture_editor_viewport",
      "Capture a viewport as a PNG image and return it inline as base64 (image content). target='editor' (default) grabs the editor 2D viewport with a fallback to the 3D viewport; use it to visually verify the scene while editing, e.g. after placing nodes or changing properties. Optional 'space' picks what the editor target captures: 'viewport' (default) keeps the 2D viewport with 3D fallback behaviour, while 'window' grabs the editor main window root viewport exactly as shown on screen - docks, toolbars and the 2D grid/selection overlays included. The optional 'region' object {x, y, width, height} crops the captured image to that rectangle (width and height must be positive; the rect is clamped to the image, an empty intersection fails with code region_out_of_bounds), and optional 'max_dimension' (64-4096) downscales the result so its longest side is at most that many pixels; it never upscales. Both parameters apply to both targets: for target='game' the crop and downscale run inside the game process. They operate on the returned output image: the result reports the clamped 'region' when one was applied, source_width/source_height whenever the output was cropped or upscaled/downscaled, and width/height (plus 'space' on the editor target) for the final image. Optional 'scale' (integer 1-8, default 1) nearest-neighbour upscales the image after the crop and before 'max_dimension', so the cap still wins when both are given and width/height report the final size. For target='game', optional 'after_frames' (integer >= 0) waits that many rendered frames and optional 'when' (a GDScript expression re-evaluated every rendered frame against the current scene) holds the capture until the expression is true - together they capture short-lived feedback such as a 2 s banner or a 0.7 s death screen without slowing the game: act first, then capture with a condition; a malformed expression fails immediately with a structured error (code when_parse_error) carrying the Expression error text, and a condition that never becomes true ends in a structured error (code when_timeout, plus frames_waited) bounded by 'timeout_ms' instead of hanging. target='editor' rejects 'after_frames'/'when' because the editor capture runs synchronously on the main thread. Optional 'diff_against_last' (default false) adds a 'diff' object comparing the final image with the previous editor capture: comparable plus changed_ratio and, when anything changed, changed_bbox; when the two sizes differ it compares the shared top-left region instead and additionally reports size_mismatch=true and current_size {x, y} next to baseline_size; when the images cannot be compared it returns comparable=false with a reason (no_baseline, unsupported_format or baseline_too_large). Optional 'annotate' (default false) draws numbered red boxes around UI controls and adds an 'elements' array (id, path, type, text, position, size) in final-image pixels whose ids match the box numbers, plus 'elements_truncated' when the 200-element cap was hit. It applies to both targets: the editor target annotates editor UI controls (window-level controls with space='window'), while target='game' annotates Controls of the running game inside the game process and returns the same elements table. Editor captures are not written to disk by default; pass save=true to also write the PNG under user://godot_autopilot/captures/ and receive its 'path' in the result. target='game' captures the running game's root window over the runtime channel and returns data, format, width and height plus the absolute path of the PNG in the OS cache directory, along with region/source_width/source_height when the game applied them; it requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension. Both targets keep only the 20 most recent gda_capture PNG files. Optional 'timeout_ms' applies to target='game' only (default: 5000, max: 30000).",
      "Capture", {"capture", "screenshot", "viewport"}, SideEffect::None, tool_flags::kCaptureImage,
      kCaptureEditorViewportParams, ::godot_autopilot::capture_ops::handle_capture_viewport}));
  v.push_back(make_spec_tool(ToolSpec{
      "review_scene_visually",
      "Review the edited scene and running game in one read-only call for visual scene design: returns editor_capture (editor viewport screenshot with optional annotate/annotate_nodes), game_capture (running game screenshot over the runtime channel), nodes (get_scene_node_screen_rect table for annotate_nodes paths) and mapping (get_editor_viewport_geometry window=offset+image*scale). Each section fails in isolation with an error object instead of failing the call; pass include_editor/include_game=false to skip a capture, omit annotate_nodes to skip the nodes table (nodes reports skipped). Shares region/max_dimension(64-4096)/scale(1-8)/annotate/annotate_nodes(1-50)/annotate_nodes_max semantics with the capture tools; space/viewport/index control the editor mapping; node_viewport(auto/2d/3d) controls the nodes table; timeout_ms bounds only the game capture.",
      "Capture", {"capture", "screenshot", "review", "scene"}, SideEffect::None, tool_flags::kNone,
      kReviewSceneVisuallyParams, ::godot_autopilot::capture_ops::handle_review_scene}));
  return v;
}

} // namespace capture_tools
} // namespace godot_autopilot

#endif