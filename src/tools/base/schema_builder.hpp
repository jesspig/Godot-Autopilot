#ifndef GODOT_AUTOPILOT_SCHEMA_BUILDER_HPP
#define GODOT_AUTOPILOT_SCHEMA_BUILDER_HPP

#include <initializer_list>
#include "util/json_value.hpp"
#include <string>
#include <vector>

namespace godot_autopilot {
namespace schema {

struct ParamDef {
  std::string name;
  std::string type;
  std::string description;
  bool required;
};

void add_required_flag(godot_autopilot::util::JsonValue &schema, const std::string &name);

bool is_supported_param_type(const std::string &type);

bool validate_param_defs(const std::vector<ParamDef> &params,
                         std::string &error);

godot_autopilot::util::JsonValue build_schema(std::initializer_list<ParamDef> params);

godot_autopilot::util::JsonValue build_schema(const std::vector<ParamDef> &params);

godot_autopilot::util::JsonValue build_action_schema(const std::vector<std::string> &actions,
                                   const std::vector<ParamDef> &params,
                                   const std::string &description = "");

} // namespace schema
} // namespace godot_autopilot

#endif
