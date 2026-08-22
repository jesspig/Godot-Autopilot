#include "tools/schema_builder.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::schema::add_required_flag;
using godot_autopilot::schema::build_schema;
using godot_autopilot::schema::ParamDef;

namespace {

bool required_array_contains(const mcp::JsonValue &schema,
                             const std::string &name) {
  const mcp::JsonValue *req = schema.Find("required");
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
  const mcp::JsonValue *props = schema.Find("properties");
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
  const mcp::JsonValue *props = schema.Find("properties");
  ASSERT_NE(props, nullptr);
  EXPECT_TRUE(props->IsObject());
  EXPECT_TRUE(props->Empty());
  const mcp::JsonValue *req = schema.Find("required");
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

TEST(SchemaBuilderTest, AddRequiredFlagIsIdempotent) {
  mcp::JsonValue schema(mcp::JsonValue::object_tag);
  schema["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
  add_required_flag(schema, "dup");
  add_required_flag(schema, "dup");
  add_required_flag(schema, "other");
  ASSERT_NE(schema.Find("required"), nullptr);
  EXPECT_TRUE(schema.Find("required")->IsArray());
  EXPECT_EQ(schema.Find("required")->Size(), 2u);
}
