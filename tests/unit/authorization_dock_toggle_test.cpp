#include "tools/authorization.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>

#include "core/sanitize_policy.hpp"
#include "tools/tool_base.hpp"
#include "ui/authorization_switches.hpp"

namespace {

using godot_autopilot::SideEffect;
namespace authorization = godot_autopilot::authorization;
namespace authorization_switches = godot_autopilot::authorization_switches;
namespace sanitize_policy = godot_autopilot::sanitize_policy;

std::string &provider_state() {
  static std::string value;
  return value;
}

std::string read_provider_state() { return provider_state(); }

class AuthorizationDockToggleTest : public ::testing::Test {
protected:
  void SetUp() override {
    previous_provider_ = authorization::allow_provider();
    previous_mode_provider_ = authorization::auth_mode_provider();
    authorization::set_allow_provider(&read_provider_state);
    authorization::set_auth_mode_provider(nullptr);
    provider_state().clear();
    const char *current = std::getenv("GODOT_AUTOPILOT_ALLOW");
    if (current != nullptr) {
      previous_env_ = current;
      had_previous_env_ = true;
    }
    set_allow_env(nullptr);
    const char *current_mode = std::getenv(authorization::kEnvAuthMode);
    if (current_mode != nullptr) {
      previous_mode_env_ = current_mode;
      had_previous_mode_env_ = true;
    }
    set_mode_env(nullptr);
  }

  void TearDown() override {
    set_allow_env(had_previous_env_ ? previous_env_.c_str() : nullptr);
    set_mode_env(had_previous_mode_env_ ? previous_mode_env_.c_str() : nullptr);
    authorization::set_allow_provider(previous_provider_);
    authorization::set_auth_mode_provider(previous_mode_provider_);
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

  static void set_mode_env(const char *value) {
#ifdef _WIN32
    _putenv_s(authorization::kEnvAuthMode, value != nullptr ? value : "");
#else
    if (value != nullptr) {
      setenv(authorization::kEnvAuthMode, value, 1);
    } else {
      unsetenv(authorization::kEnvAuthMode);
    }
#endif
  }

  godot_autopilot::authorization::AllowProvider previous_provider_ = nullptr;
  godot_autopilot::authorization::AuthModeProvider previous_mode_provider_ =
      nullptr;
  std::string previous_env_;
  bool had_previous_env_ = false;
  std::string previous_mode_env_;
  bool had_previous_mode_env_ = false;
};

bool is_known_capability(std::string_view capability) {
  for (const char *known : authorization::kKnownCapabilities) {
    if (capability == known) {
      return true;
    }
  }
  return false;
}

class SanitizePolicyResolutionTest : public ::testing::Test {
protected:
  void SetUp() override {
    previous_enabled_ = sanitize_policy::enabled();
    const char *current = std::getenv(sanitize_policy::kEnvKey);
    if (current != nullptr) {
      previous_env_ = current;
      had_previous_env_ = true;
    }
    set_desensitize_env(nullptr);
  }

  void TearDown() override {
    set_desensitize_env(had_previous_env_ ? previous_env_.c_str() : nullptr);
    sanitize_policy::set_enabled(previous_enabled_);
  }

  static void set_desensitize_env(const char *value) {
#ifdef _WIN32
    _putenv_s(sanitize_policy::kEnvKey, value != nullptr ? value : "");
#else
    if (value != nullptr) {
      setenv(sanitize_policy::kEnvKey, value, 1);
    } else {
      unsetenv(sanitize_policy::kEnvKey);
    }
#endif
  }

  bool previous_enabled_ = true;
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

TEST_F(AuthorizationDockToggleTest, ReadHasNoDockToggle) {
  EXPECT_FALSE(
      authorization::capability_has_dock_toggle(authorization::kCapabilityRead));
}

TEST_F(AuthorizationDockToggleTest, ProcessHasDockToggle) {
  EXPECT_TRUE(
      authorization::capability_has_dock_toggle(authorization::kCapabilityProcess));
}

TEST_F(AuthorizationDockToggleTest, DockToggleSetCoversSixCapabilities) {
  int toggles = 0;
  for (const char *capability : authorization::kKnownCapabilities) {
    if (authorization::capability_has_dock_toggle(capability)) {
      ++toggles;
    }
  }
  EXPECT_EQ(toggles, 6);
  for (const char *capability :
       {"code_execute", "game_runtime", "user_tools", "write", "destructive",
        "process"}) {
    EXPECT_TRUE(authorization::capability_has_dock_toggle(capability))
        << capability;
  }
}

TEST_F(AuthorizationDockToggleTest, DockToggleSetMatchesTheTogglePredicate) {
  for (const char *capability : authorization::kDockToggleCapabilities) {
    EXPECT_TRUE(authorization::capability_has_dock_toggle(capability))
        << capability;
  }
  for (const char *capability : authorization::kKnownCapabilities) {
    bool in_toggle_set = false;
    for (const char *toggle : authorization::kDockToggleCapabilities) {
      if (std::string(toggle) == capability) {
        in_toggle_set = true;
      }
    }
    EXPECT_EQ(in_toggle_set,
              authorization::capability_has_dock_toggle(capability))
        << capability;
  }
}

TEST_F(AuthorizationDockToggleTest, EveryAuthorizableCapabilityHasADockToggle) {
  for (const char *capability : authorization::kKnownCapabilities) {
    const bool read_only = std::string(capability) == authorization::kCapabilityRead;
    EXPECT_EQ(authorization::capability_has_dock_toggle(capability), !read_only)
        << capability;
  }
}

TEST_F(AuthorizationDockToggleTest, DockToggleSetOnlyHoldsKnownCapabilities) {
  for (const char *capability : authorization::kDockToggleCapabilities) {
    EXPECT_TRUE(is_known_capability(capability)) << capability;
  }
}

TEST_F(AuthorizationDockToggleTest, SwitchLabelsCoverTheDockToggleSet) {
  ASSERT_EQ(std::size(authorization_switches::kSwitchLabels),
            std::size(authorization::kDockToggleCapabilities));
  for (const char *capability : authorization::kDockToggleCapabilities) {
    const auto *spec =
        authorization_switches::find_switch_label(capability);
    ASSERT_NE(spec, nullptr) << capability;
    EXPECT_EQ(spec->capability, capability);
    EXPECT_FALSE(spec->label.empty()) << capability;
    EXPECT_FALSE(spec->tooltip.empty()) << capability;
  }
  for (const auto &spec : authorization_switches::kSwitchLabels) {
    EXPECT_TRUE(authorization::capability_has_dock_toggle(spec.capability))
        << spec.capability;
    EXPECT_NE(authorization_switches::find_switch_label(spec.capability),
              nullptr)
        << spec.capability;
  }
}

TEST_F(AuthorizationDockToggleTest, ProcessSwitchExplainsExternalProcessExecution) {
  const auto *spec = authorization_switches::find_switch_label("process");
  ASSERT_NE(spec, nullptr);
  EXPECT_EQ(spec->label, "Allow process:");
  EXPECT_NE(spec->tooltip.find("external"), std::string_view::npos);
  EXPECT_NE(spec->tooltip.find("dangerous"), std::string_view::npos);
}

TEST_F(AuthorizationDockToggleTest, SwitchInitialStateReadsTheEffectiveAllowList) {
  provider_state() = "process,write";
  for (const char *capability : authorization::kDockToggleCapabilities) {
    const bool expected =
        authorization::allow_list_contains("process,write", capability);
    EXPECT_EQ(authorization_switches::switch_initial_state(capability),
              expected)
        << capability;
  }
  provider_state().clear();
  for (const char *capability : authorization::kDockToggleCapabilities) {
    EXPECT_FALSE(authorization_switches::switch_initial_state(capability))
        << capability;
  }
  provider_state() = "all";
  for (const char *capability : authorization::kDockToggleCapabilities) {
    EXPECT_TRUE(authorization_switches::switch_initial_state(capability))
        << capability;
  }
}

TEST_F(AuthorizationDockToggleTest, SwitchInitialStatePrefersTheEnvironmentVariable) {
  provider_state().clear();
  set_allow_env("destructive");
  EXPECT_TRUE(authorization_switches::switch_initial_state("destructive"));
  EXPECT_FALSE(authorization_switches::switch_initial_state("write"));
  EXPECT_FALSE(authorization_switches::switch_initial_state("process"));
  set_allow_env(nullptr);
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

TEST_F(AuthorizationDockToggleTest, UntickingProcessFromAllKeepsTheRest) {
  const std::string allow = authorization::allow_list_remove("all", "process");
  EXPECT_FALSE(authorization::allow_list_contains(allow, "process"));
  EXPECT_FALSE(authorization::allow_list_contains(allow, "all"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "code_execute"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "game_runtime"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "user_tools"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "write"));
  EXPECT_TRUE(authorization::allow_list_contains(allow, "destructive"));
}

TEST_F(AuthorizationDockToggleTest, TickingUnderAllKeepsTheAllEntry) {
  EXPECT_EQ(authorization::allow_list_add("all", "write"), "all");
  EXPECT_EQ(authorization::allow_list_add("all", "destructive"), "all");
  EXPECT_EQ(authorization::allow_list_add("all", "process"), "all");
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

TEST_F(AuthorizationDockToggleTest, DeniedProcessNamesTheDockToggle) {
  const mcp::JsonValue denied = authorization::deny_if_unauthorized(
      "execute_os_process", SideEffect::Process);
  ASSERT_FALSE(denied.IsNull());
  const mcp::JsonValue *required = denied.Find("authorization_required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsString());
  EXPECT_EQ(required->GetString(), "process");
  const mcp::JsonValue *enable = denied.Find("enable");
  ASSERT_NE(enable, nullptr);
  ASSERT_TRUE(enable->IsString());
  EXPECT_NE(enable->GetString().find("Allow process"), std::string::npos);
  EXPECT_NE(enable->GetString().find("MCP Config dock"), std::string::npos);
}

TEST_F(SanitizePolicyResolutionTest, EnvDeclarationWinsOverTheConfigValue) {
  EXPECT_TRUE(sanitize_policy::resolve_enabled("1", false));
  EXPECT_TRUE(sanitize_policy::resolve_enabled("true", false));
  EXPECT_TRUE(sanitize_policy::resolve_enabled("on", false));
  EXPECT_TRUE(sanitize_policy::resolve_enabled("anything", false));
}

TEST_F(SanitizePolicyResolutionTest, ConfigValueAppliesWhenTheEnvIsNotDeclared) {
  EXPECT_FALSE(sanitize_policy::resolve_enabled(nullptr, false));
  EXPECT_TRUE(sanitize_policy::resolve_enabled(nullptr, true));
  EXPECT_FALSE(sanitize_policy::resolve_enabled("", false));
  EXPECT_TRUE(sanitize_policy::resolve_enabled("", true));
}

TEST_F(SanitizePolicyResolutionTest, DisabledEnvValuesTurnThePolicyOff) {
  EXPECT_FALSE(sanitize_policy::resolve_enabled("0", true));
  EXPECT_FALSE(sanitize_policy::resolve_enabled("false", true));
  EXPECT_FALSE(sanitize_policy::resolve_enabled("off", true));
}

TEST_F(SanitizePolicyResolutionTest, EnvDeclaredReportsPresenceAndValue) {
  EXPECT_FALSE(sanitize_policy::detail::env_declared(nullptr).has_value());
  EXPECT_FALSE(sanitize_policy::detail::env_declared("").has_value());
  ASSERT_TRUE(sanitize_policy::detail::env_declared("1").has_value());
  EXPECT_TRUE(sanitize_policy::detail::env_declared("1").value());
  ASSERT_TRUE(sanitize_policy::detail::env_declared("off").has_value());
  EXPECT_FALSE(sanitize_policy::detail::env_declared("off").value());
}

TEST_F(SanitizePolicyResolutionTest, TheProcessEnvValueFeedsTheResolution) {
  EXPECT_STREQ(sanitize_policy::kEnvKey, "GODOT_AUTOPILOT_DESENSITIZE");
  set_desensitize_env("1");
  EXPECT_TRUE(sanitize_policy::resolve_enabled(
      std::getenv(sanitize_policy::kEnvKey), false));
  set_desensitize_env("off");
  EXPECT_FALSE(sanitize_policy::resolve_enabled(
      std::getenv(sanitize_policy::kEnvKey), true));
  set_desensitize_env(nullptr);
  EXPECT_TRUE(sanitize_policy::resolve_enabled(
      std::getenv(sanitize_policy::kEnvKey), true));
  EXPECT_FALSE(sanitize_policy::resolve_enabled(
      std::getenv(sanitize_policy::kEnvKey), false));
}

TEST_F(SanitizePolicyResolutionTest, ResolvingDoesNotChangeTheEffectivePolicy) {
  sanitize_policy::set_enabled(false);
  EXPECT_TRUE(sanitize_policy::resolve_enabled("1", false));
  EXPECT_FALSE(sanitize_policy::enabled());
  sanitize_policy::set_enabled(true);
  EXPECT_FALSE(sanitize_policy::resolve_enabled("off", false));
  EXPECT_TRUE(sanitize_policy::enabled());
}

TEST_F(SanitizePolicyResolutionTest, PanelInitialStateReadLeavesThePolicyUnchanged) {
  sanitize_policy::set_enabled(true);
  const bool enabled_before = sanitize_policy::enabled();
  ASSERT_TRUE(enabled_before);
  const bool panel_state = sanitize_policy::enabled();
  EXPECT_EQ(panel_state, enabled_before);
  EXPECT_EQ(sanitize_policy::enabled(), enabled_before);
  sanitize_policy::set_enabled(false);
  const bool disabled_before = sanitize_policy::enabled();
  ASSERT_FALSE(disabled_before);
  EXPECT_EQ(sanitize_policy::enabled(), disabled_before);
}
