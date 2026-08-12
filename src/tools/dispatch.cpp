#include "core/command_queue.hpp"
#include "tools/dispatch.hpp"
#include "tools/runtime_ops.hpp"
#include <mcp/Content.hpp>

namespace godot_autopilot {
namespace dispatch {

std::unordered_map<std::string, HandlerFn> g_handlers;
std::unordered_map<std::string, HandlerFn> g_meta_handlers;
const std::unordered_set<std::string> meta_tool_names = {
    "ping",      "search_tools",  "list_categories", "get_tool_detail",
    "call_tool", "batch_execute", "code_execute"};

namespace {

mcp::JsonValue call_handler_impl(const std::string &name,
                                 const mcp::JsonValue &args) {
  auto it = g_handlers.find(name);
  if (it != g_handlers.end()) {
    try {
      return it->second(args);
    } catch (...) {
      std::string args_dump = args.Dump();
      if (args_dump.size() > 256) {
        args_dump.resize(256);
      }
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("internal error in tool '" + name +
                                  "': unexpected C++ exception (args: " +
                                  args_dump + ")");
      return e;
    }
  }
  if (meta_tool_names.count(name)) {
    auto meta_it = g_meta_handlers.find(name);
    if (meta_it != g_meta_handlers.end()) {
      return meta_it->second(args);
    }
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("meta tool '" + name +
                       "' cannot be invoked via this path — try calling it "
                       "directly as a top-level tool instead");
    return e;
  }
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] =
      mcp::JsonValue("domain tool '" + name +
                     "' not found — use search_tools to discover available "
                     "tools");
  return e;
}

} // namespace

mcp::JsonValue call_handler(const std::string &name,
                            const mcp::JsonValue &args) {
  if (!godot_autopilot::runtime_ops::has_editor_queue() ||
      get_editor_queue().is_main_thread()) {
    return call_handler_impl(name, args);
  }
  return get_editor_queue()
      .submit([name, args] { return call_handler_impl(name, args); })
      .get();
}

mcp::CallToolResult export_blocked_result() {
  mcp::CallToolResult err;
  err.is_error = true;
  err.content.push_back(mcp::TextContent{
      "text", R"({"error":"editor is exporting; retry after export completes"})"});
  return err;
}

} // namespace dispatch
} // namespace godot_autopilot
