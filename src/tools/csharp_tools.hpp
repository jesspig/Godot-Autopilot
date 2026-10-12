#ifndef GODOT_AUTOPILOT_CSHARP_TOOLS_HPP
#define GODOT_AUTOPILOT_CSHARP_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/csharp_bridge_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace csharp_tools {

namespace {

const std::vector<ParamSpec> kCSharpGetPropertyParams = {
    {"object_id", "integer", "Live object id from a previous result (editor process only); mutually exclusive with object_id_str, memory and path", false},
    {"object_id_str", "string", "Live object id as a decimal string for ids beyond 32-bit range (editor process only); mutually exclusive with object_id, memory and path", false},
    {"memory", "string", "Memory resource name registered via create_resource (editor process only); mutually exclusive with object_id, object_id_str and path", false},
    {"path", "string", "Scene node path in the edited scene (editor) or the running game (runtime=game), or a res:// resource path (editor only). memory:// is rejected here — pass the bare name in memory instead", false},
    {"property", "string", "C# member name to read: engine property, C# property or field (indexers are rejected by the bridge)", true},
    {"allow_non_public", "boolean", "Allow non-public C# members for this read (default false: only public engine properties, properties and fields resolve); when true the bridge also reads private and internal members, and the matching write path honors the same flag for writes", false},
    {"runtime", "string", "Target process: editor (default, resolve the locator in the editor and call the GdaCSharpBridge engine singleton) or game (forward to the game bridge over the runtime channel; only the scene-path locator form is accepted, needs a running .NET game with GdaCSharpRuntime)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds for runtime=game (default: 5000, max: 25000); rejected when runtime=editor", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready before failing when runtime=game (default: true); rejected when runtime=editor", false},
};

const std::vector<ParamSpec> kCSharpCallMethodParams = {
    {"object_id", "integer", "Live object id from a previous result (editor process only); mutually exclusive with object_id_str, memory and path", false},
    {"object_id_str", "string", "Live object id as a decimal string for ids beyond 32-bit range (editor process only); mutually exclusive with object_id, memory and path", false},
    {"memory", "string", "Memory resource name registered via create_resource (editor process only); mutually exclusive with object_id, object_id_str and path", false},
    {"path", "string", "Scene node path in the edited scene (editor) or the running game (runtime=game), or a res:// resource path (editor only). memory:// is rejected here — pass the bare name in memory instead", false},
    {"method", "string", "C# method name to call (params-array overloads are packed from trailing arguments by the bridge; overloads are matched by argument count with per-argument conversion)", true},
    {"args", "array", "Call arguments as a JSON array (max 16 entries); editor values deserialize with full Variant fidelity, game values cross as JSON-native values only", false},
    {"runtime", "string", "Target process: editor (default, resolve the locator in the editor and call the GdaCSharpBridge engine singleton) or game (forward to the game bridge over the runtime channel; only the scene-path locator form is accepted, needs a running .NET game with GdaCSharpRuntime)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds for runtime=game (default: 5000, max: 25000); rejected when runtime=editor", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready before failing when runtime=game (default: true); rejected when runtime=editor", false},
};

const std::vector<ParamSpec> kCSharpSubscribeEventParams = {
    {"object_id", "integer", "Live object id of the C# event source (editor process only); mutually exclusive with object_id_str, memory and path", false},
    {"object_id_str", "string", "Live object id of the C# event source as a decimal string (editor process only); mutually exclusive with object_id, memory and path", false},
    {"memory", "string", "Memory resource name of the C# event source (editor process only); mutually exclusive with object_id, object_id_str and path", false},
    {"path", "string", "Scene node path of the C# event source in the edited scene (editor) or the running game (runtime=game), or a res:// resource path (editor only)", false},
    {"event", "string", "C# event name on the source object (void-returning handlers with at most 4 parameters only)", true},
    {"target_object_id", "integer", "Live object id of the event receiver (editor process only); mutually exclusive with target_object_id_str, target_memory and target_path", false},
    {"target_object_id_str", "string", "Live object id of the event receiver as a decimal string (editor process only); mutually exclusive with target_object_id, target_memory and target_path", false},
    {"target_memory", "string", "Memory resource name of the event receiver (editor process only); mutually exclusive with target_object_id, target_object_id_str and target_path", false},
    {"target_path", "string", "Scene node path of the event receiver in the edited scene (editor) or the running game (runtime=game), or a res:// resource path (editor only)", false},
    {"method", "string", "Receiver method name invoked with the forwarded event arguments", true},
    {"runtime", "string", "Target process: editor (default, resolve both locators in the editor and call the GdaCSharpBridge engine singleton) or game (forward to the game bridge over the runtime channel; only scene-path locator forms are accepted, needs a running .NET game with GdaCSharpRuntime)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds for runtime=game (default: 5000, max: 25000); rejected when runtime=editor", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready before failing when runtime=game (default: true); rejected when runtime=editor", false},
};

const std::vector<ParamSpec> kCSharpSetPropertyParams = {
    {"object_id", "integer", "Live object id from a previous result (editor process only); mutually exclusive with object_id_str, memory and path", false},
    {"object_id_str", "string", "Live object id as a decimal string for ids beyond 32-bit range (editor process only); mutually exclusive with object_id, memory and path", false},
    {"memory", "string", "Memory resource name registered via create_resource (editor process only); mutually exclusive with object_id, object_id_str and path", false},
    {"path", "string", "Scene node path in the edited scene (editor) or the running game (runtime=game), or a res:// resource path (editor only). memory:// is rejected here — pass the bare name in memory instead", false},
    {"property", "string", "C# member name to write: engine property, C# property or field (indexers and read-only members are rejected by the bridge)", true},
    {"value", "object", "New member value as JSON; editor values deserialize with full Variant fidelity, game values cross as JSON-native values only", true},
    {"allow_non_public", "boolean", "Allow non-public C# members for this write (default false: only public engine properties, properties and fields resolve); when true the bridge also writes private and internal members, and the matching read path honors the same flag for reads", false},
    {"runtime", "string", "Target process: editor (default, resolve the locator in the editor and call the GdaCSharpBridge engine singleton) or game (forward to the game bridge over the runtime channel; only the scene-path locator form is accepted, needs a running .NET game with GdaCSharpRuntime)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds for runtime=game (default: 5000, max: 25000); rejected when runtime=editor", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready before failing when runtime=game (default: true); rejected when runtime=editor", false},
};

const std::vector<ParamSpec> kCSharpUnsubscribeEventParams = {
    {"token", "integer", "Subscription token returned in the result of csharp_subscribe_event on the same bridge instance (editor singleton or the running game bridge); tokens are scoped to one bridge instance, restarting the game drops every subscription", true},
    {"runtime", "string", "Target process: editor (default, release the subscription on the GdaCSharpBridge engine singleton) or game (forward to the game bridge over the runtime channel; needs a running .NET game with GdaCSharpRuntime)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds for runtime=game (default: 5000, max: 25000); rejected when runtime=editor", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready before failing when runtime=game (default: true); rejected when runtime=editor", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(5);
  v.push_back(make_spec_tool(ToolSpec{
      "csharp_get_property",
      "Read a C# member through the frozen GdaCSharpBridge ABI (get_prop): engine properties plus C# properties and fields that GDScript cannot see, including non-public members when the bridge allows them. The locator takes exactly one of object_id, object_id_str, memory or path. With runtime=editor (default) the object is resolved in the editor process and read via the GdaCSharpBridge engine singleton (requires a .NET editor build with the bridge registered). With runtime=game the scene-path locator is forwarded to the game bridge over the runtime channel (requires a running .NET game with GdaCSharpRuntime; object_id, object_id_str and memory locators are rejected there). Always needs the code_execute authorization gate, plus game_runtime when runtime=game. Returns the serialized member value.",
      "Scripts", {"csharp", "reflection", "property"}, SideEffect::CodeExecute, tool_flags::kNone,
      kCSharpGetPropertyParams, csharp_bridge_ops::handle_csharp_get}));
  v.push_back(make_spec_tool(ToolSpec{
      "csharp_call_method",
      "Call a C# method through the frozen GdaCSharpBridge ABI (call_method) with overload resolution by argument count. The locator takes exactly one of object_id, object_id_str, memory or path; args is a JSON array of at most 16 entries. With runtime=editor (default) the object is resolved in the editor process and invoked via the GdaCSharpBridge engine singleton (requires a .NET editor build with the bridge registered). With runtime=game the scene-path locator is forwarded to the game bridge over the runtime channel (requires a running .NET game with GdaCSharpRuntime; game-side args cross as JSON-native values only). Always needs the code_execute authorization gate, plus game_runtime when runtime=game. Returns the serialized return value.",
      "Scripts", {"csharp", "reflection", "method", "call"}, SideEffect::CodeExecute, tool_flags::kMutating,
      kCSharpCallMethodParams, csharp_bridge_ops::handle_csharp_call}));
  v.push_back(make_spec_tool(ToolSpec{
      "csharp_subscribe_event",
      "Subscribe a receiver method to a C# event through the frozen GdaCSharpBridge ABI (subscribe): event arguments are forwarded to the receiver method call. The source locator takes exactly one of object_id, object_id_str, memory or path, and the receiver locator takes exactly one of target_object_id, target_object_id_str, target_memory or target_path. With runtime=editor (default) both objects are resolved in the editor process and linked via the GdaCSharpBridge engine singleton (requires a .NET editor build with the bridge registered). With runtime=game both scene-path locators are forwarded to the game bridge over the runtime channel (requires a running .NET game with GdaCSharpRuntime). Always needs the code_execute authorization gate, plus game_runtime when runtime=game. Returns the subscription token issued by the bridge.",
      "Scripts", {"csharp", "reflection", "event", "subscribe"}, SideEffect::CodeExecute, tool_flags::kMutating,
      kCSharpSubscribeEventParams, csharp_bridge_ops::handle_csharp_subscribe}));
  v.push_back(make_spec_tool(ToolSpec{
      "csharp_set_property",
      "Write a C# member through the frozen GdaCSharpBridge ABI (set_prop): engine properties plus C# properties and fields that GDScript cannot see, including non-public members when allow_non_public is true (read-only members and indexers are rejected). The locator takes exactly one of object_id, object_id_str, memory or path, and value carries the new member value. With runtime=editor (default) the object is resolved in the editor process and written via the GdaCSharpBridge engine singleton (requires a .NET editor build with the bridge registered). With runtime=game the scene-path locator is forwarded to the game bridge over the runtime channel (requires a running .NET game with GdaCSharpRuntime; object_id, object_id_str and memory locators are rejected there; game-side values cross as JSON-native values only). Always needs the code_execute authorization gate, plus game_runtime when runtime=game. Returns ok on success.",
      "Scripts", {"csharp", "reflection", "property", "set"}, SideEffect::CodeExecute, tool_flags::kMutating,
      kCSharpSetPropertyParams, csharp_bridge_ops::handle_csharp_set}));
  v.push_back(make_spec_tool(ToolSpec{
      "csharp_unsubscribe_event",
      "Release a C# event subscription created by csharp_subscribe_event through the frozen GdaCSharpBridge ABI (unsubscribe) so the receiver stops getting event arguments. The token comes from the result of csharp_subscribe_event and is scoped to one bridge instance: with runtime=editor (default) the subscription is released on the GdaCSharpBridge engine singleton (requires a .NET editor build with the bridge registered), with runtime=game it is forwarded to the game bridge over the runtime channel (requires a running .NET game with GdaCSharpRuntime; restarting the game already drops every subscription). Always needs the code_execute authorization gate, plus game_runtime when runtime=game. Returns ok on success; unknown or already-released tokens are rejected.",
      "Scripts", {"csharp", "reflection", "event", "unsubscribe"}, SideEffect::CodeExecute, tool_flags::kMutating,
      kCSharpUnsubscribeEventParams, csharp_bridge_ops::handle_csharp_unsubscribe}));
  return v;
}

} // namespace csharp_tools
} // namespace godot_autopilot

#endif
