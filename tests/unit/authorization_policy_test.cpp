#include "tools/authorization.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <memory>
#include <string>

#include "tools/tool_base.hpp"

namespace {

using godot_autopilot::SideEffect;
namespace authorization = godot_autopilot::authorization;

class EnvScope {
public:
  EnvScope(const char *key, const char *value) : key_(key) {
    const char *current = std::getenv(key);
    if (current) {
      previous_ = current;
      had_previous_ = true;
    }
    apply(value);
  }

  ~EnvScope() { apply(had_previous_ ? previous_.c_str() : nullptr); }

private:
  void apply(const char *value) {
#ifdef _WIN32
    _putenv_s(key_.c_str(), value ? value : "");
#else
    if (value) {
      setenv(key_.c_str(), value, 1);
    } else {
      unsetenv(key_.c_str());
    }
#endif
  }

  std::string key_;
  std::string previous_;
  bool had_previous_ = false;
};

class AuthorizationPolicyTest : public ::testing::Test {
protected:
  void SetUp() override {
    previous_provider_ = authorization::allow_provider();
    previous_mode_provider_ = authorization::auth_mode_provider();
    authorization::set_allow_provider(nullptr);
    authorization::set_auth_mode_provider(nullptr);
    env_.reset(new EnvScope("GODOT_AUTOPILOT_ALLOW", nullptr));
    mode_env_.reset(new EnvScope(authorization::kEnvAuthMode, nullptr));
  }

  void TearDown() override {
    env_.reset();
    mode_env_.reset();
    authorization::set_allow_provider(previous_provider_);
    authorization::set_auth_mode_provider(previous_mode_provider_);
  }

  godot_autopilot::authorization::AllowProvider previous_provider_ = nullptr;
  godot_autopilot::authorization::AuthModeProvider previous_mode_provider_ =
      nullptr;
  std::unique_ptr<EnvScope> env_;
  std::unique_ptr<EnvScope> mode_env_;
};

} // namespace

TEST_F(AuthorizationPolicyTest, ReadTierIsAllowedByDefault) {
  const mcp::JsonValue denied =
      authorization::deny_if_unauthorized("get_scene_tree", SideEffect::None);
  EXPECT_TRUE(denied.IsNull());
  EXPECT_TRUE(authorization::tier_permits(authorization::kCapabilityRead, ""));
}

TEST_F(AuthorizationPolicyTest, WriteTierIsDeniedByDefault) {
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "save_resource", SideEffect::WritesFile);
  ASSERT_FALSE(denied.IsNull());
  const mcp::JsonValue *error = denied.Find("error");
  ASSERT_NE(error, nullptr);
  ASSERT_TRUE(error->IsString());
  EXPECT_NE(error->GetString().find("save_resource"), std::string::npos);
  const mcp::JsonValue *required = denied.Find("authorization_required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsString());
  EXPECT_EQ(required->GetString(), "write");
  const mcp::JsonValue *code = denied.Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "authorization_required");
  const mcp::JsonValue *enable = denied.Find("enable");
  ASSERT_NE(enable, nullptr);
  ASSERT_TRUE(enable->IsString());
  EXPECT_NE(enable->GetString().find("GODOT_AUTOPILOT_ALLOW"),
            std::string::npos);
  EXPECT_NE(enable->GetString().find("write"), std::string::npos);
}

TEST_F(AuthorizationPolicyTest, SceneWriteTierIsDeniedByDefault) {
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "property_set", SideEffect::WritesScene);
  ASSERT_FALSE(denied.IsNull());
  const mcp::JsonValue *required = denied.Find("authorization_required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsString());
  EXPECT_EQ(required->GetString(), "write");
  EXPECT_STREQ(godot_autopilot::side_effect_name(SideEffect::WritesScene),
               "writes_scene");
}

TEST_F(AuthorizationPolicyTest, WriteTierIsAllowedWhenListed) {
  EnvScope scope("GODOT_AUTOPILOT_ALLOW", "write");
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "save_resource", SideEffect::WritesFile);
  EXPECT_TRUE(denied.IsNull());
  EXPECT_TRUE(authorization::tier_permits(authorization::kCapabilityWrite,
                                          "write"));
  EXPECT_FALSE(authorization::tier_permits(authorization::kCapabilityWrite, ""));
}

TEST_F(AuthorizationPolicyTest, WriteTierIsDeniedWhenOnlyOtherTiersListed) {
  EnvScope scope("GODOT_AUTOPILOT_ALLOW", "process,code_execute");
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "save_resource", SideEffect::WritesFile);
  EXPECT_FALSE(denied.IsNull());
}

TEST_F(AuthorizationPolicyTest, DangerTierIsDeniedByDefault) {
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "create_os_process", SideEffect::Process);
  ASSERT_FALSE(denied.IsNull());
  const mcp::JsonValue *required = denied.Find("authorization_required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsString());
  EXPECT_EQ(required->GetString(), "process");
}

TEST_F(AuthorizationPolicyTest, AllKeywordUnlocksEveryTier) {
  EnvScope scope("GODOT_AUTOPILOT_ALLOW", "all");
  EXPECT_TRUE(authorization::deny_if_unauthorized("get_scene_tree",
                                                  SideEffect::None)
                  .IsNull());
  EXPECT_TRUE(authorization::deny_if_unauthorized("save_resource",
                                                  SideEffect::WritesFile)
                  .IsNull());
  EXPECT_TRUE(authorization::deny_if_unauthorized("create_os_process",
                                                  SideEffect::Process)
                  .IsNull());
  EXPECT_TRUE(authorization::deny_if_unauthorized("code_execute",
                                                  SideEffect::CodeExecute)
                  .IsNull());
  EXPECT_TRUE(authorization::deny_if_unauthorized("execute_game_script",
                                                  SideEffect::GameRuntime)
                  .IsNull());
}

TEST_F(AuthorizationPolicyTest, AllowListAllowsEveryTier) {
  EXPECT_TRUE(authorization::tier_permits(authorization::kCapabilityRead, "all"));
  EXPECT_TRUE(
      authorization::tier_permits(authorization::kCapabilityWrite, "all"));
  EXPECT_TRUE(
      authorization::tier_permits(authorization::kCapabilityProcess, "all"));
  EXPECT_TRUE(authorization::tier_permits(authorization::kCapabilityCodeExecute,
                                          "all"));
  EXPECT_TRUE(authorization::tier_permits(authorization::kCapabilityGameRuntime,
                                          "all"));
}

TEST_F(AuthorizationPolicyTest, AllowProviderDrivesTheDecision) {
  EnvScope scope("GODOT_AUTOPILOT_ALLOW", nullptr);
  authorization::set_allow_provider([]() { return std::string("write"); });
  EXPECT_TRUE(authorization::deny_if_unauthorized("save_resource",
                                                  SideEffect::WritesFile)
                  .IsNull());
  EXPECT_FALSE(authorization::deny_if_unauthorized("create_os_process",
                                                   SideEffect::Process)
                   .IsNull());
}

TEST_F(AuthorizationPolicyTest, CapabilityNameMapsToTier) {
  EXPECT_STREQ(authorization::capability_for_tool("get_scene_tree",
                                                   SideEffect::None),
               "read");
  EXPECT_STREQ(authorization::capability_for_tool("save_resource",
                                                   SideEffect::WritesFile),
               "write");
  EXPECT_STREQ(authorization::capability_for_tool("save_project_settings",
                                                   SideEffect::WritesConfig),
               "write");
  EXPECT_STREQ(authorization::capability_for_tool("show_display_dialog",
                                                   SideEffect::ShowsAlert),
               "write");
  EXPECT_STREQ(authorization::capability_for_tool("warp_display_mouse",
                                                   SideEffect::ModifiesWindow),
               "write");
  EXPECT_STREQ(authorization::capability_for_tool("property_set",
                                                   SideEffect::WritesScene),
               "write");
  EXPECT_STREQ(authorization::capability_for_tool("create_os_process",
                                                   SideEffect::Process),
               "process");
  EXPECT_STREQ(authorization::capability_for_tool("code_execute",
                                                   SideEffect::CodeExecute),
               "code_execute");
  EXPECT_STREQ(authorization::capability_for_tool("execute_game_script",
                                                   SideEffect::GameRuntime),
               "game_runtime");
}

TEST_F(AuthorizationPolicyTest, SideEffectMapsToTier) {
  EXPECT_EQ(authorization::tier_for_side_effect(SideEffect::None),
            authorization::CapabilityTier::Read);
  EXPECT_EQ(authorization::tier_for_side_effect(SideEffect::WritesFile),
            authorization::CapabilityTier::Write);
  EXPECT_EQ(authorization::tier_for_side_effect(SideEffect::WritesConfig),
            authorization::CapabilityTier::Write);
  EXPECT_EQ(authorization::tier_for_side_effect(SideEffect::ShowsAlert),
            authorization::CapabilityTier::Write);
  EXPECT_EQ(authorization::tier_for_side_effect(SideEffect::ModifiesWindow),
            authorization::CapabilityTier::Write);
  EXPECT_EQ(authorization::tier_for_side_effect(SideEffect::WritesScene),
            authorization::CapabilityTier::Write);
  EXPECT_EQ(authorization::tier_for_side_effect(SideEffect::Process),
            authorization::CapabilityTier::Danger);
  EXPECT_EQ(authorization::tier_for_side_effect(SideEffect::CodeExecute),
            authorization::CapabilityTier::Danger);
  EXPECT_EQ(authorization::tier_for_side_effect(SideEffect::GameRuntime),
            authorization::CapabilityTier::Danger);
}

TEST_F(AuthorizationPolicyTest, UnknownCapabilityNameIsPermitted) {
  EXPECT_TRUE(authorization::tier_permits("", ""));
  EXPECT_TRUE(authorization::tier_permits("", "write"));
}
