#include "util/project_path.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::util::containment_status_allows;
using godot_autopilot::util::normalize_project_path;
using godot_autopilot::util::ProjectPathResolveStatus;
using godot_autopilot::util::set_scheme_globalize_provider;

TEST(SchemePathContainmentTest, OnlyInsideIsAllowed) {
  EXPECT_TRUE(containment_status_allows(ProjectPathResolveStatus::Inside));
  EXPECT_FALSE(containment_status_allows(ProjectPathResolveStatus::Outside));
  EXPECT_FALSE(containment_status_allows(ProjectPathResolveStatus::Unresolved));
}

TEST(SchemePathContainmentTest, NoProviderKeepsLegacyLexicalBehavior) {
  set_scheme_globalize_provider({});
  const auto result = normalize_project_path("res://themes/ui.tres", false, false);
  EXPECT_TRUE(result.error.empty());
}

TEST(SchemePathContainmentTest, LexicalEscapeStillRejected) {
  set_scheme_globalize_provider({});
  const auto result = normalize_project_path("res://tests/../../outside", false, false);
  EXPECT_FALSE(result.error.empty());
}
