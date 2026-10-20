#ifndef GODOT_AUTOPILOT_TOOL_EXECUTE_HPP
#define GODOT_AUTOPILOT_TOOL_EXECUTE_HPP

#include <functional>
#include <string>
#include <utility>

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace tools {

using ToolExecutorFn =
    std::function<mcp::JsonValue(const std::string &name,
                                 const mcp::JsonValue &args)>;

inline ToolExecutorFn &tool_executor_slot() {
  static ToolExecutorFn executor;
  return executor;
}

inline void register_tool_executor(ToolExecutorFn fn) {
  tool_executor_slot() = std::move(fn);
}

inline mcp::JsonValue execute_tool(const std::string &name,
                                   const mcp::JsonValue &args) {
  ToolExecutorFn &executor = tool_executor_slot();
  if (!executor) {
    mcp::JsonValue::Object doc;
    doc["error"] = "tool executor is not registered";
    doc["code"] = "executor_unavailable";
    return mcp::JsonValue(std::move(doc));
  }
  return executor(name, args);
}

} // namespace tools
} // namespace godot_autopilot::tools

#endif
