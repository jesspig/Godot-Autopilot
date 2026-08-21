#ifndef GODOT_AUTOPILOT_SPRITEFRAMES_TOOLS_HPP
#define GODOT_AUTOPILOT_SPRITEFRAMES_TOOLS_HPP

#include "tools/tool_decl.hpp"
#include "tools/spriteframes_ops.hpp"

#include <memory>
#include <string>
#include <vector>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace spriteframes_tools {

GDA_TOOL_CLASS(CreateSpriteFramesTool, "create_spriteframes", "Create a SpriteFrames resource in memory and register it as memory://<name>; nothing is written to disk. As a side effect the built-in 'default' animation is removed, so add animations explicitly with add_spriteframes_animation before adding frames with add_spriteframes_frame. Returns class, name, path, object_id and default_animation_removed.", "SpriteFrames", std::vector<std::string>({"spriteframes","create"}), ::godot_autopilot::spriteframes_ops::handle_create, true)

GDA_TOOL_CLASS(AddSpriteFramesAnimationTool, "add_spriteframes_animation", "Add a named animation to a SpriteFrames resource created by create_spriteframes. Requires name (the memory:// reference) and animation; optional fps (default 5) and loop (default true) set playback behavior. Must run before add_spriteframes_frame targets the animation. The animation starts empty until frames are added. Returns 'ok'.", "SpriteFrames", std::vector<std::string>({"spriteframes","animation","add"}), ::godot_autopilot::spriteframes_ops::handle_add_animation, true)

GDA_TOOL_CLASS(AddSpriteFramesFrameTool, "add_spriteframes_frame", "Add one or more frames to an animation of a SpriteFrames resource. Requires name (memory:// reference), animation (created first with add_spriteframes_animation) and texture path; optional duration (default 1.0). hframes and vframes (default 1) split a sprite sheet into per-cell AtlasTextures. Returns 'ok', frames (total count) and added_frames.", "SpriteFrames", std::vector<std::string>({"spriteframes","frame","add"}), ::godot_autopilot::spriteframes_ops::handle_add_frame, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> tools;
  tools.push_back(std::make_unique<CreateSpriteFramesTool>());
  tools.push_back(std::make_unique<AddSpriteFramesAnimationTool>());
  tools.push_back(std::make_unique<AddSpriteFramesFrameTool>());
  return tools;
}

} // namespace spriteframes_tools
} // namespace godot_autopilot

#endif