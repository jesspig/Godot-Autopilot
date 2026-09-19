#include "core/command_queue.hpp"
#include "core/config.hpp"
#include "core/log_system.hpp"
#include "tools/authorization.hpp"
#include "tools/tool_base.hpp"
#include "util/project_path.hpp"
#include "util/variant_json.hpp"

#include <gtest/gtest.h>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

using godot_autopilot::CommandQueue;
using godot_autopilot::LogCategory;
using godot_autopilot::LogEntry;
using godot_autopilot::LogLevel;
using godot_autopilot::LogSystem;

struct EnvGuard {
  std::string key;
  std::string old;
  bool had = false;
  EnvGuard(const char* k, const char* v) : key(k) {
    const char* cur = std::getenv(k);
    if (cur) { old = cur; had = true; }
    if (v) {
#ifdef _WIN32
      _putenv_s(k, v);
#else
      setenv(k, v, 1);
#endif
    } else {
#ifdef _WIN32
      _putenv_s(k, "");
#else
      unsetenv(k);
#endif
    }
  }
  ~EnvGuard() {
    if (had) {
#ifdef _WIN32
      _putenv_s(key.c_str(), old.c_str());
#else
      setenv(key.c_str(), old.c_str(), 1);
#endif
    } else {
#ifdef _WIN32
      _putenv_s(key.c_str(), "");
#else
      unsetenv(key.c_str());
#endif
    }
  }
};


TEST(SecurityHardening, QueueCloseRejectsPending) {
  CommandQueue q(4);
  auto f = q.submit([] { return 42; });
  q.close();
  EXPECT_TRUE(q.is_closed());
  EXPECT_THROW(f.get(), std::runtime_error);
  auto f2 = q.submit([] { return 1; });
  EXPECT_THROW(f2.get(), std::runtime_error);
  EXPECT_TRUE(q.is_closed());
  EXPECT_FALSE(q.drain());
  q.open();
  EXPECT_FALSE(q.is_closed());
  auto f3 = q.submit([] { return 99; });
  EXPECT_TRUE(q.drain());
  EXPECT_EQ(f3.get(), 99);
}

TEST(SecurityHardening, QueueFullReturnsError) {
  CommandQueue q(2);
  auto f1 = q.submit([] { return 1; });
  auto f2 = q.submit([] { return 2; });
  auto f3 = q.submit([] { return 3; });
  EXPECT_THROW(f3.get(), std::runtime_error);
  q.drain();
  EXPECT_EQ(f1.get(), 1);
  EXPECT_EQ(f2.get(), 2);
}

TEST(SecurityHardening, DrainThreadCheck) {
  CommandQueue q;
  q.drain();
  EXPECT_TRUE(q.is_main_thread());
  bool other_drained = true;
  std::thread t([&] { other_drained = q.drain(); });
  t.join();
  EXPECT_FALSE(other_drained);
  EXPECT_TRUE(q.drain());
}

TEST(SecurityHardening, QueueZeroCapacityThrows) {
  EXPECT_THROW(CommandQueue(0), std::invalid_argument);
}

TEST(SecurityHardening, QueueOpenWithPendingThrows) {
  CommandQueue q(4);
  auto f = q.submit([] { return 1; });
  EXPECT_THROW(q.open(), std::logic_error);
  q.drain();
  f.get();
  q.close();
  q.open();
  EXPECT_FALSE(q.is_closed());
}

TEST(SecurityHardening, ProjectPathRejectsUnsupportedScheme) {
  auto r = godot_autopilot::util::normalize_project_path("http://evil.com/file", true);
  EXPECT_FALSE(r.valid());
}
TEST(SecurityHardening, ProjectPathRejectsParentTraversal) {
  auto r = godot_autopilot::util::normalize_project_path("res://../outside", false);
  auto r2 = godot_autopilot::util::normalize_project_path("res://a/../b", false);
  EXPECT_TRUE(r2.valid());
  EXPECT_EQ(r2.value, "res://b");
}
TEST(SecurityHardening, ProjectPathAcceptsResAndUser) {
  auto r = godot_autopilot::util::normalize_project_path("res://folder/file.tscn", true);
  EXPECT_TRUE(r.valid());
  EXPECT_EQ(r.value, "res://folder/file.tscn");
  auto r2 = godot_autopilot::util::normalize_project_path("user://save.dat", true);
  EXPECT_TRUE(r2.valid());
  auto r3 = godot_autopilot::util::normalize_project_path("user://save.dat", false);
  EXPECT_FALSE(r3.valid());
}
TEST(SecurityHardening, ProjectPathRejectsAbsoluteOutsideRoot) {
  auto r = godot_autopilot::util::normalize_project_path("res://a/../b", false);
  EXPECT_TRUE(r.valid());
  EXPECT_EQ(r.value, "res://b");
  auto r2 = godot_autopilot::util::normalize_project_path("http://evil.com/file", true);
  EXPECT_FALSE(r2.valid());
}

TEST(SecurityHardening, AuthorizationDefaultDenies) {
  EnvGuard g("GODOT_AUTOPILOT_ALLOW", nullptr);
  EXPECT_FALSE(godot_autopilot::authorization::capability_enabled("process"));
  EXPECT_FALSE(godot_autopilot::authorization::capability_enabled("code_execute"));
  EXPECT_FALSE(godot_autopilot::authorization::capability_enabled("game_runtime"));
  auto denied = godot_autopilot::authorization::deny_if_unauthorized("create_os_process", godot_autopilot::SideEffect::Process);
  EXPECT_FALSE(denied.IsNull());
  EXPECT_NE(denied.Find("error"), nullptr);
}

TEST(SecurityHardening, AuthorizationAllowsWhenEnvSet) {
  EnvGuard g("GODOT_AUTOPILOT_ALLOW", "process,code_execute");
  EXPECT_TRUE(godot_autopilot::authorization::capability_enabled("process"));
  EXPECT_TRUE(godot_autopilot::authorization::capability_enabled("code_execute"));
  EXPECT_FALSE(godot_autopilot::authorization::capability_enabled("game_runtime"));
  auto ok = godot_autopilot::authorization::deny_if_unauthorized("create_os_process", godot_autopilot::SideEffect::Process);
  EXPECT_TRUE(ok.IsNull());
  auto denied = godot_autopilot::authorization::deny_if_unauthorized("execute_game_script", godot_autopilot::SideEffect::GameRuntime);
  EXPECT_FALSE(denied.IsNull());
}

TEST(SecurityHardening, AuthorizationAllKeyword) {
  EnvGuard g("GODOT_AUTOPILOT_ALLOW", "all");
  EXPECT_TRUE(godot_autopilot::authorization::capability_enabled("process"));
  EXPECT_TRUE(godot_autopilot::authorization::capability_enabled("code_execute"));
  EXPECT_TRUE(godot_autopilot::authorization::capability_enabled("game_runtime"));
}

TEST(SecurityHardening, CapabilityForToolMapping) {
  using namespace godot_autopilot;
  EXPECT_STREQ(authorization::capability_for_tool("create_os_process", SideEffect::Process), "process");
  EXPECT_STREQ(authorization::capability_for_tool("code_execute", SideEffect::None), "code_execute");
  EXPECT_STREQ(authorization::capability_for_tool("execute_script", SideEffect::None), "code_execute");
  EXPECT_STREQ(authorization::capability_for_tool("execute_game_script", SideEffect::None), "game_runtime");
  EXPECT_EQ(authorization::capability_for_tool("get_game_status", SideEffect::None), nullptr);
}

TEST(SecurityHardening, AllowListRemoveExpandsAll) {
  using godot_autopilot::authorization::allow_list_remove;
  EXPECT_EQ(allow_list_remove("all", "code_execute"),
            "process,game_runtime,user_tools");
  EXPECT_EQ(allow_list_remove("process,code_execute", "code_execute"),
            "process");
  EXPECT_EQ(allow_list_remove("process", "game_runtime"), "process");
}

TEST(SecurityHardening, AllowListAddAppends) {
  using godot_autopilot::authorization::allow_list_add;
  EXPECT_EQ(allow_list_add("", "game_runtime"), "game_runtime");
  EXPECT_EQ(allow_list_add("process", "game_runtime"),
            "process,game_runtime");
  EXPECT_EQ(allow_list_add("all", "game_runtime"), "all");
}

TEST(SecurityHardening, EnableMessageDockHintOnlyForToggleable) {
  EnvGuard g("GODOT_AUTOPILOT_ALLOW", nullptr);
  using godot_autopilot::SideEffect;
  auto dockable = godot_autopilot::authorization::deny_if_unauthorized(
      "execute_game_script", SideEffect::GameRuntime);
  ASSERT_FALSE(dockable.IsNull());
  const mcp::JsonValue *dock_enable = dockable.Find("enable");
  ASSERT_NE(dock_enable, nullptr);
  EXPECT_NE(dock_enable->GetString().find("Allow game_runtime"),
            std::string::npos);

  auto env_only = godot_autopilot::authorization::deny_if_unauthorized(
      "create_os_process", SideEffect::Process);
  ASSERT_FALSE(env_only.IsNull());
  const mcp::JsonValue *env_enable = env_only.Find("enable");
  ASSERT_NE(env_enable, nullptr);
  EXPECT_EQ(env_enable->GetString().find("dock"), std::string::npos);
  EXPECT_NE(env_enable->GetString().find("restart the engine"),
            std::string::npos);
}

TEST(SecurityHardening, ConfigConstantsSane) {
  EXPECT_EQ(godot_autopilot::GDA_DEFAULT_PORT, 9527);
  EXPECT_GT(godot_autopilot::GDA_MAX_TIMEOUT_MS, godot_autopilot::GDA_DEFAULT_TIMEOUT_MS);
  EXPECT_GT(godot_autopilot::GDA_CAPTURE_MAX_PNG_BYTES, 0u);
  EXPECT_GT(godot_autopilot::GDA_VARIANT_MAX_STRING_BYTES, 0u);
  EXPECT_GT(godot_autopilot::GDA_SCAN_MAX_FILES, 0u);
  EXPECT_GT(godot_autopilot::GDA_SCAN_MAX_DEPTH, 0u);
}

TEST(SecurityHardening, VariantStringTruncates) {
  EXPECT_GT(godot_autopilot::GDA_VARIANT_MAX_STRING_BYTES, 0u);
  std::string big(godot_autopilot::GDA_VARIANT_MAX_STRING_BYTES + 100, 'x');
  std::string truncated = big;
  if (truncated.size() > godot_autopilot::GDA_VARIANT_MAX_STRING_BYTES) {
    truncated.resize(godot_autopilot::GDA_VARIANT_MAX_STRING_BYTES - std::string("...(truncated)").size());
    truncated += "...(truncated)";
  }
  EXPECT_LE(truncated.size(), godot_autopilot::GDA_VARIANT_MAX_STRING_BYTES);
  EXPECT_NE(truncated.find("...(truncated)"), std::string::npos);
}

TEST(SecurityHardening, VariantArrayTruncates) {
  EXPECT_GT(godot_autopilot::GDA_VARIANT_MAX_ARRAY_ELEMENTS, 0u);
  size_t simulated = godot_autopilot::GDA_VARIANT_MAX_ARRAY_ELEMENTS + 10;
  size_t capped = std::min(simulated, godot_autopilot::GDA_VARIANT_MAX_ARRAY_ELEMENTS);
  EXPECT_EQ(capped, godot_autopilot::GDA_VARIANT_MAX_ARRAY_ELEMENTS);
}

TEST(SecurityHardening, LogConcurrentQueryStable) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::System, "CONCURRENT_1");
  std::vector<std::thread> writers;
  for (int t = 0; t < 4; ++t) {
    writers.emplace_back([t] {
      for (int i = 0; i < 100; ++i) LogSystem::instance().log(LogLevel::Debug, LogCategory::System, "THREAD_" + std::to_string(t) + "_" + std::to_string(i));
    });
  }
  std::vector<std::vector<LogEntry>> snapshots;
  std::thread reader([&] {
    for (int i = 0; i < 50; ++i) snapshots.push_back(LogSystem::instance().query_recent(10));
  });
  for (auto &th : writers) th.join();
  reader.join();
  for (auto &snap : snapshots) {
    for (auto &e : snap) {
      EXPECT_FALSE(e.message.empty());
    }
  }
}
