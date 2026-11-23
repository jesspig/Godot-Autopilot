#include "tools/base/call_tool_elicitation.hpp"

#include <gtest/gtest.h>

using godot_autopilot::elicitation_detail::effective_side_effect;
using godot_autopilot::elicitation_detail::should_elicit;
using godot_autopilot::SideEffect;

TEST(CallToolElicitationTest, ProxyUsesTargetEffect) {
  EXPECT_EQ(effective_side_effect(true, SideEffect::None, SideEffect::Destructive),
            SideEffect::Destructive);
  EXPECT_EQ(effective_side_effect(true, SideEffect::None, SideEffect::Process),
            SideEffect::Process);
}

TEST(CallToolElicitationTest, DirectUsesSurfaceEffect) {
  EXPECT_EQ(effective_side_effect(false, SideEffect::Destructive, SideEffect::None),
            SideEffect::Destructive);
}

TEST(CallToolElicitationTest, HighRiskWithoutConfirmAndWithCapabilityElicits) {
  EXPECT_TRUE(should_elicit(SideEffect::Destructive, false, true));
  EXPECT_TRUE(should_elicit(SideEffect::Process, false, true));
  EXPECT_TRUE(should_elicit(SideEffect::CodeExecute, false, true));
  EXPECT_TRUE(should_elicit(SideEffect::GameRuntime, false, true));
}

TEST(CallToolElicitationTest, ConfirmOrMissingCapabilitySkipsElicitation) {
  EXPECT_FALSE(should_elicit(SideEffect::Destructive, true, true));
  EXPECT_FALSE(should_elicit(SideEffect::Destructive, false, false));
}

TEST(CallToolElicitationTest, LowRiskNeverElicits) {
  EXPECT_FALSE(should_elicit(SideEffect::None, false, true));
  EXPECT_FALSE(should_elicit(SideEffect::WritesFile, false, true));
  EXPECT_FALSE(should_elicit(SideEffect::WritesScene, false, true));
}
