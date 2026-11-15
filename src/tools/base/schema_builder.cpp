#include "tools/base/schema_builder.hpp"

namespace godot_autopilot {
namespace schema {

void add_required_flag(mcp::JsonValue &schema, const std::string &name) {
  auto *req = schema.Find("required");
  if (!req) {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    arr.PushBack(mcp::JsonValue(name));
    schema["required"] = std::move(arr);
  } else if (req->IsArray()) {

    for (auto &v : req->GetArray()) {
      if (v.IsString() && v.GetString() == name)
        return;
    }
    req->PushBack(mcp::JsonValue(name));
  }
}

mcp::JsonValue build_schema(const std::vector<ParamDef> &params) {
  mcp::JsonValue s(mcp::JsonValue::object_tag);
  s["type"] = mcp::JsonValue("object");
  s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
  s["required"] = mcp::JsonValue(mcp::JsonValue::array_tag);

  auto *props = s.Find("properties");
  if (!props || !props->IsObject())
    return s;

  for (const auto &p : params) {
    mcp::JsonValue prop(mcp::JsonValue::object_tag);
    if (p.type != "json") {
      prop["type"] = mcp::JsonValue(p.type);
    }
    if (!p.description.empty()) {
      prop["description"] = mcp::JsonValue(p.description);
    }
    (*props)[p.name] = std::move(prop);

    if (p.required) {
      add_required_flag(s, p.name);
    }
  }

  return s;
}

mcp::JsonValue build_schema(std::initializer_list<ParamDef> params) {
  return build_schema(std::vector<ParamDef>(params));
}

mcp::JsonValue build_action_schema(const std::vector<std::string> &actions,
                                   const std::vector<ParamDef> &params,
                                   const std::string &description) {
  mcp::JsonValue schema = build_schema(params);
  mcp::JsonValue action(mcp::JsonValue::object_tag);
  action["type"] = mcp::JsonValue("string");
  if (!description.empty()) {
    action["description"] = mcp::JsonValue(description);
  }
  mcp::JsonValue values(mcp::JsonValue::array_tag);
  for (const auto &value : actions) {
    values.PushBack(mcp::JsonValue(value));
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
