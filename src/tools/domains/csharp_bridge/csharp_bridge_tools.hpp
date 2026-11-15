#ifndef GODOT_AUTOPILOT_CSHARP_BRIDGE_TOOLS_HPP
#define GODOT_AUTOPILOT_CSHARP_BRIDGE_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/csharp_bridge/csharp_bridge_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace csharp_bridge_tools {

namespace {

const std::vector<ParamSpec> kCSharpMemberParams = {
    {"object_id", "integer", "Live object id from a previous result (editor process only); mutually exclusive with object_id_str, memory and path. Ignored by action=list, which reads Object reflection instead", false},
    {"object_id_str", "string", "Live object id as a decimal string for ids beyond 32-bit range (editor process only); mutually exclusive with object_id, memory and path", false},
    {"memory", "string", "Memory resource name registered via create_resource (editor process only); mutually exclusive with object_id, object_id_str and path", false},
    {"path", "string", "Scene node path in the edited scene (editor) or the running game (runtime=game), or a res:// resource path (editor only). memory:// is rejected here — pass the bare name in memory instead", false},
    {"property", "string", "Only with action=get or action=set: C# member name to read or write — engine property, C# property or field (indexers are rejected by the bridge)", false},
    {"value", "json", "Only with action=set: new member value as JSON; editor values deserialize with full Variant fidelity, game values cross as JSON-native values only", false},
    {"allow_non_public", "boolean", "Only with action=get or action=set: allow non-public C# members (default false: only public engine properties, properties and fields resolve); when true the bridge also reads or writes private and internal members", false},
    {"method", "string", "Only with action=call: C# method name to call (params-array overloads are packed from trailing arguments by the bridge; overloads are matched by argument count with per-argument conversion)", false},
    {"args", "array", "Only with action=call: call arguments as a JSON array (max 16 entries); editor values deserialize with full Variant fidelity, game values cross as JSON-native values only", false},
    {"kind", "string", "Only with action=list: member table to return — method, property, signal or all (default: all); tables excluded by kind come back as empty arrays with a zero count so counts stays readable", false},
    {"name_contains", "string", "Only with action=list: case-insensitive substring filter on member names, e.g. 'visible' or 'Get' (default: no filter); an empty string is rejected", false},
    {"limit", "integer", "Only with action=list: maximum number of members returned across the three tables, in methods then properties then signals order (integer 1-1000, default: 200); raise limit explicitly or page with offset when you need the full inherited table", false},
    {"offset", "integer", "Only with action=list: number of matched members to skip before applying limit, counted in the same methods then properties then signals order (default: 0); an offset past the end returns empty arrays with counts.total telling you how many existed", false},
    {"runtime", "string", "Target process: editor (default) or game (forward to the running game over the runtime channel; only scene-path locator forms are accepted, needs a running .NET game with GdaCSharpRuntime)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds for runtime=game (default: 5000, max: 25000); rejected when runtime=editor", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready before failing when runtime=game (default: true); rejected when runtime=editor", false},
};

const std::vector<ParamSpec> kCSharpEventParams = {
    {"object_id", "integer", "Live object id of the C# event source (editor process only); mutually exclusive with object_id_str, memory and path. Ignored by action=unsubscribe, which uses token", false},
    {"object_id_str", "string", "Live object id of the C# event source as a decimal string (editor process only); mutually exclusive with object_id, memory and path", false},
    {"memory", "string", "Memory resource name of the C# event source (editor process only); mutually exclusive with object_id, object_id_str and path", false},
    {"path", "string", "Scene node path of the C# event source in the edited scene (editor) or the running game (runtime=game), or a res:// resource path (editor only)", false},
    {"event", "string", "Only with action=subscribe: C# event name on the source object (void-returning handlers with at most 4 parameters only)", false},
    {"target_object_id", "integer", "Only with action=subscribe: live object id of the event receiver (editor process only); mutually exclusive with target_object_id_str, target_memory and target_path", false},
    {"target_object_id_str", "string", "Only with action=subscribe: live object id of the event receiver as a decimal string (editor process only); mutually exclusive with target_object_id, target_memory and target_path", false},
    {"target_memory", "string", "Only with action=subscribe: memory resource name of the event receiver (editor process only); mutually exclusive with target_object_id, target_object_id_str and target_path", false},
    {"target_path", "string", "Only with action=subscribe: scene node path of the event receiver in the edited scene (editor) or the running game (runtime=game), or a res:// resource path (editor only)", false},
    {"method", "string", "Only with action=subscribe: receiver method name invoked with the forwarded event arguments", false},
    {"token", "integer", "Only with action=unsubscribe: subscription token returned in the result of action=subscribe on the same bridge instance (editor singleton or the running game bridge); tokens are scoped to one bridge instance, restarting the game drops every subscription", false},
    {"runtime", "string", "Target process: editor (default) or game (forward to the running game over the runtime channel; only scene-path locator forms are accepted, needs a running .NET game with GdaCSharpRuntime)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds for runtime=game (default: 5000, max: 25000); rejected when runtime=editor", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready before failing when runtime=game (default: true); rejected when runtime=editor", false},
};

const std::vector<std::string> kCSharpMemberActions = {"get", "set", "call",
                                                       "list"};
const std::vector<std::string> kCSharpEventActions = {"subscribe",
                                                      "unsubscribe"};

inline mcp::JsonValue csharp_action_args(const mcp::JsonValue &args,
                                         const char *action) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["action"] = mcp::JsonValue(action);
  if (!args.IsObject()) {
    return out;
  }
  for (const auto &entry : args.GetObject()) {
    if (entry.first != "action") {
      out[entry.first] = entry.second;
    }
  }
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(2);
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "csharp_member",
      "Read, write, call or list C# members through the frozen GdaCSharpBridge ABI. action selects the operation: 'get' reads an engine property or a C# property/field that GDScript cannot see via get_prop; 'set' writes one via set_prop (read-only members and indexers are rejected); 'call' invokes a method via call_method with overload resolution by argument count; 'list' returns method, property and signal tables read with Object reflection for pre-call discovery. Locators: get/set/call take exactly one of object_id, object_id_str, memory or path; list takes one of the same locator fields but reads the object directly. With runtime=editor (default) the object is resolved in the editor process (get/set/call need a .NET editor build with the bridge registered; list needs no bridge). With runtime=game the scene-path locator is forwarded over the runtime channel (needs a running .NET game with GdaCSharpRuntime; object_id, object_id_str and memory locators are rejected there, and game-side values cross as JSON-native values only). This tool is CodeExecute: confirm=true is always required. Returns the serialized member or return value for get/set/call, or result {methods, properties, signals, counts, truncated} for list. Example: {\"action\": \"get\", \"path\": \"Player\", \"property\": \"Health\", \"confirm\": true} or {\"action\": \"call\", \"path\": \"Player\", \"method\": \"Jump\", \"confirm\": true}.",
      "Scripts", {"csharp", "reflection", "member", "property", "method", "list"},
      SideEffect::CodeExecute, tool_flags::kMutating,
      kCSharpMemberParams, csharp_bridge_ops::handle_csharp_member,
      schema::build_action_schema(
          kCSharpMemberActions, kCSharpMemberParams,
          "get: read a member value; set: write a member value; call: invoke a method; list: list method, property and signal tables"),
      {},
      {"{\"action\": \"get\", \"path\": \"Player\", \"property\": \"Health\", \"confirm\": true}",
       "{\"action\": \"list\", \"path\": \"Player\", \"name_contains\": \"Jump\", \"confirm\": true}"}},
      {
          ToolAlias{
              "csharp_get_property", "csharp_member",
              [](const mcp::JsonValue &args) {
                return csharp_action_args(args, "get");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "csharp_set_property", "csharp_member",
              [](const mcp::JsonValue &args) {
                return csharp_action_args(args, "set");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "csharp_call_method", "csharp_member",
              [](const mcp::JsonValue &args) {
                return csharp_action_args(args, "call");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "csharp_list_members", "csharp_member",
              [](const mcp::JsonValue &args) {
                return csharp_action_args(args, "list");
              },
              {}, "0.4.0", "0.6.0"},
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "csharp_event",
      "Subscribe a receiver method to a C# event or release the subscription through the frozen GdaCSharpBridge ABI. action selects the operation: 'subscribe' links a source event to a receiver method (source locator takes exactly one of object_id, object_id_str, memory or path; receiver locator takes exactly one of target_object_id, target_object_id_str, target_memory or target_path; event arguments are forwarded to the receiver method call) and returns the subscription token issued by the bridge; 'unsubscribe' releases that subscription by token. With runtime=editor (default) both objects are resolved in the editor process and linked via the GdaCSharpBridge engine singleton (requires a .NET editor build with the bridge registered). With runtime=game both scene-path locators are forwarded to the running game over the runtime channel (requires a running .NET game with GdaCSharpRuntime; restarting the game drops every subscription). This tool is CodeExecute: confirm=true is always required. Example: {\"action\": \"subscribe\", \"path\": \"Emitter\", \"event\": \"Exploded\", \"target_path\": \"Hud\", \"method\": \"ShowBanner\", \"confirm\": true} or {\"action\": \"unsubscribe\", \"token\": 1, \"confirm\": true}.",
      "Scripts", {"csharp", "reflection", "event", "subscribe", "unsubscribe"},
      SideEffect::CodeExecute, tool_flags::kMutating,
      kCSharpEventParams, csharp_bridge_ops::handle_csharp_event,
      schema::build_action_schema(
          kCSharpEventActions, kCSharpEventParams,
          "subscribe: link a C# event to a receiver method, returns the token; unsubscribe: release the subscription by token"),
      {},
      {"{\"action\": \"subscribe\", \"path\": \"Emitter\", \"event\": \"Exploded\", \"target_path\": \"Hud\", \"method\": \"ShowBanner\", \"confirm\": true}",
       "{\"action\": \"unsubscribe\", \"token\": 1, \"confirm\": true}"}},
      {
          ToolAlias{
              "csharp_subscribe_event", "csharp_event",
              [](const mcp::JsonValue &args) {
                return csharp_action_args(args, "subscribe");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "csharp_unsubscribe_event", "csharp_event",
              [](const mcp::JsonValue &args) {
                return csharp_action_args(args, "unsubscribe");
              },
              {}, "0.4.0", "0.6.0"},
      })));
  return v;
}

} // namespace csharp_bridge_tools
} // namespace godot_autopilot

#endif
