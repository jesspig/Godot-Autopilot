#include "tools/tool_execute.hpp"

#include <memory>
#include <string>

#include <tools/dispatch.hpp>
#include <tools/register_all.hpp>
#include <tools/tool_base.hpp>
#include <tools/tool_registry.hpp>

namespace godot_autopilot {
namespace tools {

namespace {

mcp::JsonValue export_in_progress_error() {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] = mcp::JsonValue(dispatch::export_blocked_error_text());
  result["code"] = mcp::JsonValue("export_in_progress");
  return result;
}

mcp::JsonValue tool_not_found_error(const std::string &name) {
  const std::string hint = "use search_tools to discover available tools";
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] = mcp::JsonValue("tool '" + name + "' not found — " + hint);
  result["code"] = mcp::JsonValue("tool_not_found");
  result["hint"] = mcp::JsonValue(hint);
  return result;
}

std::shared_ptr<ToolBase> find_registered_tool(const std::string &name) {
  std::shared_ptr<ToolRegistry> registry = get_active_registry();
  if (!registry) {
    return nullptr;
  }
  return registry->find_any(name);
}

} // namespace

mcp::JsonValue execute_tool_core(const std::string &name,
                                  const mcp::JsonValue &args) {
  return dispatch::call_handler(name, args);
}

mcp::JsonValue execute_tool_chain(const std::string &name,
                                  const mcp::JsonValue &args) {
  if (export_in_progress()) {
    return export_in_progress_error();
  }
  if (!find_registered_tool(name)) {
    return tool_not_found_error(name);
  }
  mcp::JsonValue invalid = run_argument_validator(name, args);
  if (!invalid.IsNull()) {
    return invalid;
  }
  mcp::JsonValue denied = run_authorization_policy(name);
  if (!denied.IsNull()) {
    return denied;
  }
  mcp::JsonValue result = execute_tool_core(name, args);
  return run_envelope_builder(name, result);
}

namespace {

struct ToolExecutorRegistration {
  ToolExecutorRegistration() { register_tool_executor(&execute_tool_chain); }
};

const ToolExecutorRegistration g_tool_executor_registration;

} // namespace

} // namespace tools
} // namespace godot_autopilot::tools
