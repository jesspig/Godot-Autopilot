#ifndef GODOT_AUTOPILOT_PROPERTY_TOOLS_HPP
#define GODOT_AUTOPILOT_PROPERTY_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/property_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace property_tools {

namespace {

const std::vector<ParamSpec> kPropertyGetParams = {
    {"path", "string", "Node path in the edited scene (string, e.g. 'Player' or 'Player/Camera2D')", true},
    {"property", "string", "Property name to read (string, e.g. 'position', 'visible')", true},
};

const std::vector<ParamSpec> kPropertySetParams = {
    {"path", "string", "Node path (scene-relative or absolute). memory:// is a resource namespace and is NOT valid here — memory resources go in the value parameter (e.g. {\"resource\": \"memory://name\"}). The response includes scene_path and scene_unsaved so the caller can confirm which scene was written", true},
    {"property", "string", "Single property name to set, dot-separated paths allowed (e.g. 'position', 'material.albedo_color'). Mutually exclusive with 'properties'; required in single form", false},
    {"value", "object", "Single property value. Resource references use two forms: {\"path\": \"res://xxx.tres\"} or {\"resource\": \"memory://name\"} (memory rejected on nodes). Dictionary properties take a JSON object; SpriteFrames 'animations' accepts an array or an object mapping names to {frames, speed, loop}. Required in single form", false},
    {"properties", "object", "Batch map of property names to values applied in one call (e.g. {\"position\": {\"x\": 1, \"y\": 2}, \"visible\": false}). Object-typed entries run first, then the rest, mirroring create_scene_node; mutually exclusive with property/value; max 32 entries", false},
    {"type_hint", "string", "Variant type hint for single form only (e.g. Vector2, Color, int, float). Omit to infer from metadata; must not be used with 'properties'", false},
};

const std::vector<ParamSpec> kPropertyGetListParams = {
    {"path", "string", "Node path in the edited scene (string, e.g. 'Player')", true},
    {"only_script_variables", "boolean", "Only return properties with PROPERTY_USAGE_SCRIPT_VARIABLE (usage bit 4096), i.e. script-declared variables (default: false)", false},
    {"property_filter", "string", "Case-sensitive substring filter on property names (default: empty = no filter)", false},
};

const std::vector<ParamSpec> kSignalConnectParams = {
    {"source_path", "string", "Node path of the signal emitter (string, e.g. 'Player')", true},
    {"signal", "string", "Signal name on the source node (string, e.g. 'health_changed')", true},
    {"target_path", "string", "Node path of the receiver (string, e.g. 'UI/HealthBar')", true},
    {"method", "string", "Method name to call on the target node (string, e.g. 'set_health')", true},
    {"persist", "boolean", "Persist the connection with CONNECT_PERSIST so it is saved with the scene and inherited by instantiations (boolean, default: true); false creates a runtime-only connection that is not saved", false},
};

const std::vector<ParamSpec> kSignalDisconnectParams = {
    {"source_path", "string", "Node path of the signal emitter (string, e.g. 'Player')", true},
    {"signal", "string", "Signal name on the source node (string, e.g. 'health_changed')", true},
    {"target_path", "string", "Node path of the receiver (string, e.g. 'UI/HealthBar')", true},
    {"method", "string", "Method name of the connected callable (string, e.g. 'set_health')", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(5);
  v.push_back(make_spec_tool(ToolSpec{
      "property_get",
      "Read a single property value from a scene node. Requires 'path' and 'property'; errors with candidate suggestions when the node or property does not exist (use property_get_list to see valid names). Returns the exact serialized value — unlike get_scene_tree's include_properties summary, which returns up to 20 filtered properties in one snapshot. Optional 'properties' (array of 1-32 non-empty names, mutually exclusive with 'property') reads many properties in one round trip and returns {name: value} plus 'missing' for absent names instead of failing the batch.",
      "Properties", {"property", "get"}, SideEffect::None, tool_flags::kNone,
      kPropertyGetParams, property_ops::handle_get}));
  v.push_back(make_spec_tool(ToolSpec{
      "property_set",
      "Set properties on a scene node. Single form requires 'path', 'property' and 'value'; batch form passes 'properties' as an object map instead (mutually exclusive, max 32, object-typed entries first then the rest, mirroring create_scene_node). Property names accept dot paths (a.b.c) into sub-resources with full-path errors and candidates. Omit 'type_hint' to infer the Variant type from metadata (single form only). Resource values resolve via {\"path\": \"res://...\"}; object-typed properties accept inline {\"type\": \"RectangleShape2D\", \"properties\": {...}} (nesting limit 4, echoed in 'inline_resources'). Dictionary properties take JSON objects; SpriteFrames 'animations' accepts an engine array or a dict mapping names to {frames, speed, loop} assembled into the engine array form. memory:// rejected on nodes. Returns 'ok' with undo info and editor undo registration (undoable:false for object values).",
      "Properties", {"property", "set"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kPropertySetParams, property_ops::handle_set}));
  v.push_back(make_spec_tool(ToolSpec{
      "property_get_list",
      "List every property of a scene node with its metadata. Requires 'path'; returns an array of entries with name, type (Variant type name and id), hint (property hint name), hint_string, usage flags and class_name. Use it to discover valid property names and enum ordering before property_get or property_set.",
      "Properties", {"property", "list"}, SideEffect::None, tool_flags::kNone,
      kPropertyGetListParams, property_ops::handle_get_list}));
  v.push_back(make_spec_tool(ToolSpec{
      "signal_connect",
      "Connect a signal on 'source_path' to a method on 'target_path'; requires 'source_path', 'signal', 'target_path', 'method'. 'persist' (default true) uses CONNECT_PERSIST, saving the connection with the scene and making every instantiation inherit it; set false for a runtime-only connection that is not saved. Returns 'connected', or 'already_connected' when the pair already exists (use signal_disconnect to remove it), plus 'persisted' and a note.",
      "Properties", {"signal", "connect"}, SideEffect::None, tool_flags::kNone,
      kSignalConnectParams, property_ops::handle_signal_connect}));
  v.push_back(make_spec_tool(ToolSpec{
      "signal_disconnect",
      "Remove a signal connection previously created by signal_connect. Requires the same four parameters: 'source_path', 'signal', 'target_path', 'method'. Returns 'disconnected', or 'not_connected' when the signal+callable pair does not exist (idempotent — repeated calls are safe). Persistent connections removed here stop being saved with the scene.",
      "Properties", {"signal", "disconnect"}, SideEffect::None, tool_flags::kNone,
      kSignalDisconnectParams, property_ops::handle_signal_disconnect}));
  return v;
}

} // namespace property_tools
} // namespace godot_autopilot

#endif