#include "core/editor_readiness.hpp"

#include <gtest/gtest.h>
#include <string>

using godot_autopilot::editor_startup_elapsed_ms;
using godot_autopilot::is_startup_window;
using godot_autopilot::kEditorStartupWindowMs;
using godot_autopilot::note_editor_startup;
using godot_autopilot::startup_gated_error;
using godot_autopilot::startup_in_progress_error;

namespace {

const char *kPlayNormalError =
    "failed to start scene playback: no game process started — ensure the "
    "current scene is saved and runnable";

const char *kCreateNormalError =
    "parent node not found: Main — 当前没有可编辑的场景根节点";

godot_autopilot::util::JsonValue normal_error(const std::string &text) {
  godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
  e["error"] = godot_autopilot::util::JsonValue(text);
  return e;
}

std::string error_text(const godot_autopilot::util::JsonValue &value) {
  const godot_autopilot::util::JsonValue *field = value.Find("error");
  return field && field->IsString() ? std::string(field->GetString())
                                    : std::string();
}

bool contains(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

} // namespace

TEST(EditorStartupGateTest, NoRootWithinWindowIsStartup) {
  EXPECT_TRUE(is_startup_window(false, 0));
  EXPECT_TRUE(is_startup_window(false, 1));
  EXPECT_TRUE(is_startup_window(false, 1000));
  EXPECT_TRUE(is_startup_window(false, kEditorStartupWindowMs - 1));
}

TEST(EditorStartupGateTest, NoRootAtOrBeyondWindowIsNotStartup) {
  EXPECT_FALSE(is_startup_window(false, kEditorStartupWindowMs));
  EXPECT_FALSE(is_startup_window(false, kEditorStartupWindowMs + 1));
  EXPECT_FALSE(is_startup_window(false, 60000));
}

TEST(EditorStartupGateTest, UnknownStartupElapsedIsNotStartup) {
  EXPECT_FALSE(is_startup_window(false, -1));
  EXPECT_FALSE(is_startup_window(false, -1000));
}

TEST(EditorStartupGateTest, EditedRootPresentNeverStartup) {
  EXPECT_FALSE(is_startup_window(true, 0));
  EXPECT_FALSE(is_startup_window(true, 1000));
  EXPECT_FALSE(is_startup_window(true, kEditorStartupWindowMs - 1));
  EXPECT_FALSE(is_startup_window(true, kEditorStartupWindowMs));
}

TEST(EditorStartupGateTest, StartupMarkProducesNonNegativeElapsed) {
  note_editor_startup();
  const int64_t elapsed = editor_startup_elapsed_ms();
  EXPECT_GE(elapsed, 0);
  EXPECT_LT(elapsed, kEditorStartupWindowMs);
}

TEST(EditorStartupGateTest, StartupErrorCarriesIf02Contract) {
  const godot_autopilot::util::JsonValue e = startup_in_progress_error();
  ASSERT_TRUE(e.IsObject());
  const godot_autopilot::util::JsonValue *code = e.Find("error_code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "startup_in_progress");
  const godot_autopilot::util::JsonValue *verdict_code = e.Find("code");
  ASSERT_NE(verdict_code, nullptr);
  ASSERT_TRUE(verdict_code->IsString());
  EXPECT_EQ(verdict_code->GetString(), "startup_in_progress");
  const godot_autopilot::util::JsonValue *retryable = e.Find("retryable");
  ASSERT_NE(retryable, nullptr);
  ASSERT_TRUE(retryable->IsBool());
  EXPECT_TRUE(retryable->GetBool());
  const godot_autopilot::util::JsonValue *hint = e.Find("hint");
  ASSERT_NE(hint, nullptr);
  ASSERT_TRUE(hint->IsString());
  EXPECT_FALSE(std::string(hint->GetString()).empty());
  const godot_autopilot::util::JsonValue *retry_after = e.Find("retry_after_ms");
  ASSERT_NE(retry_after, nullptr);
  ASSERT_TRUE(retry_after->IsInt());
  EXPECT_GT(retry_after->GetInt(), 0);
  EXPECT_FALSE(error_text(e).empty());
}

TEST(EditorStartupGateTest, StartupErrorDropsMisattributionText) {
  const std::string text = error_text(startup_in_progress_error());
  EXPECT_FALSE(contains(text, "ensure the current scene is saved and runnable"));
  EXPECT_FALSE(contains(text, "当前没有可编辑的场景根节点"));
  EXPECT_FALSE(contains(text, "parent node not found"));
  EXPECT_FALSE(contains(text, "no game process started"));
}

TEST(EditorStartupGateTest, GatedErrorInWindowReturnsStructuredStartup) {
  const godot_autopilot::util::JsonValue gated =
      startup_gated_error(true, normal_error(kPlayNormalError));
  const godot_autopilot::util::JsonValue *code = gated.Find("error_code");
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(code->GetString(), "startup_in_progress");
  const godot_autopilot::util::JsonValue *retryable = gated.Find("retryable");
  ASSERT_NE(retryable, nullptr);
  EXPECT_TRUE(retryable->GetBool());
  EXPECT_NE(error_text(gated), kPlayNormalError);
  EXPECT_FALSE(contains(error_text(gated),
                        "ensure the current scene is saved and runnable"));
}

TEST(EditorStartupGateTest, GatedErrorOutsideWindowKeepsPlayTextVerbatim) {
  const godot_autopilot::util::JsonValue gated =
      startup_gated_error(false, normal_error(kPlayNormalError));
  EXPECT_EQ(error_text(gated), kPlayNormalError);
  EXPECT_EQ(gated.Find("error_code"), nullptr);
  EXPECT_EQ(gated.Find("retryable"), nullptr);
  EXPECT_EQ(gated.Find("hint"), nullptr);
}

TEST(EditorStartupGateTest, GatedErrorOutsideWindowKeepsCreateTextVerbatim) {
  const godot_autopilot::util::JsonValue gated =
      startup_gated_error(false, normal_error(kCreateNormalError));
  EXPECT_EQ(error_text(gated), kCreateNormalError);
  EXPECT_EQ(gated.Find("error_code"), nullptr);
  EXPECT_EQ(gated.Find("retryable"), nullptr);
}

TEST(EditorStartupGateTest, GatedErrorInWindowReplacesCreateMisattribution) {
  const godot_autopilot::util::JsonValue gated =
      startup_gated_error(true, normal_error(kCreateNormalError));
  const godot_autopilot::util::JsonValue *code = gated.Find("error_code");
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(code->GetString(), "startup_in_progress");
  EXPECT_FALSE(contains(error_text(gated), "parent node not found"));
  EXPECT_FALSE(contains(error_text(gated), "当前没有可编辑的场景根节点"));
}