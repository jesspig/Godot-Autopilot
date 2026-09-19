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
    {"property", "string", "Property name to set (string, e.g. 'position', 'modulate')", true},
    {"value", "object", "Property value to set. Resource references use two forms: {\"path\": \"res://xxx.tres\"} for a disk resource assigned to a node property, or {\"resource\": \"memory://name\"} for an in-memory resource (only valid in resource editing scenarios). Assigning a memory:// resource to a node property is rejected because it would corrupt the saved scene file", true},
    {"type_hint", "string", "Variant type hint (string, e.g. Vector2, Color, int, float). Omit to infer the type automatically from the property metadata", false},
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
      "Set a property on a scene node. Requires 'path', 'property' and 'value'. Omit 'type_hint' to infer the Variant type automatically from the property metadata (query property_get_list first for enum ordering). Resource values resolve via {\"path\": \"res://...\"}; an object-typed property also accepts an inline resource description {\"type\": \"RectangleShape2D\", \"properties\": {\"size\": {\"x\": 20, \"y\": 28}}} which creates a pathless sub-resource in one step (saved as a sub_resource with the scene, nesting limit 4) and is echoed in 'inline_resources' as {property, type} entries. memory:// resources are rejected to protect the scene file. Read-only or invalid assignments error; returns 'ok' with an 'undo' entry (old value) and registers the change with the editor undo stack so Ctrl-Z reverts it (undoable:true), except when the old or new value is an Object reference — then no action is recorded and the response reports undoable:false with undo_skip_reason.",
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