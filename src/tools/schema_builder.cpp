#include "tools/schema_builder.hpp"

namespace godot_self_driving {
namespace schema {

mcp::JsonValue make_object_schema() {
  mcp::JsonValue s(mcp::JsonValue::object_tag);
  s["type"] = mcp::JsonValue("object");
  mcp::JsonValue props(mcp::JsonValue::object_tag);
  s["properties"] = std::move(props);
  mcp::JsonValue req(mcp::JsonValue::array_tag);
  s["required"] = std::move(req);
  return s;
}

mcp::JsonValue make_empty_schema() {
  mcp::JsonValue s(mcp::JsonValue::object_tag);
  s["type"] = mcp::JsonValue("object");
  s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
  return s;
}

void add_param(mcp::JsonValue &schema, const ParamDef &param) {
  auto *props = schema.Find("properties");
  if (!props || !props->IsObject())
    return;

  mcp::JsonValue prop(mcp::JsonValue::object_tag);
  prop["type"] = mcp::JsonValue(param.type);
  if (!param.description.empty()) {
    prop["description"] = mcp::JsonValue(param.description);
  }
  (*props)[param.name] = std::move(prop);

  if (param.required) {
    add_required_flag(schema, param.name);
  }
}

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

mcp::JsonValue string_param(const std::string &desc, bool required) {
  mcp::JsonValue p(mcp::JsonValue::object_tag);
  p["type"] = mcp::JsonValue("string");
  if (!desc.empty())
    p["description"] = mcp::JsonValue(desc);
  if (required)
    p["required"] = mcp::JsonValue(true);
  return p;
}

mcp::JsonValue int_param(const std::string &desc, bool required) {
  mcp::JsonValue p(mcp::JsonValue::object_tag);
  p["type"] = mcp::JsonValue("integer");
  if (!desc.empty())
    p["description"] = mcp::JsonValue(desc);
  if (required)
    p["required"] = mcp::JsonValue(true);
  return p;
}

mcp::JsonValue num_param(const std::string &desc, bool required) {
  mcp::JsonValue p(mcp::JsonValue::object_tag);
  p["type"] = mcp::JsonValue("number");
  if (!desc.empty())
    p["description"] = mcp::JsonValue(desc);
  if (required)
    p["required"] = mcp::JsonValue(true);
  return p;
}

mcp::JsonValue bool_param(const std::string &desc, bool required) {
  mcp::JsonValue p(mcp::JsonValue::object_tag);
  p["type"] = mcp::JsonValue("boolean");
  if (!desc.empty())
    p["description"] = mcp::JsonValue(desc);
  if (required)
    p["required"] = mcp::JsonValue(true);
  return p;
}

mcp::JsonValue obj_param(const std::string &desc, bool required) {
  mcp::JsonValue p(mcp::JsonValue::object_tag);
  p["type"] = mcp::JsonValue("object");
  if (!desc.empty())
    p["description"] = mcp::JsonValue(desc);
  if (required)
    p["required"] = mcp::JsonValue(true);
  return p;
}

mcp::JsonValue arr_param(const std::string &desc, bool required) {
  mcp::JsonValue p(mcp::JsonValue::object_tag);
  p["type"] = mcp::JsonValue("array");
  if (!desc.empty())
    p["description"] = mcp::JsonValue(desc);
  if (required)
    p["required"] = mcp::JsonValue(true);
  return p;
}

mcp::JsonValue build_schema(std::initializer_list<ParamDef> params) {
  auto schema = make_object_schema();
  auto *props = schema.Find("properties");
  if (!props || !props->IsObject())
    return schema;

  for (auto &p : params) {
    mcp::JsonValue prop(mcp::JsonValue::object_tag);
    prop["type"] = mcp::JsonValue(p.type);
    if (!p.description.empty()) {
      prop["description"] = mcp::JsonValue(p.description);
    }
    (*props)[p.name] = std::move(prop);

    if (p.required) {
      add_required_flag(schema, p.name);
    }
  }

  return schema;
}

} // namespace schema
} // namespace godot_self_driving
