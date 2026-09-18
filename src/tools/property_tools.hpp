#ifndef GODOT_AUTOPILOT_PROPERTY_TOOLS_HPP
#define GODOT_AUTOPILOT_PROPERTY_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/property_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace property_tools {

GDA_TOOL_CLASS(PropertyGetTool, "property_get",
               "Read a single property value from a scene node. Requires 'path' and 'property'; errors with candidate suggestions when the node or property does not exist (use property_get_list to see valid names). Returns the exact serialized value — unlike get_scene_tree's include_properties summary, which returns up to 20 filtered properties in one snapshot. Optional 'properties' (array of 1-32 non-empty names, mutually exclusive with 'property') reads many properties in one round trip and returns {name: value} plus 'missing' for absent names instead of failing the batch.",
               "Properties", std::vector<std::string>({"property", "get"}), property_ops::handle_get, true)

GDA_TOOL_CLASS(PropertySetTool, "property_set",
               "Set a property on a scene node. Requires 'path', 'property' and 'value'. Omit 'type_hint' to infer the Variant type automatically from the property metadata (query property_get_list first for enum ordering). Resource values resolve via {\"path\": \"res://...\"}; an object-typed property also accepts an inline resource description {\"type\": \"RectangleShape2D\", \"properties\": {\"size\": {\"x\": 20, \"y\": 28}}} which creates a pathless sub-resource in one step (saved as a sub_resource with the scene, nesting limit 4) and is echoed in 'inline_resources' as {property, type} entries. memory:// resources are rejected to protect the scene file. Read-only or invalid assignments error; returns 'ok' with an 'undo' entry (old value) and registers the change with the editor undo stack so Ctrl-Z reverts it (undoable:true), except when the old or new value is an Object reference — then no action is recorded and the response reports undoable:false with undo_skip_reason.",
               "Properties", std::vector<std::string>({"property", "set"}), property_ops::handle_set, true)

GDA_TOOL_CLASS(PropertyGetListTool, "property_get_list",
               "List every property of a scene node with its metadata. Requires 'path'; returns an array of entries with name, type (Variant type name and id), hint (property hint name), hint_string, usage flags and class_name. Use it to discover valid property names and enum ordering before property_get or property_set.",
               "Properties", std::vector<std::string>({"property", "list"}), property_ops::handle_get_list, true)

GDA_TOOL_CLASS(SignalConnectTool, "signal_connect",
               "Connect a signal on 'source_path' to a method on 'target_path'; requires 'source_path', 'signal', 'target_path', 'method'. 'persist' (default true) uses CONNECT_PERSIST, saving the connection with the scene and making every instantiation inherit it; set false for a runtime-only connection that is not saved. Returns 'connected', or 'already_connected' when the pair already exists (use signal_disconnect to remove it), plus 'persisted' and a note.",
               "Properties", std::vector<std::string>({"signal", "connect"}), property_ops::handle_signal_connect, true)

GDA_TOOL_CLASS(SignalDisconnectTool, "signal_disconnect",
               "Remove a signal connection previously created by signal_connect. Requires the same four parameters: 'source_path', 'signal', 'target_path', 'method'. Returns 'disconnected', or 'not_connected' when the signal+callable pair does not exist (idempotent — repeated calls are safe). Persistent connections removed here stop being saved with the scene.",
               "Properties", std::vector<std::string>({"signal", "disconnect"}), property_ops::handle_signal_disconnect, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(5);
  v.push_back(std::make_unique<PropertyGetTool>());
  v.push_back(std::make_unique<PropertySetTool>());
  v.push_back(std::make_unique<PropertyGetListTool>());
  v.push_back(std::make_unique<SignalConnectTool>());
  v.push_back(std::make_unique<SignalDisconnectTool>());
  return v;
}

} // namespace property_tools
} // namespace godot_autopilot

#endif