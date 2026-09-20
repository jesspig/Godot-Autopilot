
#include "tools/schema_builder.hpp"
#include "tools/tool_args.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

using godot_autopilot::Args;
using godot_autopilot::ToolArgError;
using godot_autopilot::schema::build_schema;
using godot_autopilot::schema::ParamDef;

namespace {

std::vector<ParamDef> full_params() {
  return {
      {"path", "string", "resource path", true},
      {"count", "integer", "", false},
      {"scale", "number", "", false},
      {"flag", "boolean", "", false},
      {"payload", "object", "", false},
      {"items", "array", "", false},
  };
}

std::string error_message(const std::function<void()> &call) {
  try {
    call();
  } catch (const ToolArgError &e) {
    return e.what();
  }
  return "<no error>";
}

} // namespace

TEST(ArgsTest, MissingRequiredParameterMessage) {
  const std::vector<ParamDef> params = full_params();
  Args args(mcp::JsonValue::Parse("{}"), params);
  EXPECT_EQ(error_message([&] { args.require_string("path"); }),
            "missing required parameter: path");
}

TEST(ArgsTest, WrongTypeMessage) {
  const std::vector<ParamDef> params = full_params();
  Args args(mcp::JsonValue::Parse(R"({"path": 3})"), params);
  EXPECT_EQ(error_message([&] { args.require_string("path"); }),
            "invalid parameter: path must be a string");
}

TEST(ArgsTest, NullTreatedAsAbsent) {
  const std::vector<ParamDef> params = full_params();
  Args args(mcp::JsonValue::Parse(R"({"path": null})"), params);
  EXPECT_FALSE(args.has("path"));
  EXPECT_TRUE(args.is_null("path"));
  EXPECT_EQ(args.opt_string("path"), std::nullopt);
  EXPECT_EQ(error_message([&] { args.require_string("path"); }),
            "missing required parameter: path");
}

TEST(ArgsTest, IntAcceptsIntegralNumber) {
  const std::vector<ParamDef> params = full_params();
  Args args(mcp::JsonValue::Parse(R"({"count": 3.0, "other": 4})"), params);
  const std::optional<int64_t> from_double = args.opt_int("count");
  ASSERT_TRUE(from_double.has_value());
  EXPECT_EQ(*from_double, 3);
  EXPECT_EQ(args.require_int("count"), 3);
  EXPECT_EQ(args.require_int("other"), 4);
}

TEST(ArgsTest, IntRejectsFractional) {
  const std::vector<ParamDef> params = full_params();
  Args args(mcp::JsonValue::Parse(R"({"count": 3.5})"), params);
  EXPECT_EQ(error_message([&] { args.opt_int("count"); }),
            "invalid parameter: count must be a integer");
  EXPECT_EQ(error_message([&] { args.require_int("count"); }),
            "invalid parameter: count must be a integer");
}

TEST(ArgsTest, NumberAcceptsInt) {
  const std::vector<ParamDef> params = full_params();
  Args args(mcp::JsonValue::Parse(R"({"scale": 3})"), params);
  const std::optional<double> value = args.opt_number("scale");
  ASSERT_TRUE(value.has_value());
  EXPECT_DOUBLE_EQ(*value, 3.0);
  EXPECT_DOUBLE_EQ(args.require_number("scale"), 3.0);
}

TEST(ArgsTest, BoolStrict) {
  const std::vector<ParamDef> params = full_params();
  Args args(mcp::JsonValue::Parse(R"({"flag": 1})"), params);
  EXPECT_EQ(error_message([&] { args.opt_bool("flag"); }),
            "invalid parameter: flag must be a boolean");
  EXPECT_EQ(error_message([&] { args.require_bool("flag"); }),
            "invalid parameter: flag must be a boolean");
}

TEST(ArgsTest, DefaultsOnAbsent) {
  const std::vector<ParamDef> params = full_params();
  Args args(mcp::JsonValue::Parse("{}"), params);
  EXPECT_EQ(args.get_string("path", "res://fallback"), "res://fallback");
  EXPECT_EQ(args.get_int("count", 7), 7);
  EXPECT_DOUBLE_EQ(args.get_number("scale", 1.5), 1.5);
  EXPECT_TRUE(args.get_bool("flag", true));
}

TEST(ArgsTest, ObjectAndArrayAccess) {
  const std::vector<ParamDef> params = full_params();

  Args absent(mcp::JsonValue::Parse("{}"), params);
  EXPECT_EQ(absent.opt_object("payload"), nullptr);
  EXPECT_EQ(absent.opt_array("items"), nullptr);
  EXPECT_EQ(error_message([&] { absent.require_object("payload"); }),
            "missing required parameter: payload");
  EXPECT_EQ(error_message([&] { absent.require_array("items"); }),
            "missing required parameter: items");

  Args wrong(mcp::JsonValue::Parse(R"({"payload": 3, "items": "x"})"), params);
  EXPECT_EQ(error_message([&] { wrong.opt_object("payload"); }),
            "invalid parameter: payload must be a object");
  EXPECT_EQ(error_message([&] { wrong.opt_array("items"); }),
            "invalid parameter: items must be a array");

  Args present(
      mcp::JsonValue::Parse(R"({"payload": {"a": 1}, "items": [1, 2]})"),
      params);
  const mcp::JsonValue *payload = present.opt_object("payload");
  ASSERT_NE(payload, nullptr);
  EXPECT_TRUE(payload->IsObject());
  EXPECT_TRUE(present.require_object("payload").IsObject());
  const mcp::JsonValue *items = present.opt_array("items");
  ASSERT_NE(items, nullptr);
  EXPECT_TRUE(items->IsArray());
  EXPECT_EQ(present.require_array("items").Size(), 2u);
}

TEST(ArgsTest, RejectUnknownDetectsExtraKey) {
  const std::vector<ParamDef> params = full_params();

  Args extra(mcp::JsonValue::Parse(R"({"path": "x", "extra": 1})"), params);
  EXPECT_EQ(error_message([&] { extra.reject_unknown(); }),
            "unknown parameter: extra");

  Args null_extra(mcp::JsonValue::Parse(R"({"path": "x", "extra": null})"),
                  params);
  EXPECT_EQ(error_message([&] { null_extra.reject_unknown(); }),
            "unknown parameter: extra");

  Args known(mcp::JsonValue::Parse(R"({"path": "x", "count": 1})"), params);
  EXPECT_NO_THROW(known.reject_unknown());

  Args non_object(mcp::JsonValue::Parse("[1, 2]"), params);
  EXPECT_NO_THROW(non_object.reject_unknown());
}

TEST(ArgsTest, SchemaFromVectorParams) {
  const std::vector<ParamDef> params = {
      {"a", "string", "desc", true},
      {"b", "integer", "", false},
  };
  const mcp::JsonValue schema = build_schema(params);

  ASSERT_TRUE(schema.IsObject());
  const mcp::JsonValue *type = schema.Find("type");
  ASSERT_NE(type, nullptr);
  EXPECT_EQ(type->GetString(), "object");
  const mcp::JsonValue *props = schema.Find("properties");
  ASSERT_NE(props, nullptr);
  ASSERT_TRUE(props->IsObject());
  EXPECT_EQ(props->Size(), 2u);
  EXPECT_EQ(props->At("a").At("type").GetString(), "string");
  EXPECT_EQ(props->At("a").At("description").GetString(), "desc");
  EXPECT_EQ(props->At("b").At("type").GetString(), "integer");
  EXPECT_EQ(props->At("b").Find("description"), nullptr);

  const mcp::JsonValue *required = schema.Find("required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsArray());
  ASSERT_EQ(required->Size(), 1u);
  EXPECT_EQ((*required)[0].GetString(), "a");

  const mcp::JsonValue list_schema =
      build_schema({{"a", "string", "desc", true}, {"b", "integer", "", false}});
  EXPECT_TRUE(schema == list_schema);
}
