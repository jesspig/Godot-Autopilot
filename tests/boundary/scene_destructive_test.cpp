#include "tools/domains/editor/editor_safety.hpp"
#include "tools/domains/editor/editor_tools.hpp"
#include "tools/domains/scene/scene_safety.hpp"
#include "tools/domains/scene/scene_tools.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

using godot_autopilot::editor_safety::check_reload_discard;
using godot_autopilot::editor_safety::check_save_as_target;
using godot_autopilot::editor_safety::ReloadDiscardFailure;
using godot_autopilot::editor_safety::SaveAsTargetFailure;
using godot_autopilot::scene_safety::check_replace_confirm;
using godot_autopilot::scene_safety::is_scene_file;
using godot_autopilot::scene_safety::PackRecoveryAction;
using godot_autopilot::scene_safety::pack_recovery_action;
using godot_autopilot::scene_safety::ReplaceConfirmFailure;

TEST(SceneDestructiveTest, PackRecoveryDecisions) {
  EXPECT_EQ(pack_recovery_action(true, true, true),
            PackRecoveryAction::kSucceeded);
  EXPECT_EQ(pack_recovery_action(false, true, true),
            PackRecoveryAction::kSucceeded);
  EXPECT_EQ(pack_recovery_action(true, false, true),
            PackRecoveryAction::kAbortBeforeCommit);
  EXPECT_EQ(pack_recovery_action(false, false, true),
            PackRecoveryAction::kAbortBeforeCommit);
  EXPECT_EQ(pack_recovery_action(true, true, false),
            PackRecoveryAction::kRestoreBackup);
  EXPECT_EQ(pack_recovery_action(false, true, false),
            PackRecoveryAction::kDiscardStaging);
}

TEST(SceneDestructiveTest, SceneExtensionGate) {
  EXPECT_TRUE(is_scene_file("res://scenes/level.tscn"));
  EXPECT_TRUE(is_scene_file("res://scenes/level.scn"));
  EXPECT_FALSE(is_scene_file("res://scenes/level.tres"));
  EXPECT_FALSE(is_scene_file("res://scenes/level"));
  EXPECT_FALSE(is_scene_file("res://scenes/level.tscn.bak"));
}

TEST(SceneDestructiveTest, ReplaceRequiresConfirmation) {
  EXPECT_EQ(check_replace_confirm(false),
            ReplaceConfirmFailure::kConfirmationRequired);
  EXPECT_EQ(check_replace_confirm(true), ReplaceConfirmFailure::kNone);
}

TEST(SceneDestructiveTest, SaveAsTargetGate) {
  EXPECT_EQ(check_save_as_target(false, true, false, false, false),
            SaveAsTargetFailure::kInvalidPath);
  EXPECT_EQ(check_save_as_target(true, false, false, false, false),
            SaveAsTargetFailure::kInvalidExtension);
  EXPECT_EQ(check_save_as_target(true, true, false, false, false),
            SaveAsTargetFailure::kNone);
  EXPECT_EQ(check_save_as_target(true, true, true, false, false),
            SaveAsTargetFailure::kTargetExists);
  EXPECT_EQ(check_save_as_target(true, true, true, true, false),
            SaveAsTargetFailure::kConfirmationRequired);
  EXPECT_EQ(check_save_as_target(true, true, true, true, true),
            SaveAsTargetFailure::kNone);
}

TEST(SceneDestructiveTest, ReloadDiscardGate) {
  EXPECT_EQ(check_reload_discard(false, false, false), ReloadDiscardFailure::kNone);
  EXPECT_EQ(check_reload_discard(true, false, false),
            ReloadDiscardFailure::kConfirmationRequired);
  EXPECT_EQ(check_reload_discard(true, true, false),
            ReloadDiscardFailure::kConfirmationRequired);
  EXPECT_EQ(check_reload_discard(true, false, true),
            ReloadDiscardFailure::kConfirmationRequired);
  EXPECT_EQ(check_reload_discard(true, true, true), ReloadDiscardFailure::kNone);
}

static mcp::JsonValue find_tool_schema(
    const std::vector<std::unique_ptr<godot_autopilot::ToolBase>> &tools,
    const std::string &name) {
  for (const auto &tool : tools) {
    if (tool->meta().name == name) {
      return tool->input_schema();
    }
  }
  return mcp::JsonValue(mcp::JsonValue::object_tag);
}

static bool schema_has_property(const mcp::JsonValue &schema,
                                const std::string &name) {
  const mcp::JsonValue *properties = schema.Find("properties");
  return properties != nullptr && properties->IsObject() &&
         properties->Find(name) != nullptr;
}

TEST(SceneDestructiveTest, SchemasExposeDestructiveGuards) {
  const auto scene_tools = godot_autopilot::scene_tools::make_tools();
  const mcp::JsonValue edit = find_tool_schema(scene_tools, "edit_scene_node");
  EXPECT_TRUE(schema_has_property(edit, "confirm"));
  const mcp::JsonValue pack = find_tool_schema(scene_tools, "pack_scene");
  EXPECT_TRUE(schema_has_property(pack, "overwrite"));

  const auto editor_tools = godot_autopilot::editor_tools::make_tools();
  const mcp::JsonValue scene_control =
      find_tool_schema(editor_tools, "editor_scene_control");
  EXPECT_TRUE(schema_has_property(scene_control, "discard_unsaved"));
  EXPECT_TRUE(schema_has_property(scene_control, "confirm"));
  const mcp::JsonValue save = find_tool_schema(editor_tools, "save_editor_scene");
  EXPECT_TRUE(schema_has_property(save, "overwrite"));
  EXPECT_TRUE(schema_has_property(save, "confirm"));
}
