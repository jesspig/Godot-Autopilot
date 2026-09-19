#ifndef GODOT_AUTOPILOT_TOOL_ARGS_HPP
#define GODOT_AUTOPILOT_TOOL_ARGS_HPP

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <mcp/JsonValue.hpp>
#include <tools/schema_builder.hpp>

namespace godot_autopilot {

class ToolArgError : public std::runtime_error {
public:
  explicit ToolArgError(const std::string &message)
      : std::runtime_error(message) {}
};

class Args {
public:
  Args(const mcp::JsonValue &raw, const std::vector<schema::ParamDef> &params);

  const mcp::JsonValue &raw() const;
  bool has(const std::string &name) const;
  bool is_null(const std::string &name) const;
  std::optional<std::string> opt_string(const std::string &name) const;
  std::optional<int64_t> opt_int(const std::string &name) const;
  std::optional<double> opt_number(const std::string &name) const;
  std::optional<bool> opt_bool(const std::string &name) const;
  const mcp::JsonValue *opt_object(const std::string &name) const;
  const mcp::JsonValue *opt_array(const std::string &name) const;
  std::string require_string(const std::string &name) const;
  int64_t require_int(const std::string &name) const;
  double require_number(const std::string &name) const;
  bool require_bool(const std::string &name) const;
  const mcp::JsonValue &require_object(const std::string &name) const;
  const mcp::JsonValue &require_array(const std::string &name) const;
  std::string get_string(const std::string &name,
                         const std::string &fallback) const;
  int64_t get_int(const std::string &name, int64_t fallback) const;
  double get_number(const std::string &name, double fallback) const;
  bool get_bool(const std::string &name, bool fallback) const;
  void reject_unknown() const;

private:
  mcp::JsonValue raw_;
  const std::vector<schema::ParamDef> &params_;
};

} // namespace godot_autopilot

#endif
