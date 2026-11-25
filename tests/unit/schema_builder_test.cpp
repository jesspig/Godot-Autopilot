#include "tools/base/schema_builder.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::schema::add_required_flag;
using godot_autopilot::schema::build_action_schema;
using godot_autopilot::schema::build_schema;
using godot_autopilot::schema::is_supported_param_type;
using godot_autopilot::schema::ParamDef;
using godot_autopilot::schema::validate_param_defs;

namespace {

bool required_array_contains(const godot_autopilot::util::JsonValue &schema,
                             const std::string &name) {
  const godot_autopilot::util::JsonValue *req = schema.Find("required");
  if (!req || !req->IsArray()) {
    return false;
  }
  for (const auto &v : req->GetArray()) {
    if (v.IsString() && v.GetString() == name) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST(SchemaBuilderTest, BuildSchemaPutsRequiredInTopLevelArray) {
  auto schema = build_schema({
      {"alpha", "string", "first param", true},
      {"beta", "integer", "second param", false},
  });
  EXPECT_EQ(schema.At("type").GetString(), "object");
  const godot_autopilot::util::JsonValue *props = schema.Find("properties");
  ASSERT_NE(props, nullptr);
  EXPECT_TRUE(props->IsObject());
  EXPECT_EQ(props->GetObject().size(), 2u);
  EXPECT_EQ(props->At("alpha").At("type").GetString(), "string");
  EXPECT_EQ(props->At("alpha").At("description").GetString(), "first param");
  EXPECT_EQ(props->At("beta").At("type").GetString(), "integer");
  EXPECT_TRUE(required_array_contains(schema, "alpha"));
  EXPECT_FALSE(required_array_contains(schema, "beta"));
}

TEST(SchemaBuilderTest, BuildSchemaEmptyParams) {
  auto schema = build_schema({});
  ASSERT_TRUE(schema.IsObject());
  EXPECT_EQ(schema.At("type").GetString(), "object");
  const godot_autopilot::util::JsonValue *props = schema.Find("properties");
  ASSERT_NE(props, nullptr);
  EXPECT_TRUE(props->IsObject());
  EXPECT_TRUE(props->Empty());
  const godot_autopilot::util::JsonValue *req = schema.Find("required");
  ASSERT_NE(req, nullptr);
  EXPECT_TRUE(req->IsArray());
  EXPECT_TRUE(req->Empty());
}

TEST(SchemaBuilderTest, BuildSchemaUnknownTypeDoesNotCrash) {
  auto schema = build_schema({{"weird", "unknown_kind", "", true}});
  EXPECT_EQ(schema.Find("properties")->At("weird").At("type").GetString(),
            "unknown_kind");
  EXPECT_TRUE(required_array_contains(schema, "weird"));
}

TEST(SchemaBuilderTest, BuildSchemaJsonTypeOmitsTypeField) {
  auto schema = build_schema({
      {"payload", "json", "any JSON value", true},
      {"path", "string", "node path", true},
  });
  const godot_autopilot::util::JsonValue *props = schema.Find("properties");
  ASSERT_NE(props, nullptr);
  ASSERT_TRUE(props->IsObject());
  const godot_autopilot::util::JsonValue &payload = props->At("payload");
  EXPECT_EQ(payload.Find("type"), nullptr);
  EXPECT_EQ(payload.At("description").GetString(), "any JSON value");
  EXPECT_EQ(props->At("path").At("type").GetString(), "string");
  EXPECT_TRUE(required_array_contains(schema, "payload"));
}

TEST(SchemaBuilderTest, BuildActionSchemaAddsEnumAndRequiredAction) {
  auto schema = build_action_schema(
      {"get", "set"}, {{"path", "string", "node path", true}});
  const godot_autopilot::util::JsonValue *props = schema.Find("properties");
  ASSERT_NE(props, nullptr);
  const godot_autopilot::util::JsonValue &action = props->At("action");
  EXPECT_EQ(action.At("type").GetString(), "string");
  const godot_autopilot::util::JsonValue *values = action.Find("enum");
  ASSERT_NE(values, nullptr);
  ASSERT_TRUE(values->IsArray());
  ASSERT_EQ(values->Size(), 2u);
  EXPECT_EQ((*values)[0].GetString(), "get");
  EXPECT_EQ((*values)[1].GetString(), "set");
  EXPECT_TRUE(required_array_contains(schema, "action"));
  EXPECT_TRUE(required_array_contains(schema, "path"));
}

TEST(SchemaBuilderTest, AddRequiredFlagIsIdempotent) {
  godot_autopilot::util::JsonValue schema(godot_autopilot::util::JsonValue::object_tag);
  schema["properties"] = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
  add_required_flag(schema, "dup");
  add_required_flag(schema, "dup");
  add_required_flag(schema, "other");
  ASSERT_NE(schema.Find("required"), nullptr);
  EXPECT_TRUE(schema.Find("required")->IsArray());
  EXPECT_EQ(schema.Find("required")->Size(), 2u);
}

TEST(SchemaBuilderTest, DynamicParamWhitelistAcceptsSupportedTypes) {
  for (const char *type : {"string", "integer", "number", "boolean", "object",
                           "array", "json"}) {
    EXPECT_TRUE(is_supported_param_type(type)) << type;
  }
  EXPECT_FALSE(is_supported_param_type("unknown_kind"));
  EXPECT_FALSE(is_supported_param_type(""));
}

TEST(SchemaBuilderTest, ValidateParamDefsRejectsDuplicateNames) {
  const std::vector<ParamDef> params = {
      {"value", "string", "", false},
      {"value", "integer", "", false},
  };
  std::string error;
  EXPECT_FALSE(validate_param_defs(params, error));
  EXPECT_NE(error.find("duplicate parameter name: value"), std::string::npos);
}

TEST(SchemaBuilderTest, ValidateParamDefsRejectsUnsupportedTypes) {
  const std::vector<ParamDef> params = {
      {"value", "unknown_kind", "", false},
  };
  std::string error;
  EXPECT_FALSE(validate_param_defs(params, error));
  EXPECT_NE(error.find("unsupported parameter type: unknown_kind"),
            std::string::npos);
}

TEST(SchemaBuilderTest, ValidateParamDefsAcceptsEmptyAndValidDefinitions) {
  std::string error;
  EXPECT_TRUE(validate_param_defs({}, error));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(validate_param_defs({{"payload", "json", "", true}}, error));
  EXPECT_TRUE(error.empty());
}
