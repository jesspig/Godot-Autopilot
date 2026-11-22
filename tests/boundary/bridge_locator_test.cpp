#include "tools/domains/csharp_bridge/bridge_locator.hpp"

#include <gtest/gtest.h>

using godot_autopilot::csharp_bridge_detail::bridge_ready_expression;

TEST(BridgeLocatorTest, ReadyRequiresBridgeMetaOnTheLocatedHolder) {
  const std::string expr = bridge_ready_expression("__gda_holder");
  EXPECT_NE(expr.find("__gda_holder != null"), std::string::npos);
  EXPECT_NE(expr.find("__gda_holder.has_meta(\"bridge\")"), std::string::npos);
  EXPECT_NE(expr.find("__gda_holder.get_meta(\"bridge\") != null"), std::string::npos);
}

TEST(BridgeLocatorTest, ReadyExpressionUsesTheGivenVariable) {
  const std::string expr = bridge_ready_expression("node_x");
  EXPECT_NE(expr.find("node_x != null"), std::string::npos);
  EXPECT_EQ(expr.find("__gda_holder"), std::string::npos);
}
