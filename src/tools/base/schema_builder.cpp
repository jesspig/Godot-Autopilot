#include "tools/base/schema_builder.hpp"

#include <unordered_set>

namespace godot_autopilot {
namespace schema {

bool is_supported_param_type(const std::string &type) {
  return type == "string" || type == "integer" || type == "number" ||
         type == "boolean" || type == "object" || type == "array" ||
         type == "json";
}

bool validate_param_defs(const std::vector<ParamDef> &params,
                         std::string &error) {
  std::unordered_set<std::string> names;
  for (const ParamDef &param : params) {
    if (param.name.empty()) {
      error = "parameter name must be a non-empty string";
      return false;
    }
    if (!names.insert(param.name).second) {
      error = "duplicate parameter name: " + param.name;
      return false;
    }
    if (!is_supported_param_type(param.type)) {
      error = "unsupported parameter type: " + param.type;
      return false;
    }
  }
  error.clear();
  return true;
}

void add_required_flag(godot_autopilot::util::JsonValue &schema, const std::string &name) {
  auto *req = schema.Find("required");
  if (!req) {
    godot_autopilot::util::JsonValue arr(godot_autopilot::util::JsonValue::array_tag);
    arr.PushBack(godot_autopilot::util::JsonValue(name));
    schema["required"] = std::move(arr);
  } else if (req->IsArray()) {

    for (auto &v : req->GetArray()) {
      if (v.IsString() && v.GetString() == name)
        return;
    }
    req->PushBack(godot_autopilot::util::JsonValue(name));
  }
}

godot_autopilot::util::JsonValue build_schema(const std::vector<ParamDef> &params) {
  godot_autopilot::util::JsonValue s(godot_autopilot::util::JsonValue::object_tag);
  s["type"] = godot_autopilot::util::JsonValue("object");
  s["properties"] = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
  s["required"] = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::array_tag);

  auto *props = s.Find("properties");
  if (!props || !props->IsObject())
    return s;

  for (const auto &p : params) {
    godot_autopilot::util::JsonValue prop(godot_autopilot::util::JsonValue::object_tag);
    if (p.type != "json") {
      prop["type"] = godot_autopilot::util::JsonValue(p.type);
    }
    if (!p.description.empty()) {
      prop["description"] = godot_autopilot::util::JsonValue(p.description);
    }
    (*props)[p.name] = std::move(prop);

    if (p.required) {
      add_required_flag(s, p.name);
    }
  }

  return s;
}

godot_autopilot::util::JsonValue build_schema(std::initializer_list<ParamDef> params) {
  return build_schema(std::vector<ParamDef>(params));
}

godot_autopilot::util::JsonValue build_action_schema(const std::vector<std::string> &actions,
                                   const std::vector<ParamDef> &params,
                                   const std::string &description) {
  godot_autopilot::util::JsonValue schema = build_schema(params);
  godot_autopilot::util::JsonValue action(godot_autopilot::util::JsonValue::object_tag);
  action["type"] = godot_autopilot::util::JsonValue("string");
  if (!description.empty()) {
    action["description"] = godot_autopilot::util::JsonValue(description);
  }
  godot_autopilot::util::JsonValue values(godot_autopilot::util::JsonValue::array_tag);
  for (const auto &value : actions) {
    values.PushBack(godot_autopilot::util::JsonValue(value));
  }
  action["enum"] = std::move(values);
  auto *properties = schema.Find("properties");
  if (properties != nullptr && properties->IsObject()) {
    (*properties)["action"] = std::move(action);
  }
  add_required_flag(schema, "action");
  return schema;
}

} // namespace schema
} // namespace godot_autopilot
