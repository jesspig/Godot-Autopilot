#include "tools/schema_builder.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_self_driving::schema::add_param;
using godot_self_driving::schema::add_required_flag;
using godot_self_driving::schema::arr_param;
using godot_self_driving::schema::bool_param;
using godot_self_driving::schema::build_schema;
using godot_self_driving::schema::int_param;
using godot_self_driving::schema::make_empty_schema;
using godot_self_driving::schema::make_object_schema;
using godot_self_driving::schema::num_param;
using godot_self_driving::schema::obj_param;
using godot_self_driving::schema::ParamDef;
using godot_self_driving::schema::string_param;

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

TEST(SchemaBuilderTest, MakeObjectSchemaShape) {
  auto schema = make_object_schema();
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

TEST(SchemaBuilderTest, MakeEmptySchemaHasNoRequired) {
  auto schema = make_empty_schema();
  EXPECT_EQ(schema.At("type").GetString(), "object");
  ASSERT_NE(schema.Find("properties"), nullptr);
  EXPECT_TRUE(schema.Find("properties")->IsObject());
  EXPECT_EQ(schema.Find("required"), nullptr);
}

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
  ASSERT_NE(schema.Find("required"), nullptr);
  EXPECT_TRUE(schema.Find("required")->IsArray());
  EXPECT_TRUE(schema.Find("required")->Empty());
  EXPECT_TRUE(schema.Find("properties")->Empty());
}

TEST(SchemaBuilderTest, BuildSchemaUnknownTypeDoesNotCrash) {
  auto schema = build_schema({{"weird", "unknown_kind", "", true}});
  EXPECT_EQ(schema.Find("properties")->At("weird").At("type").GetString(),
            "unknown_kind");
  EXPECT_TRUE(required_array_contains(schema, "weird"));
}

TEST(SchemaBuilderTest, ParamHelpersEmbedRequiredInProperty) {
  auto prop = string_param("text input", true);
  EXPECT_EQ(prop.At("type").GetString(), "string");
  EXPECT_EQ(prop.At("description").GetString(), "text input");
  const mcp::JsonValue *req = prop.Find("required");
  ASSERT_NE(req, nullptr);
  EXPECT_TRUE(req->IsBool());
  EXPECT_TRUE(req->GetBool());

  auto optional = string_param("", false);
  EXPECT_EQ(optional.At("type").GetString(), "string");
  EXPECT_EQ(optional.Find("required"), nullptr);
  EXPECT_EQ(optional.Find("description"), nullptr);
}

TEST(SchemaBuilderTest, ContractDifferenceTopLevelArrayVsEmbeddedBool) {
  auto via_builder = build_schema({{"p", "string", "", true}});
  ASSERT_NE(via_builder.Find("required"), nullptr);
  EXPECT_TRUE(via_builder.Find("required")->IsArray());

  auto via_helper = string_param("", true);
  ASSERT_NE(via_helper.Find("required"), nullptr);
  EXPECT_TRUE(via_helper.Find("required")->IsBool());
}

TEST(SchemaBuilderTest, ParamHelpersTypeMapping) {
  EXPECT_EQ(int_param("", false).At("type").GetString(), "integer");
  EXPECT_EQ(num_param("", false).At("type").GetString(), "number");
  EXPECT_EQ(bool_param("", false).At("type").GetString(), "boolean");
  EXPECT_EQ(obj_param("", false).At("type").GetString(), "object");
  EXPECT_EQ(arr_param("", false).At("type").GetString(), "array");
}

TEST(SchemaBuilderTest, AddParamPopulatesSchema) {
  auto schema = make_empty_schema();
  add_param(schema, {"x", "string", "the x", true});
  add_param(schema, {"y", "integer", "", false});
  const mcp::JsonValue *props = schema.Find("properties");
  ASSERT_NE(props, nullptr);
  EXPECT_TRUE(props->IsObject());
  EXPECT_EQ(props->GetObject().size(), 2u);
  EXPECT_TRUE(required_array_contains(schema, "x"));
  EXPECT_FALSE(required_array_contains(schema, "y"));
}

TEST(SchemaBuilderTest, AddRequiredFlagIsIdempotent) {
  auto schema = make_object_schema();
  add_required_flag(schema, "dup");
  add_required_flag(schema, "dup");
  add_required_flag(schema, "other");
  ASSERT_NE(schema.Find("required"), nullptr);
  EXPECT_TRUE(schema.Find("required")->IsArray());
  EXPECT_EQ(schema.Find("required")->Size(), 2u);
}
