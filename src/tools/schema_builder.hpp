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

mcp::JsonValue make_object_schema();
mcp::JsonValue make_empty_schema();
void add_param(mcp::JsonValue &schema, const ParamDef &param);
void add_required_flag(mcp::JsonValue &schema, const std::string &name);

mcp::JsonValue string_param(const std::string &desc, bool required);
mcp::JsonValue int_param(const std::string &desc, bool required);
mcp::JsonValue num_param(const std::string &desc, bool required);
mcp::JsonValue bool_param(const std::string &desc, bool required);
mcp::JsonValue obj_param(const std::string &desc, bool required);
mcp::JsonValue arr_param(const std::string &desc, bool required);

mcp::JsonValue build_schema(std::initializer_list<ParamDef> params);

} // namespace schema
} // namespace godot_autopilot

#endif
