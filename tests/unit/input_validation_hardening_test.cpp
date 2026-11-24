#include "util/bm25_index.hpp"
#include "util/input_guards.hpp"
#include "util/project_path.hpp"

#include <gtest/gtest.h>

#include "support/project_path_provider.hpp"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <string>
#include <system_error>
#include <vector>

using godot_autopilot::util::ProjectPathResolveStatus;

TEST(InputValidationHardening, PhysicsFpsRange) {
  using godot_autopilot::util::kMaxPhysicsFps;
  using godot_autopilot::util::kMinPhysicsFps;
  using godot_autopilot::util::physics_fps_is_valid;
  EXPECT_FALSE(physics_fps_is_valid(kMinPhysicsFps - 1));
  EXPECT_TRUE(physics_fps_is_valid(kMinPhysicsFps));
  EXPECT_TRUE(physics_fps_is_valid(60));
  EXPECT_TRUE(physics_fps_is_valid(kMaxPhysicsFps));
  EXPECT_FALSE(physics_fps_is_valid(kMaxPhysicsFps + 1));
  EXPECT_FALSE(physics_fps_is_valid(INT64_MIN));
  EXPECT_FALSE(physics_fps_is_valid(INT64_MAX));
}

TEST(InputValidationHardening, MaxFpsRange) {
  using godot_autopilot::util::kMaxMaxFps;
  using godot_autopilot::util::kMinMaxFps;
  using godot_autopilot::util::max_fps_is_valid;
  EXPECT_FALSE(max_fps_is_valid(kMinMaxFps - 1));
  EXPECT_TRUE(max_fps_is_valid(kMinMaxFps));
  EXPECT_TRUE(max_fps_is_valid(60));
  EXPECT_TRUE(max_fps_is_valid(kMaxMaxFps));
  EXPECT_FALSE(max_fps_is_valid(kMaxMaxFps + 1));
}

TEST(InputValidationHardening, TimeScaleRange) {
  using godot_autopilot::util::kMaxTimeScale;
  using godot_autopilot::util::time_scale_is_valid;
  EXPECT_TRUE(time_scale_is_valid(0.0));
  EXPECT_TRUE(time_scale_is_valid(1.0));
  EXPECT_TRUE(time_scale_is_valid(kMaxTimeScale));
  EXPECT_FALSE(time_scale_is_valid(-0.1));
  EXPECT_FALSE(time_scale_is_valid(kMaxTimeScale + 0.01));
  EXPECT_FALSE(time_scale_is_valid(std::numeric_limits<double>::infinity()));
  EXPECT_FALSE(time_scale_is_valid(std::numeric_limits<double>::quiet_NaN()));
}

TEST(InputValidationHardening, GameBridgeMalformedJsonDoesNotThrow) {
  mcp::JsonValue parsed;
  EXPECT_TRUE(godot_autopilot::util::try_parse_json(R"({"op":"ping"})", parsed));
  EXPECT_TRUE(parsed.IsObject());

  EXPECT_FALSE(godot_autopilot::util::try_parse_json("not-json", parsed));
  EXPECT_FALSE(godot_autopilot::util::try_parse_json("{\"op\":", parsed));
  EXPECT_FALSE(godot_autopilot::util::try_parse_json("", parsed));
}

TEST(InputValidationHardening, DecimalObjectIdParsingRejectsOverflow) {
  using godot_autopilot::util::parse_decimal_int64;
  int64_t out = 0;
  EXPECT_TRUE(parse_decimal_int64("0", out));
  EXPECT_EQ(out, 0);
  EXPECT_TRUE(parse_decimal_int64("123456789", out));
  EXPECT_EQ(out, 123456789);
  EXPECT_TRUE(parse_decimal_int64("9223372036854775807", out));
  EXPECT_EQ(out, INT64_MAX);
  EXPECT_FALSE(parse_decimal_int64("", out));
  EXPECT_FALSE(parse_decimal_int64("12a", out));
  EXPECT_FALSE(parse_decimal_int64("-1", out));
  EXPECT_FALSE(parse_decimal_int64("99999999999999999999999999", out));
  EXPECT_EQ(out, INT64_MAX);
}

TEST(InputValidationHardening, AsciiLowercaseSafeForHighBytes) {
  const std::string input("A\xC3\xB6\xFF"
                          "Z");
  const std::string expected("a\xC3\xB6\xFF"
                             "z");
  EXPECT_EQ(godot_autopilot::util::ascii_lowercase(input), expected);
  EXPECT_EQ(godot_autopilot::util::ascii_tolower('\xFF'),
            static_cast<char>(std::tolower(static_cast<unsigned char>('\xFF'))));
}

TEST(InputValidationHardening, BbcodeEscaping) {
  using godot_autopilot::util::escape_bbcode;
  EXPECT_EQ(escape_bbcode("plain text"), "plain text");
  EXPECT_EQ(escape_bbcode("[b]bold[/b]"), "[lb]b[rb]bold[lb]/b[rb]");
  EXPECT_EQ(escape_bbcode("err [url=http://x]y[/url]"),
            "err [lb]url=http://x[rb]y[lb]/url[rb]");
}

TEST(InputValidationHardening, ResourceTemplateMissingVariable) {
  using godot_autopilot::util::resource_template_value;
  const std::map<std::string, std::string> vars{{"path", "res://a"}};
  std::string out = "sentinel";
  EXPECT_TRUE(resource_template_value(vars, "path", out));
  EXPECT_EQ(out, "res://a");
  out = "sentinel";
  EXPECT_FALSE(resource_template_value(vars, "key", out));
  EXPECT_EQ(out, "sentinel");
}

TEST(InputValidationHardening, ResourceUriPathNormalization) {
  using godot_autopilot::util::normalize_resource_uri_path;
  std::string out;
  EXPECT_TRUE(normalize_resource_uri_path("a/b/c.gd", out));
  EXPECT_EQ(out, "res://a/b/c.gd");
  EXPECT_TRUE(normalize_resource_uri_path("res://a/b/c.gd", out));
  EXPECT_EQ(out, "res://a/b/c.gd");
  EXPECT_TRUE(normalize_resource_uri_path("a\\b\\c.gd", out));
  EXPECT_EQ(out, "res://a/b/c.gd");
  EXPECT_TRUE(normalize_resource_uri_path("a/./b/../c.gd", out));
  EXPECT_EQ(out, "res://a/c.gd");
  EXPECT_FALSE(normalize_resource_uri_path("../outside.gd", out));
  EXPECT_FALSE(normalize_resource_uri_path("a/../../outside.gd", out));
  EXPECT_FALSE(normalize_resource_uri_path("", out));
  EXPECT_FALSE(normalize_resource_uri_path("res://", out));
}

TEST(InputValidationHardening, TestDirPathUsesProjectNormalizer) {
  godot_autopilot_test::ProjectPathProviderGuard provider;
  const auto traversal =
      godot_autopilot::util::normalize_project_path("res://tests/../../outside", false, true);
  EXPECT_FALSE(traversal.valid());
  const auto ok =
      godot_autopilot::util::normalize_project_path("res://tests", false, true);
  EXPECT_TRUE(ok.valid());
  EXPECT_EQ(ok.value, "res://tests");
}

TEST(InputValidationHardening, ThemeReadPathUsesProjectNormalizer) {
  godot_autopilot_test::ProjectPathProviderGuard provider;
  const auto traversal = godot_autopilot::util::normalize_project_path(
      "res://themes/../../outside.tres", true, false);
  EXPECT_FALSE(traversal.valid());
  const auto ok = godot_autopilot::util::normalize_project_path(
      "res://themes/ui.tres", true, false);
  EXPECT_TRUE(ok.valid());
  EXPECT_EQ(ok.value, "res://themes/ui.tres");
}

TEST(InputValidationHardening, ProjectPathResolvesSymlinkContainment) {
  std::error_code ec;
  const std::filesystem::path base =
      std::filesystem::temp_directory_path(ec) / "gda_path_guard_test";
  if (ec) {
    return;
  }
  const std::filesystem::path root = base / "root";
  const std::filesystem::path outside = base / "outside";
  std::filesystem::remove_all(base, ec);
  std::filesystem::create_directories(root, ec);
  if (ec) {
    return;
  }
  std::filesystem::create_directories(outside, ec);
  if (ec) {
    std::filesystem::remove_all(base, ec);
    return;
  }
  const std::filesystem::path link = root / "link_to_outside";
  std::filesystem::create_directory_symlink(outside, link, ec);
  if (ec) {
    std::filesystem::remove_all(base, ec);
    GTEST_SKIP() << "symlink creation not permitted on this platform";
  }

  const std::string root_str = root.generic_string();
  EXPECT_EQ(godot_autopilot::util::project_path_resolve_status(
                root.generic_string(), root_str),
            ProjectPathResolveStatus::Inside);
  EXPECT_EQ(godot_autopilot::util::project_path_resolve_status(
                outside.generic_string(), root_str),
            ProjectPathResolveStatus::Outside);
  EXPECT_EQ(godot_autopilot::util::project_path_resolve_status(
                link.generic_string(), root_str),
            ProjectPathResolveStatus::Outside);

  std::filesystem::remove_all(base, ec);
}

TEST(InputValidationHardening, Bm25EmptyTextHonorsTags) {
  godot_autopilot::Bm25Index idx;
  idx.add_entry("alpha", "desc", "CatA", {"tag-a"});
  idx.add_entry("beta", "desc", "CatB", {"tag-b"});

  godot_autopilot::Bm25Index::SearchQuery q;
  q.text = "";
  q.tags = std::vector<std::string>{"tag-b"};
  const auto results = idx.search(q);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].name, "beta");
}

TEST(InputValidationHardening, Bm25EmptyTextHonorsCategoryAndTags) {
  godot_autopilot::Bm25Index idx;
  idx.add_entry("alpha", "desc", "CatA", {"shared"});
  idx.add_entry("beta", "desc", "CatB", {"shared"});

  godot_autopilot::Bm25Index::SearchQuery q;
  q.text = "";
  q.category = "CatB";
  q.tags = std::vector<std::string>{"shared"};
  const auto results = idx.search(q);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].name, "beta");
}

TEST(InputValidationHardening, Bm25CandidateAverageLength) {
  EXPECT_DOUBLE_EQ(godot_autopilot::bm25_average_document_length({}), 1.0);
  EXPECT_DOUBLE_EQ(godot_autopilot::bm25_average_document_length({0, 0}), 1.0);
  EXPECT_DOUBLE_EQ(godot_autopilot::bm25_average_document_length({2, 4}), 3.0);
}
