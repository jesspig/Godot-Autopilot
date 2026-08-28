#include "core/command_queue.hpp"
#include "core/log_system.hpp"
#include "tools/dispatch.hpp"
#include "tools/runtime_ops.hpp"
#include <mcp/Content.hpp>

namespace godot_autopilot {
namespace dispatch {

std::unordered_map<std::string, HandlerFn> g_handlers;
std::unordered_map<std::string, HandlerFn> g_meta_handlers;

namespace {

mcp::JsonValue call_handler_impl(const std::string &name,
                                 const mcp::JsonValue &args) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  auto it = g_handlers.find(name);
  if (it != g_handlers.end()) {
    try {
      result = it->second(args);
    } catch (...) {
      std::string args_dump = args.Dump();
      if (args_dump.size() > 256) {
        args_dump.resize(256);
      }
      result = mcp::JsonValue(mcp::JsonValue::object_tag);
      result["error"] = mcp::JsonValue("internal error in tool '" + name +
                                       "': unexpected C++ exception (args: " +
                                       args_dump + ")");
    }
  } else if (auto meta_it = g_meta_handlers.find(name); meta_it != g_meta_handlers.end()) {
    result = meta_it->second(args);
  } else {
    result = mcp::JsonValue(mcp::JsonValue::object_tag);
    result["error"] =
        mcp::JsonValue("domain tool '" + name +
                       "' not found — use search_tools to discover available "
                       "tools");
  }
  bool has_error = result.IsObject() && result.Find("error") != nullptr;
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
      "call_tool: " + name + " -> " + (has_error ? "error" : "ok"));
  return result;
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
