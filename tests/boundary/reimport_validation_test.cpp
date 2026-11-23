#include "tools/domains/resource/reimport_validation.hpp"

#include <gtest/gtest.h>

using godot_autopilot::resource_ops_detail::validate_reimport_counts;
using godot_autopilot::resource_ops_detail::ReimportCounts;

TEST(ReimportValidationTest, FilesMustBeNonEmptyStringArray) {
  ReimportCounts c;
  c.has_files = true;
  c.files_is_array = true;
  c.string_count = 0;
  EXPECT_FALSE(validate_reimport_counts(c, 256));
  c.string_count = 2;
  EXPECT_TRUE(validate_reimport_counts(c, 256));
}

TEST(ReimportValidationTest, FilesNonArrayRejected) {
  ReimportCounts c;
  c.has_files = true;
  c.files_is_array = false;
  c.string_count = 1;
  EXPECT_FALSE(validate_reimport_counts(c, 256));
}

TEST(ReimportValidationTest, FilesOverLimitRejected) {
  ReimportCounts c;
  c.has_files = true;
  c.files_is_array = true;
  c.string_count = 300;
  EXPECT_FALSE(validate_reimport_counts(c, 256));
}

TEST(ReimportValidationTest, PathFallbackRequiresString) {
  ReimportCounts c;
  c.has_files = false;
  c.path_is_string = false;
  EXPECT_FALSE(validate_reimport_counts(c, 256));
  c.path_is_string = true;
  EXPECT_TRUE(validate_reimport_counts(c, 256));
}
