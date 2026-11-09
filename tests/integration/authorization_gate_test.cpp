#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class AuthorizationGate : public IntegrationHarnessWithAllow {
 public:
  AuthorizationGate() : IntegrationHarnessWithAllow("none") {}
};

TEST_F(AuthorizationGate, CreateSceneNodeDeniedByDefault) {
  mcp::JsonValue result =
      call("create_scene_node", mcp::JsonValue(mcp::JsonValue::object_tag));
  const auto *error = result.Find("error");
  ASSERT_NE(error, nullptr) << result.Dump();
  ASSERT_TRUE(error->IsString()) << result.Dump();
  EXPECT_FALSE(error->GetString().empty());
  EXPECT_EQ(
      error->GetString(),
      "authorization required for 'create_scene_node'; set "
      "GODOT_AUTOPILOT_ALLOW to include 'write' for trusted local development");
  const auto *code = result.Find("code");
  ASSERT_NE(code, nullptr) << result.Dump();
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "authorization_required");
  const auto *required = result.Find("authorization_required");
  ASSERT_NE(required, nullptr) << result.Dump();
  ASSERT_TRUE(required->IsString());
  EXPECT_EQ(required->GetString(), "write");
}

} // namespace
