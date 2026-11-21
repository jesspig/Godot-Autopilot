#include "util/hash_util.hpp"
#include "util/input_guards.hpp"
#include "util/string_util.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

TEST(DupHelpersTest, DecimalDigits) {
  using godot_autopilot::util::is_decimal_digits;
  EXPECT_FALSE(is_decimal_digits(""));
  EXPECT_TRUE(is_decimal_digits("0"));
  EXPECT_TRUE(is_decimal_digits("123456789"));
  EXPECT_FALSE(is_decimal_digits("12a"));
  EXPECT_FALSE(is_decimal_digits("-1"));
  EXPECT_FALSE(is_decimal_digits(" 1"));
}

TEST(DupHelpersTest, SignedDecimalParsing) {
  using godot_autopilot::util::parse_signed_decimal_int64;
  int64_t out = 0;
  EXPECT_TRUE(parse_signed_decimal_int64("123", out));
  EXPECT_EQ(out, 123);
  EXPECT_TRUE(parse_signed_decimal_int64("-42", out));
  EXPECT_EQ(out, -42);
  EXPECT_FALSE(parse_signed_decimal_int64("", out));
  EXPECT_FALSE(parse_signed_decimal_int64("-", out));
  EXPECT_FALSE(parse_signed_decimal_int64("12a", out));
  EXPECT_FALSE(parse_signed_decimal_int64("9223372036854775808", out));
}

TEST(DupHelpersTest, StringArgStatus) {
  using godot_autopilot::util::StringArgStatus;
  using godot_autopilot::util::string_arg_status;
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  std::string out = "sentinel";
  EXPECT_EQ(string_arg_status(args, "k", out, true), StringArgStatus::Missing);
  args["k"] = mcp::JsonValue(123);
  EXPECT_EQ(string_arg_status(args, "k", out, true), StringArgStatus::WrongType);
  args["k"] = mcp::JsonValue("");
  EXPECT_EQ(string_arg_status(args, "k", out, true), StringArgStatus::Empty);
  EXPECT_EQ(string_arg_status(args, "k", out, false), StringArgStatus::Ok);
  args["k"] = mcp::JsonValue("value");
  EXPECT_EQ(string_arg_status(args, "k", out, true), StringArgStatus::Ok);
  EXPECT_EQ(out, "value");
}

TEST(DupHelpersTest, RequireNonemptyStringArg) {
  using godot_autopilot::util::require_nonempty_string_arg;
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  std::string out;
  EXPECT_FALSE(require_nonempty_string_arg(args, "k", out));
  args["k"] = mcp::JsonValue("");
  EXPECT_FALSE(require_nonempty_string_arg(args, "k", out));
  args["k"] = mcp::JsonValue("x");
  EXPECT_TRUE(require_nonempty_string_arg(args, "k", out));
  EXPECT_EQ(out, "x");
}

TEST(DupHelpersTest, ResUri) {
  using godot_autopilot::util::res_uri;
  EXPECT_EQ(res_uri("scene.tscn"), "res://scene.tscn");
  EXPECT_EQ(res_uri("res://scene.tscn"), "res://scene.tscn");
  EXPECT_EQ(res_uri("/scene.tscn"), "res:///scene.tscn");
}

TEST(DupHelpersTest, ReceivedKeys) {
  using godot_autopilot::util::received_keys_suffix;
  using godot_autopilot::util::received_keys_text;
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  EXPECT_EQ(received_keys_suffix(args), " (received no parameters)");
  EXPECT_EQ(received_keys_text(args, false), "(received no parameters)");
  args["name"] = mcp::JsonValue("x");
  EXPECT_EQ(received_keys_suffix(args), " (received keys: name)");
  EXPECT_EQ(received_keys_text(args, false), "(received keys: name)");
}

TEST(DupHelpersTest, Fnv1a) {
  using godot_autopilot::util::fnv1a64;
  using godot_autopilot::util::fnv1a64_chars;
  using godot_autopilot::util::fnv1a64_hex;
  using godot_autopilot::util::fnv1a64_hex_printf;
  EXPECT_EQ(fnv1a64(""), 14695981039346656037ULL);
  EXPECT_EQ(fnv1a64_hex(""), "cbf29ce484222325");
  EXPECT_EQ(fnv1a64_hex_printf("hello"), fnv1a64_hex("hello"));
  EXPECT_EQ(fnv1a64_hex("hello").size(), 16u);
  const std::string payload(200, 'x');
  EXPECT_EQ(fnv1a64_chars(payload.data(), payload.size()), fnv1a64(payload));
}

TEST(DupHelpersTest, ObjectLocatorFields) {
  using godot_autopilot::util::LocatorFieldStatus;
  using godot_autopilot::util::ObjectLocatorFields;
  using godot_autopilot::util::parse_locator_string;
  using godot_autopilot::util::parse_object_locator;
  ObjectLocatorFields fields;

  bool out_of_range = false;
  EXPECT_EQ(parse_locator_string("memory://foo", true, fields, out_of_range),
            "memory://foo");
  EXPECT_EQ(fields.name, "foo");
  EXPECT_FALSE(out_of_range);
  fields = ObjectLocatorFields();
  parse_locator_string("res://a/b.tres", true, fields, out_of_range);
  EXPECT_EQ(fields.path, "res://a/b.tres");
  fields = ObjectLocatorFields();
  parse_locator_string("123", true, fields, out_of_range);
  EXPECT_TRUE(fields.has_object_id);
  EXPECT_EQ(fields.object_id, 123);
  fields = ObjectLocatorFields();
  parse_locator_string("-1", true, fields, out_of_range);
  EXPECT_EQ(fields.name, "-1");
  EXPECT_FALSE(fields.has_object_id);

  mcp::JsonValue obj(mcp::JsonValue::object_tag);
  obj["path"] = mcp::JsonValue("res://a.tres");
  obj["object_id_str"] = mcp::JsonValue("456");
  fields = ObjectLocatorFields();
  EXPECT_EQ(parse_object_locator(obj, true, fields), LocatorFieldStatus::Ok);
  EXPECT_EQ(fields.path, "res://a.tres");
  EXPECT_TRUE(fields.has_object_id);
  EXPECT_EQ(fields.object_id, 456);
  obj["bogus"] = mcp::JsonValue(1);
  EXPECT_EQ(parse_object_locator(obj, true, fields),
            LocatorFieldStatus::UnknownField);
}

TEST(DupHelpersTest, JsonEscape) {
  using godot_autopilot::util::append_json_escaped;
  using godot_autopilot::util::json_escape;
  EXPECT_EQ(json_escape("plain"), "plain");
  EXPECT_EQ(json_escape("q\"q"), "q\\\"q");
  EXPECT_EQ(json_escape("back\\slash"), "back\\\\slash");
  EXPECT_EQ(json_escape("a\nb\rc\td"), "a\\nb\\rc\\td");
  EXPECT_EQ(json_escape("\x01"), "\\u0001");
  std::string out = "prefix:";
  append_json_escaped(out, "x\ny");
  EXPECT_EQ(out, "prefix:x\\ny");
}
