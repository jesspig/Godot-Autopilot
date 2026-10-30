#include "tools/argument_validator.hpp"

#include <memory>
#include <string>
#include <vector>

#include <core/log_system.hpp>
#include <tools/register_all.hpp>
#include <tools/tool_args.hpp>
#include <tools/tool_base.hpp>
#include <tools/tool_execute.hpp>
#include <tools/tool_registry.hpp>

namespace godot_autopilot {
namespace argument_validator {

namespace {

mcp::JsonValue invalid_argument_error(const std::string &message,
                                       const std::string &parameter) {
  mcp::JsonValue error(mcp::JsonValue::object_tag);
  error["error"] = mcp::JsonValue(message);
  error["code"] = mcp::JsonValue("invalid_argument");
  error["parameter"] = mcp::JsonValue(parameter);
  return error;
}

void check_declared_type(const Args &parsed, const ParamSpec &param) {
  const std::string &type = param.type;
  if (type == "json") {
    return;
  }
  if (type == "string") {
    parsed.opt_string(param.name);
    return;
  }
  if (type == "number") {
    parsed.opt_number(param.name);
    return;
  }
  if (type == "integer") {
    parsed.opt_int(param.name);
    return;
  }
  if (type == "boolean") {
    parsed.opt_bool(param.name);
    return;
  }
  if (type == "object") {
    parsed.opt_object(param.name);
    return;
  }
  if (type == "array") {
    parsed.opt_array(param.name);
    return;
  }
}

std::vector<std::string> collect_unknown_keys(
    const std::vector<ParamSpec> &params, const mcp::JsonValue &args) {
  std::vector<std::string> unknown;
  if (!args.IsObject()) {
    return unknown;
  }
  for (const auto &entry : args.GetObject()) {
    bool declared = false;
    for (const ParamSpec &param : params) {
      if (param.name == entry.first) {
        declared = true;
        break;
      }
    }
    if (!declared) {
      unknown.push_back(entry.first);
    }
  }
  return unknown;
}

std::string join_keys(const std::vector<std::string> &keys) {
  std::string text;
  for (const std::string &key : keys) {
    if (!text.empty()) {
      text += ", ";
    }
    text += key;
  }
  return text;
}

} // namespace

ValidationOutcome validate_params(const std::vector<ParamSpec> &params,
                                  const mcp::JsonValue &args) {
  ValidationOutcome outcome;
  Args parsed(args, params);
  for (const ParamSpec &param : params) {
    if (param.required && !parsed.has(param.name)) {
      outcome.error = invalid_argument_error(
          "missing required parameter: " + param.name, param.name);
      return outcome;
    }
    try {
      check_declared_type(parsed, param);
    } catch (const ToolArgError &error) {
      outcome.error = invalid_argument_error(error.what(), param.name);
      return outcome;
    }
  }
  outcome.unknown_keys = collect_unknown_keys(params, args);
  return outcome;
}

ValidationOutcome validate_tool_args(const std::string &name,
                                     const mcp::JsonValue &args) {
  ValidationOutcome outcome;
  std::shared_ptr<ToolRegistry> registry = get_active_registry();
  if (!registry) {
    return outcome;
  }
  std::shared_ptr<ToolBase> tool = registry->find_any(name);
  if (!tool) {
    return outcome;
  }
  const SpecTool *spec_tool = dynamic_cast<const SpecTool *>(tool.get());
  if (!spec_tool) {
    return outcome;
  }
  return validate_params(spec_tool->spec().params, args);
}

mcp::JsonValue validate_tool_args_for_dispatch(const std::string &name,
                                                const mcp::JsonValue &args) {
  ValidationOutcome outcome = validate_tool_args(name, args);
  if (!outcome.unknown_keys.empty()) {
    LogSystem::instance().log(
        LogLevel::Warning, LogCategory::Tools,
        "call_tool " + name + ": unknown parameters ignored: " +
            join_keys(outcome.unknown_keys));
  }
  if (outcome.passed()) {
    return mcp::JsonValue();
  }
  return outcome.error;
}

namespace {

struct ArgumentValidatorRegistration {
  ArgumentValidatorRegistration() {
    tools::register_argument_validator(
        &argument_validator::validate_tool_args_for_dispatch);
  }
};

const ArgumentValidatorRegistration g_argument_validator_registration;

} // namespace

} // namespace argument_validator
} // namespace godot_autopilot
