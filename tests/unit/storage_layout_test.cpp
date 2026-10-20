#include "core/storage_paths.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

using godot_autopilot::ProjectStore;
using godot_autopilot::StoragePaths;

namespace {

void set_store_env(const char *name, const char *value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

void unset_store_env(const char *name) {
#ifdef _WIN32
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

bool path_within(const std::string &path, const std::string &dir) {
  if (path.size() <= dir.size()) {
    return false;
  }
  if (path.compare(0, dir.size(), dir) != 0) {
    return false;
  }
  return path[dir.size()] == '/';
}

} // namespace

TEST(StorageLayoutTest, GlobalConfigPathLivesInCentralRoot) {
  set_store_env("GODOT_AUTOPILOT_HOME", "C:/gda/home");
  const std::string root = ProjectStore::root_dir();
  const std::string config = StoragePaths::global_config_path();
  EXPECT_EQ(root, "C:/gda/home/.godot-autopilot");
  EXPECT_EQ(config, root + "/config.json");
  EXPECT_TRUE(path_within(config, root));
  unset_store_env("GODOT_AUTOPILOT_HOME");
}

TEST(StorageLayoutTest, ProjectConfigPathLivesInProjectDirectory) {
  set_store_env("GODOT_AUTOPILOT_HOME", "C:/gda/home");
  const std::string project_dir = StoragePaths::project_dir("C:/projects/Demo");
  const std::string config =
      StoragePaths::project_config_path("C:/projects/Demo");
  EXPECT_EQ(config, project_dir + "/config.json");
  EXPECT_TRUE(path_within(config, project_dir));
  unset_store_env("GODOT_AUTOPILOT_HOME");
}

TEST(StorageLayoutTest, LogTraceCaptureDirsLiveInProjectDirectory) {
  set_store_env("GODOT_AUTOPILOT_HOME", "C:/gda/home");
  const std::string project_dir = StoragePaths::project_dir("C:/projects/Demo");
  const std::string log_dir = StoragePaths::log_dir("C:/projects/Demo");
  const std::string trace_dir = StoragePaths::trace_dir("C:/projects/Demo");
  const std::string captures_dir = StoragePaths::captures_dir("C:/projects/Demo");
  EXPECT_EQ(log_dir, project_dir + "/logs");
  EXPECT_EQ(trace_dir, project_dir + "/traces");
  EXPECT_EQ(captures_dir, project_dir + "/captures");
  EXPECT_TRUE(path_within(log_dir, project_dir));
  EXPECT_TRUE(path_within(trace_dir, project_dir));
  EXPECT_TRUE(path_within(captures_dir, project_dir));
  unset_store_env("GODOT_AUTOPILOT_HOME");
}

TEST(StorageLayoutTest, ProjectDirsStayUnderCentralRoot) {
  set_store_env("GODOT_AUTOPILOT_HOME", "C:/gda/home");
  const std::string root = ProjectStore::root_dir();
  const std::string project_dir = StoragePaths::project_dir("C:/projects/Demo");
  EXPECT_TRUE(path_within(project_dir, root));
  unset_store_env("GODOT_AUTOPILOT_HOME");
}

TEST(StorageLayoutTest, DifferentProjectPathsYieldDifferentProjectDirectories) {
  set_store_env("GODOT_AUTOPILOT_HOME", "C:/gda/home");
  EXPECT_NE(StoragePaths::project_dir("C:/projects/Demo"),
            StoragePaths::project_dir("D:/projects/Demo"));
  EXPECT_NE(StoragePaths::project_config_path("C:/projects/Demo"),
            StoragePaths::project_config_path("D:/projects/Demo"));
  unset_store_env("GODOT_AUTOPILOT_HOME");
}

TEST(StorageLayoutTest, TrailingSeparatorDoesNotChangeProjectDirectory) {
  set_store_env("GODOT_AUTOPILOT_HOME", "C:/gda/home");
  EXPECT_EQ(StoragePaths::project_dir("C:/projects/Demo"),
            StoragePaths::project_dir("C:/projects/Demo/"));
  EXPECT_EQ(StoragePaths::project_config_path("C:/projects/Demo"),
            StoragePaths::project_config_path("C:/projects/Demo/"));
  unset_store_env("GODOT_AUTOPILOT_HOME");
}

TEST(StorageLayoutTest, EmptyProjectPathYieldsNoStoragePaths) {
  set_store_env("GODOT_AUTOPILOT_HOME", "C:/gda/home");
  EXPECT_TRUE(StoragePaths::project_dir("").empty());
  EXPECT_TRUE(StoragePaths::project_config_path("").empty());
  EXPECT_TRUE(StoragePaths::log_dir("").empty());
  EXPECT_TRUE(StoragePaths::trace_dir("").empty());
  EXPECT_TRUE(StoragePaths::captures_dir("").empty());
  unset_store_env("GODOT_AUTOPILOT_HOME");
}
