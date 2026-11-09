#ifndef GODOT_AUTOPILOT_TOOL_EXECUTE_HPP
#define GODOT_AUTOPILOT_TOOL_EXECUTE_HPP

#include <functional>
#include <string>
#include <utility>

#include <mcp/JsonValue.hpp>

#include <core/export_guard.hpp>

namespace godot_autopilot {
namespace tools {

using ToolExecutorFn =
    std::function<mcp::JsonValue(const std::string &name,
                                 const mcp::JsonValue &args)>;

using ArgumentValidatorFn =
    std::function<mcp::JsonValue(const std::string &name,
                                 const mcp::JsonValue &args)>;

using AuthorizationFn = std::function<mcp::JsonValue(const std::string &name)>;

using EnvelopeFn = std::function<mcp::JsonValue(const std::string &name,
                                                const mcp::JsonValue &result)>;

using ExportGuardFn = std::function<bool()>;

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

inline ArgumentValidatorFn &argument_validator_slot() {
  static ArgumentValidatorFn validator;
  return validator;
}

inline void register_argument_validator(ArgumentValidatorFn fn) {
  argument_validator_slot() = std::move(fn);
}

inline mcp::JsonValue run_argument_validator(const std::string &name,
                                             const mcp::JsonValue &args) {
  ArgumentValidatorFn &validator = argument_validator_slot();
  if (!validator) {
    return mcp::JsonValue();
  }
  return validator(name, args);
}

inline AuthorizationFn &authorization_policy_slot() {
  static AuthorizationFn policy;
  return policy;
}

inline void register_authorization_policy(AuthorizationFn fn) {
  authorization_policy_slot() = std::move(fn);
}

inline mcp::JsonValue run_authorization_policy(const std::string &name) {
  AuthorizationFn &policy = authorization_policy_slot();
  if (!policy) {
    return mcp::JsonValue();
  }
  return policy(name);
}

inline EnvelopeFn &envelope_builder_slot() {
  static EnvelopeFn builder;
  return builder;
}

inline void register_envelope_builder(EnvelopeFn fn) {
  envelope_builder_slot() = std::move(fn);
}

inline mcp::JsonValue run_envelope_builder(const std::string &name,
                                            const mcp::JsonValue &result) {
  EnvelopeFn &builder = envelope_builder_slot();
  if (!builder) {
    return result;
  }
  return builder(name, result);
}

inline ExportGuardFn &export_guard_slot() {
  static ExportGuardFn guard = []() {
    return godot_autopilot::ExportGuard::is_exporting();
  };
  return guard;
}

inline void register_export_guard(ExportGuardFn fn) {
  export_guard_slot() = std::move(fn);
}

inline bool export_in_progress() {
  ExportGuardFn &guard = export_guard_slot();
  if (!guard) {
    return false;
  }
  return guard();
}

mcp::JsonValue execute_tool_core(const std::string &name,
                                  const mcp::JsonValue &args);

mcp::JsonValue execute_tool_chain(const std::string &name,
                                  const mcp::JsonValue &args);

} // namespace tools
} // namespace godot_autopilot::tools

#endif
