#include <gtest/gtest.h>

#include <string>

#include "tools/base/confirmation.hpp"

using godot_autopilot::SideEffect;
using godot_autopilot::confirmation::error;
using godot_autopilot::confirmation::provided;
using godot_autopilot::confirmation::required_for;

TEST(ConfirmationTest, RequiredForHighRiskSideEffects) {
  EXPECT_FALSE(required_for(SideEffect::None));
  EXPECT_FALSE(required_for(SideEffect::WritesFile));
  EXPECT_FALSE(required_for(SideEffect::WritesConfig));
  EXPECT_FALSE(required_for(SideEffect::ShowsAlert));
  EXPECT_FALSE(required_for(SideEffect::ModifiesWindow));
  EXPECT_FALSE(required_for(SideEffect::WritesScene));
  EXPECT_TRUE(required_for(SideEffect::Destructive));
  EXPECT_TRUE(required_for(SideEffect::Process));
  EXPECT_TRUE(required_for(SideEffect::CodeExecute));
  EXPECT_TRUE(required_for(SideEffect::GameRuntime));
}

TEST(ConfirmationTest, ProvidedReadsBooleanConfirm) {
  godot_autopilot::util::JsonValue missing(godot_autopilot::util::JsonValue::object_tag);
  godot_autopilot::util::JsonValue false_value(godot_autopilot::util::JsonValue::object_tag);
  false_value["confirm"] = godot_autopilot::util::JsonValue(false);
  godot_autopilot::util::JsonValue true_value(godot_autopilot::util::JsonValue::object_tag);
  true_value["confirm"] = godot_autopilot::util::JsonValue(true);
  EXPECT_FALSE(provided(missing));
  EXPECT_FALSE(provided(false_value));
  EXPECT_TRUE(provided(true_value));
  EXPECT_FALSE(provided(godot_autopilot::util::JsonValue("confirm")));
}

TEST(ConfirmationTest, ErrorCarriesStructuredCodeAndTier) {
  const godot_autopilot::util::JsonValue result = error("delete_scene_node", SideEffect::Destructive);
  const godot_autopilot::util::JsonValue *code = result.Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "confirmation_required");
  const godot_autopilot::util::JsonValue *required = result.Find("confirm_required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsBool());
  EXPECT_TRUE(required->GetBool());
  const godot_autopilot::util::JsonValue *effect = result.Find("side_effect");
  ASSERT_NE(effect, nullptr);
  ASSERT_TRUE(effect->IsString());
  EXPECT_EQ(effect->GetString(), "destructive");
}
