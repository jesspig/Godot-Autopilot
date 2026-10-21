#include "tools/authorization.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "tools/tool_base.hpp"

namespace {

using godot_autopilot::SideEffect;
namespace authorization = godot_autopilot::authorization;

std::string &provider_state() {
  static std::string value;
  return value;
}

std::string read_provider_state() { return provider_state(); }

class AuthorizationDockToggleTest : public ::testing::Test {
protected:
  void SetUp() override {
    previous_provider_ = authorization::allow_provider();
    authorization::set_allow_provider(&read_provider_state);
    provider_state().clear();
    const char *current = std::getenv("GODOT_AUTOPILOT_ALLOW");
    if (current != nullptr) {
      previous_env_ = current;
      had_previous_env_ = true;
    }
    set_allow_env(nullptr);
  }

  void TearDown() override {
    set_allow_env(had_previous_env_ ? previous_env_.c_str() : nullptr);
    authorization::set_allow_provider(previous_provider_);
    provider_state().clear();
  }

  static void set_allow_env(const char *value) {
#ifdef _WIN32
    _putenv_s("GODOT_AUTOPILOT_ALLOW", value != nullptr ? value : "");
#else
    if (value != nullptr) {
      setenv("GODOT_AUTOPILOT_ALLOW", value, 1);
    } else {
      unsetenv("GODOT_AUTOPILOT_ALLOW");
    }
#endif
  }

  godot_autopilot::authorization::AllowProvider previous_provider_ = nullptr;
  std::string previous_env_;
  bool had_previous_env_ = false;
};

} // namespace

TEST_F(AuthorizationDockToggleTest, WriteAndDestructiveHaveDockToggles) {
  EXPECT_TRUE(
      authorization::capability_has_dock_toggle(authorization::kCapabilityWrite));
  EXPECT_TRUE(authorization::capability_has_dock_toggle(
      authorization::kCapabilityDestructive));
}

TEST_F(AuthorizationDockToggleTest, ReadAndProcessHaveNoDockToggle) {
  EXPECT_FALSE(
      authorization::capability_has_dock_toggle(authorization::kCapabilityRead));
  EXPECT_FALSE(
      authorization::capability_has_dock_toggle(authorization::kCapabilityProcess));
}

TEST_F(AuthorizationDockToggleTest, DockToggleSetCoversFiveCapabilities) {
  int toggles = 0;
  for (const char *capability : authorization::kKnownCapabilities) {
    if (authorization::capability_has_dock_toggle(capability)) {
      ++toggles;
    }
  }
  EXPECT_EQ(toggles, 5);
  for (const char *capability :
       {"code_execute", "game_runtime", "user_tools", "write",
        "destructive"}) {
    EXPECT_TRUE(authorization::capability_has_dock_toggle(capability))
        << capability;
  }
}

TEST_F(AuthorizationDockToggleTest, TickingWriteAddsItToTheAllowList) {
  const std::string allow = authorization::allow_list_add("", "write");
  EXPECT_EQ(allow, "write");
  EXPECT_TRUE(authorization::allow_list_contains(allow, "write"));
  EXPECT_TRUE(
      authorization::tier_permits(authorization::kCapabilityWrite, allow));
}

TEST_F(AuthorizationDockToggleTest, TickingWriteKeepsExistingEntries) {
  const std::string allow = authorization::allow_list_add("code_execute", "write");
  EXPECT_TRUE(authorization::allow_list_contains(allow, "code_execute"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "write"));
}

TEST_F(AuthorizationDockToggleTest, UntickingWriteRemovesOnlyWrite) {
  const std::string allow =
      authorization::allow_list_remove("write,code_execute", "write");
  EXPECT_EQ(allow, "code_execute");
  EXPECT_FALSE(authorization::allow_list_contains(allow, "write"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "code_execute"));
}

TEST_F(AuthorizationDockToggleTest, UntickingWriteFromAllKeepsTheOtherEntries) {
  const std::string allow = authorization::allow_list_remove("all", "write");
  EXPECT_FALSE(authorization::allow_list_contains(allow, "write"));
  EXPECT_FALSE(authorization::allow_list_contains(allow, "all"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "code_execute"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "game_runtime"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "user_tools"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "destructive"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "process"));
}

TEST_F(AuthorizationDockToggleTest, UntickingDestructiveFromAllKeepsTheRest) {
  const std::string allow =
      authorization::allow_list_remove("all", "destructive");
  EXPECT_FALSE(authorization::allow_list_contains(allow, "destructive"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "write"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "code_execute"));
}

TEST_F(AuthorizationDockToggleTest, TickingUnderAllKeepsTheAllEntry) {
  EXPECT_EQ(authorization::allow_list_add("all", "write"), "all");
  EXPECT_EQ(authorization::allow_list_add("all", "destructive"), "all");
}

TEST_F(AuthorizationDockToggleTest, AllowListIsReadOnEveryCall) {
  provider_state().clear();
  EXPECT_FALSE(authorization::capability_enabled("write"));
  provider_state() = "write";
  EXPECT_TRUE(authorization::capability_enabled("write"));
  provider_state().clear();
  EXPECT_FALSE(authorization::capability_enabled("write"));
}

TEST_F(AuthorizationDockToggleTest, DeniedWriteNamesTheDockToggle) {
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "save_resource", SideEffect::WritesFile);
  ASSERT_FALSE(denied.IsNull());
  const mcp::JsonValue *enable = denied.Find("enable");
  ASSERT_NE(enable, nullptr);
  ASSERT_TRUE(enable->IsString());
  EXPECT_NE(enable->GetString().find("Allow write"), std::string::npos);
  EXPECT_NE(enable->GetString().find("MCP Config dock"), std::string::npos);
}

TEST_F(AuthorizationDockToggleTest, DeniedDestructiveNamesTheDockToggle) {
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "delete_display_window", SideEffect::Destructive);
  ASSERT_FALSE(denied.IsNull());
  const mcp::JsonValue *required = denied.Find("authorization_required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsString());
  EXPECT_EQ(required->GetString(), "destructive");
  const mcp::JsonValue *enable = denied.Find("enable");
  ASSERT_NE(enable, nullptr);
  ASSERT_TRUE(enable->IsString());
  EXPECT_NE(enable->GetString().find("Allow destructive"), std::string::npos);
  EXPECT_NE(enable->GetString().find("MCP Config dock"), std::string::npos);
}
