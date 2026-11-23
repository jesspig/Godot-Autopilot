#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/resource_registry.hpp"

namespace {

using godot_autopilot::resource_registry::is_object_id_key;
using godot_autopilot::resource_registry::name_key_points_to;
using godot_autopilot::resource_registry::oid_key;
using godot_autopilot::resource_registry::stale_name_keys;

} // namespace

TEST(ResourceRegistryTest, ObjectIdKeyAcceptsNegativeIntegers) {
  constexpr int64_t negative = -9223370460652037000LL;
  EXPECT_TRUE(is_object_id_key(oid_key(negative)));
  EXPECT_EQ(oid_key(negative), std::to_string(negative));
  EXPECT_TRUE(is_object_id_key("-1"));
  EXPECT_TRUE(is_object_id_key("0"));
  EXPECT_TRUE(is_object_id_key("9223370460652037000"));
}

TEST(ResourceRegistryTest, ObjectIdKeyRejectsNamesAndMalformedNumbers) {
  EXPECT_FALSE(is_object_id_key("name:Player"));
  EXPECT_FALSE(is_object_id_key("-"));
  EXPECT_FALSE(is_object_id_key("--1"));
  EXPECT_FALSE(is_object_id_key("12a"));
  EXPECT_FALSE(is_object_id_key("+1"));
  EXPECT_FALSE(is_object_id_key(""));
}

TEST(ResourceRegistryTest, NameKeyMatchesOnlyMatchingInstance) {
  EXPECT_TRUE(name_key_points_to("name:Enemy", 42, 42));
  EXPECT_FALSE(name_key_points_to("name:Enemy", 42, 43));
  EXPECT_FALSE(name_key_points_to("42", 42, 42));
  EXPECT_FALSE(name_key_points_to("name:", 42, 42));
  EXPECT_FALSE(name_key_points_to("nam:Enemy", 42, 42));
}

TEST(ResourceRegistryTest, StaleNameKeysSelectsMatchingNamedEntries) {
  const std::vector<std::pair<std::string, int64_t>> entries = {
      {"name:Enemy", 42},
      {"name:Player", 7},
      {"42", 42},
      {"7", 7},
      {"name:Ghost", 42},
  };
  const std::vector<std::string> stale = stale_name_keys(entries, 42);
  ASSERT_EQ(stale.size(), 2u);
  EXPECT_EQ(stale[0], "name:Enemy");
  EXPECT_EQ(stale[1], "name:Ghost");
}

TEST(ResourceRegistryTest, StaleNameKeysEmptyWhenNoMatch) {
  const std::vector<std::pair<std::string, int64_t>> entries = {
      {"name:Player", 7}, {"7", 7},
  };
  EXPECT_TRUE(stale_name_keys(entries, 42).empty());
}
