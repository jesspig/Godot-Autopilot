#include "tools/editor_ui_ops.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using godot_autopilot::editor_ui_ops::clamp_scene_tree_max_items;
using godot_autopilot::editor_ui_ops::lower_ascii;
using godot_autopilot::editor_ui_ops::scene_tree_relative_path;
using godot_autopilot::editor_ui_ops::scene_tree_row_matches;

} // namespace

TEST(EditorUiTreeTest, LowerAsciiFoldsOnlyAsciiLetters) {
  EXPECT_EQ(lower_ascii("SceneTreeItem"), "scenetreeitem");
  EXPECT_EQ(lower_ascii("ABC-123_x"), "abc-123_x");
  EXPECT_EQ(lower_ascii(""), "");
  EXPECT_EQ(lower_ascii("\xC3\x84"), "\xC3\x84");
}

TEST(EditorUiTreeTest, ClampSceneTreeMaxItemsBoundsToOneThousand) {
  EXPECT_EQ(clamp_scene_tree_max_items(-5), 1);
  EXPECT_EQ(clamp_scene_tree_max_items(0), 1);
  EXPECT_EQ(clamp_scene_tree_max_items(1), 1);
  EXPECT_EQ(clamp_scene_tree_max_items(200), 200);
  EXPECT_EQ(clamp_scene_tree_max_items(1000), 1000);
  EXPECT_EQ(clamp_scene_tree_max_items(1001), 1000);
  EXPECT_EQ(clamp_scene_tree_max_items(100000), 1000);
}

TEST(EditorUiTreeTest, RowMatchesEmptyFilterKeepsEveryRow) {
  EXPECT_TRUE(scene_tree_row_matches("Alpha/Beta", "Beta", "", false, false));
  EXPECT_TRUE(scene_tree_row_matches("", "", "", false, false));
  EXPECT_TRUE(scene_tree_row_matches("Alpha/Beta", "Beta", "", true, false));
}

TEST(EditorUiTreeTest, RowMatchesFilterIsCaseInsensitiveAcrossPathAndName) {
  EXPECT_TRUE(scene_tree_row_matches("Alpha/Beta", "Beta", "alpha", false, false));
  EXPECT_TRUE(scene_tree_row_matches("Alpha/Beta", "Beta", "BETA", false, false));
  EXPECT_TRUE(scene_tree_row_matches("Alpha/Beta", "Beta", "ha/be", false, false));
  EXPECT_FALSE(scene_tree_row_matches("Alpha/Beta", "Beta", "gamma", false, false));
}

TEST(EditorUiTreeTest, RowMatchesSelectedOnlyRequiresSelection) {
  EXPECT_FALSE(scene_tree_row_matches("Gamma", "Gamma", "", false, true));
  EXPECT_TRUE(scene_tree_row_matches("Gamma", "Gamma", "", true, true));
  EXPECT_FALSE(scene_tree_row_matches("Gamma", "Gamma", "gam", false, true));
  EXPECT_TRUE(scene_tree_row_matches("Gamma", "Gamma", "gam", true, true));
}

TEST(EditorUiTreeTest, RelativePathJoinsSegments) {
  EXPECT_EQ(scene_tree_relative_path("", "Root"), "Root");
  EXPECT_EQ(scene_tree_relative_path("Alpha", "Beta"), "Alpha/Beta");
  EXPECT_EQ(scene_tree_relative_path("Alpha/Beta", "Gamma"), "Alpha/Beta/Gamma");
  EXPECT_EQ(scene_tree_relative_path("Alpha", ""), "Alpha");
  EXPECT_EQ(scene_tree_relative_path("", ""), "");
}
