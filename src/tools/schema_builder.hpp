#ifndef GODOT_AUTOPILOT_SCHEMA_BUILDER_HPP
#define GODOT_AUTOPILOT_SCHEMA_BUILDER_HPP

#include <initializer_list>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace schema {

struct ParamDef {
  std::string name;
  std::string type;
  std::string description;
  bool required;
};

void add_required_flag(mcp::JsonValue &schema, const std::string &name);

mcp::JsonValue build_schema(std::initializer_list<ParamDef> params);

} // namespace schema
} // namespace godot_autopilot

#endif
