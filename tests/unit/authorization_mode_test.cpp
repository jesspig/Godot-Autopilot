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

std::string &mode_state() {
  static std::string value;
  return value;
}

std::string read_mode_state() { return mode_state(); }

class AuthorizationModeTest : public ::testing::Test {
protected:
  void SetUp() override {
    previous_provider_ = authorization::allow_provider();
    previous_mode_provider_ = authorization::auth_mode_provider();
    authorization::set_allow_provider(nullptr);
    authorization::set_auth_mode_provider(nullptr);
    mode_state().clear();
    mode_env_.reset(new EnvScope(authorization::kEnvAuthMode, nullptr));
    allow_env_.reset(new EnvScope("GODOT_AUTOPILOT_ALLOW", nullptr));
  }

  void TearDown() override {
    mode_env_.reset();
    allow_env_.reset();
    mode_state().clear();
    authorization::set_allow_provider(previous_provider_);
    authorization::set_auth_mode_provider(previous_mode_provider_);
  }

  godot_autopilot::authorization::AllowProvider previous_provider_ = nullptr;
  godot_autopilot::authorization::AuthModeProvider previous_mode_provider_ =
      nullptr;
  std::unique_ptr<EnvScope> mode_env_;
  std::unique_ptr<EnvScope> allow_env_;
};

} // namespace

TEST_F(AuthorizationModeTest, StrictModeIsTheDefault) {
  EXPECT_EQ(authorization::current_auth_mode(), authorization::AuthMode::Strict);
  EXPECT_STREQ(authorization::auth_mode_name(authorization::AuthMode::Strict),
               "strict");
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "save_resource", SideEffect::WritesFile);
  EXPECT_FALSE(denied.IsNull());
  const mcp::JsonValue *required = denied.Find("authorization_required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsString());
  EXPECT_EQ(required->GetString(), "write");
}

TEST_F(AuthorizationModeTest, LegacyModeAllowsWriteTools) {
  EnvScope scope(authorization::kEnvAuthMode, "legacy");
  EXPECT_EQ(authorization::current_auth_mode(), authorization::AuthMode::Legacy);
  EXPECT_TRUE(authorization::tier_permits(authorization::kCapabilityWrite, "",
                                           authorization::AuthMode::Legacy));
  EXPECT_TRUE(authorization::deny_if_unauthorized("save_resource",
                                                   SideEffect::WritesFile)
                  .IsNull());
  EXPECT_TRUE(authorization::deny_if_unauthorized("show_display_dialog",
                                                   SideEffect::ShowsAlert)
                  .IsNull());
}

TEST_F(AuthorizationModeTest, LegacyModeStillDeniesDangerTools) {
  EnvScope scope(authorization::kEnvAuthMode, "legacy");
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "create_os_process", SideEffect::Process);
  ASSERT_FALSE(denied.IsNull());
  const mcp::JsonValue *required = denied.Find("authorization_required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsString());
  EXPECT_EQ(required->GetString(), "process");
}

TEST_F(AuthorizationModeTest, LegacyModeStillDeniesDestructiveTools) {
  EnvScope scope(authorization::kEnvAuthMode, "legacy");
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "delete_scene_node", SideEffect::Destructive);
  ASSERT_FALSE(denied.IsNull());
  const mcp::JsonValue *required = denied.Find("authorization_required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsString());
  EXPECT_EQ(required->GetString(), "destructive");
}

TEST_F(AuthorizationModeTest, InvalidModeFallsBackToStrict) {
  EnvScope scope(authorization::kEnvAuthMode, "relaxed");
  EXPECT_EQ(authorization::current_auth_mode(), authorization::AuthMode::Strict);
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "save_resource", SideEffect::WritesFile);
  EXPECT_FALSE(denied.IsNull());
}

TEST_F(AuthorizationModeTest, InvalidConfigValueFallsBackToStrict) {
  authorization::set_auth_mode_provider(&read_mode_state);
  mode_state() = "compat";
  EXPECT_EQ(authorization::current_auth_mode(), authorization::AuthMode::Strict);
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "save_resource", SideEffect::WritesFile);
  EXPECT_FALSE(denied.IsNull());
}

TEST_F(AuthorizationModeTest, ConfigValueIsUsedWhenEnvironmentIsUnset) {
  authorization::set_auth_mode_provider(&read_mode_state);
  mode_state() = "legacy";
  EXPECT_EQ(authorization::current_auth_mode(), authorization::AuthMode::Legacy);
  EXPECT_TRUE(authorization::deny_if_unauthorized("save_resource",
                                                   SideEffect::WritesFile)
                  .IsNull());
}

TEST_F(AuthorizationModeTest, EmptyConfigValueFallsBackToStrict) {
  authorization::set_auth_mode_provider(&read_mode_state);
  mode_state().clear();
  EXPECT_EQ(authorization::current_auth_mode(), authorization::AuthMode::Strict);
}

TEST_F(AuthorizationModeTest, EnvironmentVariableWinsOverConfig) {
  authorization::set_auth_mode_provider(&read_mode_state);
  mode_state() = "legacy";
  EnvScope scope(authorization::kEnvAuthMode, "strict");
  EXPECT_EQ(authorization::current_auth_mode(), authorization::AuthMode::Strict);
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "save_resource", SideEffect::WritesFile);
  EXPECT_FALSE(denied.IsNull());
}

TEST_F(AuthorizationModeTest, DeniedWriteExplainsTheCompatibilityMode) {
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "save_resource", SideEffect::WritesFile);
  ASSERT_FALSE(denied.IsNull());
  const mcp::JsonValue *mode = denied.Find("authorization_mode");
  ASSERT_NE(mode, nullptr);
  ASSERT_TRUE(mode->IsString());
  EXPECT_EQ(mode->GetString(), "strict");
  const mcp::JsonValue *enable = denied.Find("enable");
  ASSERT_NE(enable, nullptr);
  ASSERT_TRUE(enable->IsString());
  EXPECT_NE(enable->GetString().find("legacy"), std::string::npos);
  EXPECT_NE(enable->GetString().find("Compatibility"), std::string::npos);
  EXPECT_NE(enable->GetString().find(authorization::kEnvAuthMode),
            std::string::npos);
}

TEST_F(AuthorizationModeTest, DeniedDangerKeepsTheOriginalHintInLegacyMode) {
  EnvScope scope(authorization::kEnvAuthMode, "legacy");
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "create_os_process", SideEffect::Process);
  ASSERT_FALSE(denied.IsNull());
  const mcp::JsonValue *mode = denied.Find("authorization_mode");
  ASSERT_NE(mode, nullptr);
  ASSERT_TRUE(mode->IsString());
  EXPECT_EQ(mode->GetString(), "legacy");
  const mcp::JsonValue *enable = denied.Find("enable");
  ASSERT_NE(enable, nullptr);
  ASSERT_TRUE(enable->IsString());
  EXPECT_EQ(enable->GetString().find("Compatibility"), std::string::npos);
  EXPECT_NE(enable->GetString().find("Allow process"), std::string::npos);
}

TEST_F(AuthorizationModeTest, ParseAuthModeAcceptsOnlyKnownValues) {
  authorization::AuthMode mode = authorization::AuthMode::Legacy;
  EXPECT_TRUE(authorization::parse_auth_mode("strict", mode));
  EXPECT_EQ(mode, authorization::AuthMode::Strict);
  EXPECT_TRUE(authorization::parse_auth_mode("legacy", mode));
  EXPECT_EQ(mode, authorization::AuthMode::Legacy);
  EXPECT_FALSE(authorization::parse_auth_mode("Legacy", mode));
  EXPECT_FALSE(authorization::parse_auth_mode("", mode));
  EXPECT_FALSE(authorization::parse_auth_mode("all", mode));
}

TEST_F(AuthorizationModeTest, ReadTierStaysAllowedInEveryMode) {
  EXPECT_TRUE(authorization::tier_permits(authorization::kCapabilityRead, "",
                                           authorization::AuthMode::Strict));
  EXPECT_TRUE(authorization::tier_permits(authorization::kCapabilityRead, "",
                                           authorization::AuthMode::Legacy));
  EXPECT_TRUE(authorization::deny_if_unauthorized("get_scene_tree",
                                                   SideEffect::None)
                  .IsNull());
}
