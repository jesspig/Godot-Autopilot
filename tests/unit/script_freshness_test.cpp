// script_freshness_test.cpp — 脚本取用口径（cache mode 选择 / fresh 标志判定）的 L1 覆盖
//
// 判定实现在 src/tools/script_ops.cpp：
//   - script_load_cache_mode(bool)：决定 ResourceLoader::load() 的 p_cache_mode。
//     godot-cpp 生成头的缺省实参是 (CacheMode)1 = CACHE_MODE_REUSE —— 命中
//     ResourceCache 时直接返回旧实例、不读盘（core/io/resource_loader.cpp:800-809），
//     故「从磁盘刷新」必须显式传 (=0) CACHE_MODE_IGNORE。
//   - wants_fresh_load(args)：只读工具（load_script / get_script_property /
//     get_script_property_list）的可选 fresh 参数口径，缺省 false 保持既有
//     REUSE 语义，只认字面 true。
// 两者都不触碰 Godot 对象（只命名引擎枚举常量），可纯单元测试。

#include "tools/script_ops.hpp"

#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <string_view>

namespace godot_autopilot {
namespace script_ops {

// 定义于 src/tools/script_ops.cpp（script_ops.hpp 只声明工具 handler）。
int script_load_cache_mode(bool fresh);
bool wants_fresh_load(const mcp::JsonValue &args);

} // namespace script_ops
} // namespace godot_autopilot

using godot_autopilot::script_ops::script_load_cache_mode;
using godot_autopilot::script_ops::wants_fresh_load;

namespace {

// Godot 4.7 ResourceLoader::CacheMode（core/io/resource_loader.h）与 godot-cpp 生成头
// resource_loader.hpp 一致：IGNORE = 0、REUSE = 1。用字面量把数值契约钉在测试里，
// 避免 L1 测试引入 Godot 绑定头。
constexpr int kCacheModeIgnore = 0;
constexpr int kCacheModeReuse = 1;

mcp::JsonValue args_with(std::string_view key, const mcp::JsonValue &value) {
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  args[key] = value;
  return args;
}

} // namespace

TEST(ScriptFreshnessTest, CachedLoadUsesReuseCacheMode) {
  // 缺省口径等于 godot-cpp 的缺省实参：返回缓存实例、不读盘。
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
