#include "core/project_store.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

using godot_autopilot::ProjectStore;

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

} // namespace

TEST(ProjectStoreTest, SameNameDifferentPathYieldsDifferentDirectory) {
  const std::string first = ProjectStore::project_dir_name("Demo", "C:/a/Demo");
  const std::string second =
      ProjectStore::project_dir_name("Demo", "D:/b/Demo");
  EXPECT_NE(first, second);
}

TEST(ProjectStoreTest, RenameKeepsDirectoryUnchanged) {
  const std::string before =
      ProjectStore::project_dir_name("OldName", "C:/projects/Demo");
  const std::string after =
      ProjectStore::project_dir_name("NewName", "C:/projects/Demo");
  EXPECT_EQ(before, after);
}

TEST(ProjectStoreTest, DirectoryNameUsesPathBasename) {
  const std::string dir =
      ProjectStore::project_dir_name("Ignored", "C:/projects/Demo");
  EXPECT_NE(dir.find("Demo"), std::string::npos);
  EXPECT_EQ(dir.find("Ignored"), std::string::npos);
}

TEST(ProjectStoreTest, ReplacesIllegalCharactersInDirectoryName) {
  const std::string dir =
      ProjectStore::project_dir_name("ignored", "C:/projects/My:Game*?");
  EXPECT_EQ(dir.find(':'), std::string::npos);
  EXPECT_EQ(dir.find('*'), std::string::npos);
  EXPECT_EQ(dir.find('?'), std::string::npos);
  EXPECT_NE(dir.find("My_Game"), std::string::npos);
}

TEST(ProjectStoreTest, TruncatesLongProjectName) {
  const std::string long_name(80, 'x');
  EXPECT_EQ(ProjectStore::sanitize_project_name(long_name).size(), 40u);
}

TEST(ProjectStoreTest, PathHashIsStableAcrossSeparatorStyles) {
  EXPECT_EQ(ProjectStore::project_path_hash("C:\\projects\\Demo"),
            ProjectStore::project_path_hash("C:/projects/Demo"));
  EXPECT_NE(ProjectStore::project_path_hash("C:/projects/Demo"),
            ProjectStore::project_path_hash("C:/projects/Other"));
  EXPECT_EQ(ProjectStore::project_path_hash("C:/projects/Demo").size(), 8u);
}

TEST(ProjectStoreTest, DirectoryNameEndsWithPathHash) {
  const std::string path = "C:/projects/Demo";
  const std::string hash = ProjectStore::project_path_hash(path);
  const std::string dir = ProjectStore::project_dir_name("Demo", path);
  ASSERT_GT(dir.size(), hash.size());
  EXPECT_EQ(dir.substr(dir.size() - hash.size() - 1), "-" + hash);
}

TEST(ProjectStoreTest, RootLayoutFollowsHomeOverride) {
  set_store_env("GODOT_AUTOPILOT_HOME", "C:/gda/home");
  const std::string root = "C:/gda/home/.godot-autopilot";
  const std::string project =
      root + "/projects/" +
      ProjectStore::project_dir_name("", "C:/projects/Demo");

  EXPECT_EQ(ProjectStore::root_dir(), root);
  EXPECT_EQ(ProjectStore::global_config_path(), root + "/config.json");
  EXPECT_EQ(ProjectStore::instances_index_path(), root + "/instances.json");
  EXPECT_EQ(ProjectStore::project_dir("Demo", "C:/projects/Demo"), project);
  EXPECT_EQ(ProjectStore::instance_dir("C:/projects/Demo"),
            project + "/instances");
  EXPECT_EQ(ProjectStore::project_config_path("C:/projects/Demo"),
            project + "/config.json");
  unset_store_env("GODOT_AUTOPILOT_HOME");
}
