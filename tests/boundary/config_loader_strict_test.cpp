#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "runner/config_loader.hpp"
#include "util/owned_exception.hpp"

namespace {

using godot_autopilot::RuntimeError;

gda_test::TestCase load_case_with_requires(const std::string &requires_json) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "gda_config_loader_test";
  fs::create_directories(dir);
  const fs::path file = dir / "requires_probe.json";
  {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << R"({"name":"t","requires":)" + requires_json + R"(,"pipeline":{}})";
  }
  struct FileGuard {
    const fs::path &path;
    ~FileGuard() { fs::remove(path); }
  } guard{file};
  return gda_test::load_test_case(file.string());
}

gda_test::TestCase load_case_text(const std::string &json) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "gda_config_loader_test";
  fs::create_directories(dir);
  const fs::path file = dir / "strict_probe.json";
  {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << json;
  }
  struct FileGuard {
    const fs::path &path;
    ~FileGuard() { fs::remove(path); }
  } guard{file};
  return gda_test::load_test_case(file.string());
}

} // namespace

TEST(ConfigLoaderStrict, RejectsUnknownTopLevelField) {
  EXPECT_THROW(load_case_text(R"({"name":"t","pipeline":{},"bogus":1})"), RuntimeError);
}

TEST(ConfigLoaderStrict, RejectsUnknownStepField) {
  EXPECT_THROW(load_case_text(
      R"({"name":"t","pipeline":{"stages":[{"steps":[{"tool":"ping","bogus":1}]}]}})"),
      RuntimeError);
}

TEST(ConfigLoaderStrict, RejectsEmptyExpect) {
  EXPECT_THROW(load_case_text(
      R"({"name":"t","pipeline":{"stages":[{"steps":[{"tool":"ping","expect":{}}]}]}})"),
      RuntimeError);
  EXPECT_THROW(load_case_text(
      R"({"name":"t","pipeline":{"stages":[{"steps":[{"tool":"ping","expect":{"allow_error":true}}]}]}})"),
      RuntimeError);
}

TEST(ConfigLoaderStrict, RequireTypeRejectsUnknownExpected) {
  const mcp::JsonValue value = mcp::JsonValue::Parse(R"({"a":1})");
  EXPECT_THROW(gda_test::RequireType(value, "probe", "integer"), RuntimeError);
  EXPECT_NO_THROW(gda_test::RequireType(value, "probe", "object"));
}

TEST(ConfigLoaderRequires, RequiresParsesValidList) {
  const gda_test::TestCase tc =
      load_case_with_requires(R"(["write","code_execute","all"])");
  ASSERT_EQ(tc.requires.size(), 3u);
  EXPECT_EQ(tc.requires[0], "write");
  EXPECT_EQ(tc.requires[1], "code_execute");
  EXPECT_EQ(tc.requires[2], "all");

  const gda_test::TestCase empty =
      load_case_with_requires(R"([])");
  EXPECT_TRUE(empty.requires.empty());
}

TEST(ConfigLoaderRequires, RejectsUnknownCapability) {
  EXPECT_THROW(load_case_with_requires(R"(["wriite"])"), RuntimeError);
}

TEST(ConfigLoaderRequires, RejectsDuplicateEntry) {
  EXPECT_THROW(load_case_with_requires(R"(["write","write"])"), RuntimeError);
}

TEST(ConfigLoaderRequires, RejectsNonStringEntry) {
  EXPECT_THROW(load_case_with_requires(R"(["write",7])"), RuntimeError);
}
