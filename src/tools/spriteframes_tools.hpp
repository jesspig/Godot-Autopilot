#ifndef GODOT_AUTOPILOT_SPRITEFRAMES_TOOLS_HPP
#define GODOT_AUTOPILOT_SPRITEFRAMES_TOOLS_HPP

#include <tools/tool_spec.hpp>
#include "tools/spriteframes_ops.hpp"

#include <memory>
#include <string>
#include <vector>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace spriteframes_tools {

namespace {

const std::vector<ParamSpec> kCreateSpriteframesParams = {
    {"name", "string", "Resource name used as the memory:// reference for later tools (e.g. 'hero_walk')", true},
    {"default_animation", "string", "Optional animation created immediately as an empty animation (engine-default fps/loop). Use 'default' to match the AnimatedSprite2D default animation property, so a node that never sets animation keeps a deterministic first animation in the editor instead of falling back to an arbitrary list entry; runtime playback still needs an explicit play() call or property_set on the node", false},
};

const std::vector<ParamSpec> kAddSpriteframesAnimationParams = {
    {"name", "string", "SpriteFrames name from create_spriteframes (memory:// reference)", true},
    {"animation", "string", "Animation name to add (e.g. 'walk', 'idle')", true},
    {"fps", "number", "Animation playback speed in frames per second (default: 5)", false},
    {"loop", "boolean", "Loop the animation (default: true)", false},
};

const std::vector<ParamSpec> kAddSpriteframesFrameParams = {
    {"name", "string", "SpriteFrames name from create_spriteframes (memory:// reference)", true},
    {"animation", "string", "Animation name to add the frame to (created with add_spriteframes_animation first)", true},
    {"texture", "string", "Texture file path (e.g. res://frame.png); must exist and import as Texture2D", true},
    {"duration", "number", "Frame duration in seconds (default: 1.0)", false},
    {"hframes", "integer", "Horizontal frame count for spritesheet splitting (default: 1; must be >= 1)", false},
    {"vframes", "integer", "Vertical frame count for spritesheet splitting (default: 1; must be >= 1)", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(3);
  v.push_back(make_spec_tool(ToolSpec{
      "create_spriteframes",
      "Create a SpriteFrames resource in memory and register it as memory://<name>; nothing is written to disk. As a side effect the built-in 'default' animation is removed, so add animations explicitly with add_spriteframes_animation before adding frames with add_spriteframes_frame. Returns class, name, path, object_id and default_animation_removed.",
      "SpriteFrames", {"spriteframes", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateSpriteframesParams, ::godot_autopilot::spriteframes_ops::handle_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_spriteframes_animation",
      "Add a named animation to a SpriteFrames resource created by create_spriteframes. Requires name (the memory:// reference) and animation; optional fps (default 5) and loop (default true) set playback behavior. Must run before add_spriteframes_frame targets the animation. The animation starts empty until frames are added. Returns 'ok'.",
      "SpriteFrames", {"spriteframes", "animation", "add"}, SideEffect::None, tool_flags::kNone,
      kAddSpriteframesAnimationParams, ::godot_autopilot::spriteframes_ops::handle_add_animation}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_spriteframes_frame",
      "Add one or more frames to an animation of a SpriteFrames resource. Requires name (memory:// reference), animation (created first with add_spriteframes_animation) and texture path; optional duration (default 1.0). hframes and vframes (default 1) split a sprite sheet into per-cell AtlasTextures. Returns 'ok', frames (total count) and added_frames.",
      "SpriteFrames", {"spriteframes", "frame", "add"}, SideEffect::None, tool_flags::kNone,
      kAddSpriteframesFrameParams, ::godot_autopilot::spriteframes_ops::handle_add_frame}));
  return v;
}

} // namespace spriteframes_tools
} // namespace godot_autopilot

#endif