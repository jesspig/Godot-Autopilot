#include "core/gda_home.hpp"
#include "core/path_util.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

using godot_autopilot::HomeResolver;

namespace {

bool probe_accept_all(const std::string &path) { return !path.empty(); }

HomeResolver::DirectoryProbe probe_rejecting(const std::string &rejected) {
  return [rejected](const std::string &path) { return path != rejected; };
}

void set_home_env(const char *name, const char *value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

void unset_home_env(const char *name) {
#ifdef _WIN32
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

} // namespace

TEST(HomeResolverTest, WindowsResolvesHomeEnvironmentFirst) {
  const std::string resolved = HomeResolver::resolve(
      "C:/Users/dev", "C:/Users/other", "C:/Users/other/AppData/Roaming", "",
      HomeResolver::HostPlatform::kWindows, probe_accept_all);
  EXPECT_EQ(resolved, "C:/Users/dev");
}

TEST(HomeResolverTest, PosixResolvesHomeEnvironmentFirst) {
  const std::string resolved =
      HomeResolver::resolve("/home/dev", "", "", "",
                            HomeResolver::HostPlatform::kPosix,
                            probe_accept_all);
  EXPECT_EQ(resolved, "/home/dev");
}

TEST(HomeResolverTest, PosixIgnoresUserProfileFallback) {
  const std::string resolved =
      HomeResolver::resolve("", "/home/ignored", "", "",
                            HomeResolver::HostPlatform::kPosix,
                            probe_accept_all);
  EXPECT_TRUE(resolved.empty());
}

TEST(HomeResolverTest, WindowsFallsBackToUserProfile) {
  const std::string resolved = HomeResolver::resolve(
      "", "C:/Users/dev", "C:/Users/dev/AppData/Roaming", "",
      HomeResolver::HostPlatform::kWindows, probe_accept_all);
  EXPECT_EQ(resolved, "C:/Users/dev");
}

TEST(HomeResolverTest, WindowsDerivesHomeFromAppData) {
  const std::string resolved = HomeResolver::resolve(
      "", "", "C:/Users/dev/AppData/Roaming", "",
      HomeResolver::HostPlatform::kWindows, probe_accept_all);
  EXPECT_EQ(resolved, "C:/Users/dev");
}

TEST(HomeResolverTest, PosixDerivesHomeFromDataDir) {
  const std::string resolved =
      HomeResolver::resolve("", "", "", "/home/dev/.local/share",
                            HomeResolver::HostPlatform::kPosix,
                            probe_accept_all);
  EXPECT_EQ(resolved, "/home/dev");
}

TEST(HomeResolverTest, RejectedCandidateDegradesToNextLevel) {
  const std::string resolved = HomeResolver::resolve(
      "", "C:/Users/dev", "C:/Users/other/AppData/Roaming", "",
      HomeResolver::HostPlatform::kWindows, probe_rejecting("C:/Users/dev"));
  EXPECT_EQ(resolved, "C:/Users/other");
}

TEST(HomeResolverTest, RejectedDerivedCandidateYieldsEmptyResult) {
  const std::string resolved = HomeResolver::resolve(
      "", "", "C:/Users/dev/AppData/Roaming", "",
      HomeResolver::HostPlatform::kWindows, probe_rejecting("C:/Users/dev"));
  EXPECT_TRUE(resolved.empty());
}

TEST(HomeResolverTest, InvalidDerivedCandidateDegradesToFailure) {
  const std::string resolved = HomeResolver::resolve(
      "", "", "C:/Users/dev", "", HomeResolver::HostPlatform::kWindows,
      probe_accept_all);
  EXPECT_TRUE(resolved.empty());
}

TEST(HomeResolverTest, ValidateShapeRejectsFilesystemRoot) {
  EXPECT_FALSE(HomeResolver::validate_shape("/"));
  EXPECT_FALSE(HomeResolver::validate_shape("C:/"));
  EXPECT_FALSE(HomeResolver::validate_shape("C:"));
  EXPECT_FALSE(HomeResolver::validate_shape(""));
  EXPECT_TRUE(HomeResolver::validate_shape("C:/Users/dev"));
  EXPECT_TRUE(HomeResolver::validate_shape("/home/dev"));
  EXPECT_TRUE(HomeResolver::validate_shape("C:/Users/dev/"));
}

TEST(HomeResolverTest, ValidateRequiresExistingDirectory) {
  EXPECT_FALSE(HomeResolver::validate("C:/gda/definitely/missing/directory"));
  const std::string current = std::filesystem::current_path().string();
  EXPECT_TRUE(HomeResolver::validate(current));
  EXPECT_FALSE(HomeResolver::validate(""));
}

TEST(HomeResolverTest, ParentDirWalksUpRequestedLevels) {
  EXPECT_EQ(godot_autopilot::parent_dir("C:/Users/dev/AppData/Roaming", 2),
            "C:/Users/dev");
  EXPECT_EQ(godot_autopilot::parent_dir("/home/dev/.local/share", 2),
            "/home/dev");
  EXPECT_EQ(godot_autopilot::parent_dir("/home/dev/.local/share", 1),
            "/home/dev/.local");
  EXPECT_EQ(godot_autopilot::parent_dir("relative/path", 1), "relative");
  EXPECT_TRUE(godot_autopilot::parent_dir("relative", 1).empty());
  EXPECT_EQ(godot_autopilot::parent_dir("/home/dev", 1), "/home");
  EXPECT_EQ(godot_autopilot::parent_dir("/home", 1), "/");
}

TEST(HomeResolverTest, EnvironmentOverrideWins) {
  set_home_env("GODOT_AUTOPILOT_HOME", "C:/gda/home-override");
  EXPECT_EQ(HomeResolver::gda_home(), "C:/gda/home-override");
  unset_home_env("GODOT_AUTOPILOT_HOME");
}
