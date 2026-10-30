#ifndef GODOT_AUTOPILOT_ARGUMENT_VALIDATOR_HPP
#define GODOT_AUTOPILOT_ARGUMENT_VALIDATOR_HPP

#include <string>
#include <vector>

#include <mcp/JsonValue.hpp>

#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace argument_validator {

struct ValidationOutcome {
  mcp::JsonValue error;
  std::vector<std::string> unknown_keys;

  bool passed() const { return error.IsNull(); }
};

ValidationOutcome validate_params(const std::vector<ParamSpec> &params,
                                  const mcp::JsonValue &args);

ValidationOutcome validate_tool_args(const std::string &name,
                                     const mcp::JsonValue &args);

mcp::JsonValue validate_tool_args_for_dispatch(const std::string &name,
                                                const mcp::JsonValue &args);

} // namespace argument_validator
} // namespace godot_autopilot

#endif
