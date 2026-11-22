#include "core/path_util.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

using godot_autopilot::atomic_temp_path;
using godot_autopilot::write_file_atomically;

TEST(AtomicWritePathTest, TempPathIncludesSerialAndIsUnique) {
  const std::string a = atomic_temp_path("state.json", 11);
  const std::string b = atomic_temp_path("state.json", 12);
  EXPECT_NE(a, b);
  EXPECT_NE(a.find(".tmp-"), std::string::npos);
  EXPECT_NE(a.find("11"), std::string::npos);
}

TEST(AtomicWritePathTest, AutoSerialTempPathsDiffer) {
  EXPECT_NE(atomic_temp_path("state.json", 0), atomic_temp_path("state.json", 0));
}

TEST(AtomicWritePathTest, WriteReplacesContentWithoutTempResidue) {
  std::error_code ec;
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path(ec) / "gda_atomic_path_test";
  if (ec) return;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  if (ec) return;
  const std::filesystem::path target = dir / "index.json";
  { std::ofstream(target, std::ios::binary) << "OLD"; }
  EXPECT_TRUE(write_file_atomically(target.string(), "NEW"));
  std::ifstream in(target, std::ios::binary);
  const std::string content((std::istreambuf_iterator<char>(in)), {});
  EXPECT_EQ(content, "NEW");
  for (const auto &entry : std::filesystem::directory_iterator(dir)) {
    EXPECT_EQ(entry.path().filename().string().find(".tmp-"), std::string::npos);
  }
  std::filesystem::remove_all(dir, ec);
}
