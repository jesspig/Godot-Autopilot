#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class KeycodeAliasSuite : public IntegrationHarnessWithAllow {
 public:
  KeycodeAliasSuite() : IntegrationHarnessWithAllow("game_runtime") {}
};

mcp::JsonValue make_args() {
  return mcp::JsonValue(mcp::JsonValue::object_tag);
}

void assert_error_and_hint_non_empty(const mcp::JsonValue &result) {
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
  mcp::JsonValue args = make_args();
  args["type"] = mcp::JsonValue("key");
  args["keycode"] = mcp::JsonValue("KEY_P");
  mcp::JsonValue result = call("queue_game_input", args);
  assert_error_and_hint_non_empty(result);
}

TEST_F(KeycodeAliasSuite, QueueGameInputBareKeycodeAcceptedToChannel) {
  mcp::JsonValue args = make_args();
  args["type"] = mcp::JsonValue("key");
  args["keycode"] = mcp::JsonValue("P");
  mcp::JsonValue result = call("queue_game_input", args);
  assert_error_and_hint_non_empty(result);
}

} // namespace
