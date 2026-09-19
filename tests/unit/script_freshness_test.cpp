
#include "tools/script_ops.hpp"

#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <string_view>

namespace godot_autopilot {
namespace script_ops {

int script_load_cache_mode(bool fresh);
bool wants_fresh_load(const mcp::JsonValue &args);

} // namespace script_ops
} // namespace godot_autopilot

using godot_autopilot::script_ops::script_load_cache_mode;
using godot_autopilot::script_ops::wants_fresh_load;

namespace {

constexpr int kCacheModeIgnore = 0;
constexpr int kCacheModeReuse = 1;

mcp::JsonValue args_with(std::string_view key, const mcp::JsonValue &value) {
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  args[key] = value;
  return args;
}

} // namespace

TEST(ScriptFreshnessTest, CachedLoadUsesReuseCacheMode) {
  EXPECT_EQ(kCacheModeReuse, script_load_cache_mode(false));
}

TEST(ScriptFreshnessTest, FreshLoadBypassesResourceCache) {
  EXPECT_EQ(kCacheModeIgnore, script_load_cache_mode(true));
}

TEST(ScriptFreshnessTest, FreshFlagDefaultsToFalse) {
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  EXPECT_FALSE(wants_fresh_load(args));
}

TEST(ScriptFreshnessTest, FreshFlagTrueEnablesDiskReload) {
  EXPECT_TRUE(wants_fresh_load(args_with("fresh", mcp::JsonValue(true))));
}

TEST(ScriptFreshnessTest, FreshFlagFalseKeepsCachedSemantics) {
  EXPECT_FALSE(wants_fresh_load(args_with("fresh", mcp::JsonValue(false))));
}

TEST(ScriptFreshnessTest, FreshFlagIgnoresNonBooleanValues) {
  EXPECT_FALSE(wants_fresh_load(args_with("fresh", mcp::JsonValue(1))));
  EXPECT_FALSE(wants_fresh_load(args_with("fresh", mcp::JsonValue("true"))));
  EXPECT_FALSE(wants_fresh_load(args_with("fresh", mcp::JsonValue())));
}

TEST(ScriptFreshnessTest, UnrelatedArgumentsKeepCachedSemantics) {
  EXPECT_FALSE(
      wants_fresh_load(args_with("path", mcp::JsonValue("res://probe.gd"))));
}
