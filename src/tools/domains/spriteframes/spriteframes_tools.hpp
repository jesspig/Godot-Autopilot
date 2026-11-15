#ifndef GODOT_AUTOPILOT_SPRITEFRAMES_TOOLS_HPP
#define GODOT_AUTOPILOT_SPRITEFRAMES_TOOLS_HPP

#include <tools/base/tool_spec.hpp>
#include "tools/domains/spriteframes/spriteframes_ops.hpp"

#include <memory>
#include <string>
#include <vector>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace spriteframes_tools {

namespace {

const std::vector<ParamSpec> kUpdateSpriteframesParams = {
    {"name", "string", "Resource name used as the memory:// reference for later tools (e.g. 'hero_walk'); with action=create it is required, later actions reference the name created there", true},
    {"default_animation", "string", "Only with action=create: optional animation created immediately as an empty animation (engine-default fps/loop). Use 'default' to match the AnimatedSprite2D default animation property, so a node that never sets animation keeps a deterministic first animation in the editor instead of falling back to an arbitrary list entry; runtime playback still needs an explicit play() call or property_set on the node", false},
    {"animation", "string", "Only with action=add_animation or action=add_frame: animation name to add (e.g. 'walk', 'idle') or the animation receiving the frame (created with action=add_animation first)", false},
    {"fps", "number", "Only with action=add_animation: animation playback speed in frames per second (default: 5)", false},
    {"loop", "boolean", "Only with action=add_animation: loop the animation (default: true)", false},
    {"texture", "string", "Only with action=add_frame: texture file path (e.g. res://frame.png); must exist and import as Texture2D", false},
    {"duration", "number", "Only with action=add_frame: frame duration in seconds (default: 1.0)", false},
    {"hframes", "integer", "Only with action=add_frame: horizontal frame count for spritesheet splitting (default: 1; must be >= 1)", false},
    {"vframes", "integer", "Only with action=add_frame: vertical frame count for spritesheet splitting (default: 1; must be >= 1)", false},
};

const std::vector<std::string> kUpdateSpriteframesActions = {
    "create", "add_animation", "add_frame"};

inline mcp::JsonValue spriteframes_action_args(const mcp::JsonValue &args,
                                               const char *action) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["action"] = mcp::JsonValue(action);
  if (!args.IsObject()) {
    return out;
  }
  for (const auto &entry : args.GetObject()) {
    if (entry.first != "action" && entry.first != "confirm") {
      out[entry.first] = entry.second;
    }
  }
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(1);
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "update_spriteframes",
      "Build a SpriteFrames resource in memory and register it as memory://<name>; nothing is written to disk. action selects the operation: 'create' registers a new resource and, as a side effect, removes the built-in 'default' animation so later animations are explicit (optional default_animation creates one empty animation immediately); 'add_animation' appends a named animation with optional fps (default 5) and loop (default true) and must run before add_frame targets it; 'add_frame' adds one or more frames to an existing animation (requires animation and texture path; optional duration default 1.0, and hframes/vframes default 1 split a sprite sheet into per-cell AtlasTextures). Returns create: class, name, path, object_id and default_animation_removed; add_animation: 'ok'; add_frame: 'ok', frames (total count) and added_frames. Example: {\"action\": \"create\", \"name\": \"hero_walk\"} or {\"action\": \"add_frame\", \"name\": \"hero_walk\", \"animation\": \"walk\", \"texture\": \"res://frame.png\"}.",
      "SpriteFrames", {"spriteframes", "create", "animation", "frame", "atlas"},
      SideEffect::None, tool_flags::kNone,
      kUpdateSpriteframesParams, spriteframes_ops::handle_update_spriteframes,
      schema::build_action_schema(
          kUpdateSpriteframesActions, kUpdateSpriteframesParams,
          "create: register a new in-memory SpriteFrames; add_animation: append a named animation; add_frame: add texture frames to an animation"),
      {},
      {"update_spriteframes action=create name=hero_walk",
       "update_spriteframes action=add_animation name=hero_walk animation=walk fps=8"}},
      {
          ToolAlias{
              "create_spriteframes", "update_spriteframes",
              [](const mcp::JsonValue &args) {
                return spriteframes_action_args(args, "create");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "add_spriteframes_animation", "update_spriteframes",
              [](const mcp::JsonValue &args) {
                return spriteframes_action_args(args, "add_animation");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "add_spriteframes_frame", "update_spriteframes",
              [](const mcp::JsonValue &args) {
                return spriteframes_action_args(args, "add_frame");
              },
              {}, "0.4.0", "0.6.0"},
      })));
  return v;
}

} // namespace spriteframes_tools
} // namespace godot_autopilot

#endif
