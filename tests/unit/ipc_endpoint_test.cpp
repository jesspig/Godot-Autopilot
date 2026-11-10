#include <gtest/gtest.h>

#include <string>

#include "core/storage/project_store.hpp"
#include "core/transport/ipc_endpoint.hpp"

namespace {

constexpr const char *kProjectPath = "C:/projects/sample_game";

} // namespace

TEST(IpcEndpointTest, DerivesRunDirSocketWithNameAndHash) {
  const std::string hash = godot_autopilot::ProjectStore::project_path_hash(kProjectPath);
  const std::string path = godot_autopilot::ipc_endpoint::derive_socket_path(
      "sample_game", kProjectPath, "C:/gda/run", "C:/local/gda");
  EXPECT_NE(path.find("sample_game-"), std::string::npos);
  EXPECT_NE(path.find(hash), std::string::npos);
  EXPECT_NE(path.find(".sock"), std::string::npos);
  EXPECT_NE(path.find("gda"), std::string::npos);
  EXPECT_LE(path.size(), godot_autopilot::ipc_endpoint::kMaxSocketPathChars);
}

TEST(IpcEndpointTest, LongRunDirFallsBackToHashOnlyPath) {
  const std::string hash = godot_autopilot::ProjectStore::project_path_hash(kProjectPath);
  const std::string long_run(120, 'x');
  const std::string path = godot_autopilot::ipc_endpoint::derive_socket_path(
      "sample_game", kProjectPath, long_run, "C:/local/gda");
  const std::string forward = "C:/local/gda/" + hash + ".sock";
  const std::string backward = "C:/local/gda\\" + hash + ".sock";
  EXPECT_TRUE(path == forward || path == backward);
  EXPECT_LE(path.size(), godot_autopilot::ipc_endpoint::kMaxSocketPathChars);
}

TEST(IpcEndpointTest, SanitizesProjectNameAndHandlesEmpty) {
  const std::string sanitized = godot_autopilot::ProjectStore::sanitize_project_name("my game");
  const std::string path = godot_autopilot::ipc_endpoint::derive_socket_path(
      "my game", kProjectPath, "C:/gda/run", "C:/local/gda");
  EXPECT_NE(path.find(sanitized), std::string::npos);
  const std::string fallback_name = godot_autopilot::ipc_endpoint::derive_socket_path(
      "", kProjectPath, "C:/gda/run", "C:/local/gda");
  EXPECT_NE(fallback_name.find("project-"), std::string::npos);
}

TEST(IpcEndpointTest, LeftoverRecreatedOnlyWhenUnreachable) {
  EXPECT_TRUE(godot_autopilot::ipc_endpoint::should_recreate_leftover(true, false));
  EXPECT_FALSE(godot_autopilot::ipc_endpoint::should_recreate_leftover(true, true));
  EXPECT_FALSE(godot_autopilot::ipc_endpoint::should_recreate_leftover(false, false));
  EXPECT_FALSE(godot_autopilot::ipc_endpoint::should_recreate_leftover(false, true));
}