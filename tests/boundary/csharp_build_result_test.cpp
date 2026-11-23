#include "tools/domains/editor/csharp_build_result.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using godot_autopilot::editor_ops_detail::scan_error_lines;
using godot_autopilot::editor_ops_detail::truncate_digest;

TEST(CSharpBuildResultTest, DigestUnderLimitIsUnchanged) {
  auto d = truncate_digest("short output", 100);
  EXPECT_EQ(d.text, "short output");
  EXPECT_FALSE(d.truncated);
}

TEST(CSharpBuildResultTest, DigestOverLimitIsMarkedTruncated) {
  auto d = truncate_digest(std::string(200, 'x'), 50);
  EXPECT_TRUE(d.truncated);
  EXPECT_NE(d.text.find("truncated"), std::string::npos);
}

TEST(CSharpBuildResultTest, ErrorScanFindsCaseInsensitiveErrors) {
  std::vector<std::string> lines = {"all good", "ERROR: boom", "Error: again", "warning"};
  auto scan = scan_error_lines(lines, 10);
  EXPECT_EQ(scan.errors.size(), 2u);
  EXPECT_EQ(scan.errors[0], "ERROR: boom");
  EXPECT_FALSE(scan.truncated);
}

TEST(CSharpBuildResultTest, ErrorScanMarksTruncation) {
  std::vector<std::string> lines = {"error 1", "error 2", "error 3"};
  auto scan = scan_error_lines(lines, 2);
  EXPECT_EQ(scan.errors.size(), 2u);
  EXPECT_TRUE(scan.truncated);
}
