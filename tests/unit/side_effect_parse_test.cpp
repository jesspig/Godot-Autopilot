#include <gtest/gtest.h>

#include <string>

#include "tools/base/tool_base.hpp"

using godot_autopilot::SideEffect;

namespace {

bool parses_to(const std::string &text, SideEffect expected) {
  SideEffect out = SideEffect::None;
  return godot_autopilot::parse_side_effect(text, out) && out == expected;
}

} // namespace

TEST(SideEffectParseTest, AcceptsDestructiveAndWritesScene) {
  EXPECT_TRUE(parses_to("destructive", SideEffect::Destructive));
  EXPECT_TRUE(parses_to("writes_scene", SideEffect::WritesScene));
}

TEST(SideEffectParseTest, AcceptsAllLegacyNames) {
  EXPECT_TRUE(parses_to("", SideEffect::None));
  EXPECT_TRUE(parses_to("writes_file", SideEffect::WritesFile));
  EXPECT_TRUE(parses_to("writes_config", SideEffect::WritesConfig));
  EXPECT_TRUE(parses_to("shows_alert", SideEffect::ShowsAlert));
  EXPECT_TRUE(parses_to("modifies_window", SideEffect::ModifiesWindow));
  EXPECT_TRUE(parses_to("process", SideEffect::Process));
  EXPECT_TRUE(parses_to("code_execute", SideEffect::CodeExecute));
  EXPECT_TRUE(parses_to("game_runtime", SideEffect::GameRuntime));
}

TEST(SideEffectParseTest, RejectsUnknown) {
  SideEffect out = SideEffect::None;
  EXPECT_FALSE(godot_autopilot::parse_side_effect("unknown", out));
}
