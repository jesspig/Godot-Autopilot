#include <gtest/gtest.h>

#include "util/json_value.hpp"

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class KeycodeAliasSuite : public IntegrationHarnessWithAllow {
 public:
  KeycodeAliasSuite() : IntegrationHarnessWithAllow("game_runtime") {}
};

godot_autopilot::util::JsonValue make_args() {
  return godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
}

void assert_error_and_hint_non_empty(const godot_autopilot::util::JsonValue &result) {
  const auto *error = result.Find("error");
  ASSERT_NE(error, nullptr) << result.Dump();
  ASSERT_TRUE(error->IsString()) << result.Dump();
  EXPECT_FALSE(error->GetString().empty());
  const auto *hint = result.Find("hint");
  ASSERT_NE(hint, nullptr) << result.Dump();
  ASSERT_TRUE(hint->IsString()) << result.Dump();
  EXPECT_FALSE(hint->GetString().empty());
}

TEST_F(KeycodeAliasSuite, QueueGameInputPrefixedKeycodeAcceptedToChannel) {
  godot_autopilot::util::JsonValue args = make_args();
  args["type"] = godot_autopilot::util::JsonValue("key");
  args["keycode"] = godot_autopilot::util::JsonValue("KEY_P");
  godot_autopilot::util::JsonValue result = call("queue_game_input", args);
  assert_error_and_hint_non_empty(result);
}

TEST_F(KeycodeAliasSuite, QueueGameInputBareKeycodeAcceptedToChannel) {
  godot_autopilot::util::JsonValue args = make_args();
  args["type"] = godot_autopilot::util::JsonValue("key");
  args["keycode"] = godot_autopilot::util::JsonValue("P");
  godot_autopilot::util::JsonValue result = call("queue_game_input", args);
  assert_error_and_hint_non_empty(result);
}

} // namespace
