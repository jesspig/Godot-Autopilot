#include "util/script_capability.hpp"

#include <gtest/gtest.h>

using godot_autopilot::script_capability::classify;
using godot_autopilot::script_capability::Code;

TEST(ScriptCapabilityTest, GdScriptNodeIsMountableAndRunnable) {
  const auto r = classify(true, false, true, "GDScript", "Node", false, false,
                          true, true);
  EXPECT_TRUE(r.runtime_mountable);
  EXPECT_TRUE(r.editor_runnable);
  EXPECT_EQ(r.code, Code::kMountable);
}

TEST(ScriptCapabilityTest, NonToolCSharpNodeIsRuntimeOnly) {
  const auto r = classify(true, false, true, "CSharpScript", "Node", false,
                          false, false, true);
  EXPECT_TRUE(r.runtime_mountable);
  EXPECT_FALSE(r.editor_runnable);
  EXPECT_EQ(r.code, Code::kRuntimeOnly);
}

TEST(ScriptCapabilityTest, ToolCSharpNodeIsRunnable) {
  const auto r = classify(true, false, true, "CSharpScript", "Node", false,
                          true, true, true);
  EXPECT_TRUE(r.runtime_mountable);
  EXPECT_TRUE(r.editor_runnable);
  EXPECT_EQ(r.code, Code::kMountable);
}

TEST(ScriptCapabilityTest, MissingClassIsRejected) {
  const auto r = classify(true, false, true, "CSharpScript", "", false, false,
                          false, false);
  EXPECT_FALSE(r.runtime_mountable);
  EXPECT_EQ(r.code, Code::kClassMissing);
}

TEST(ScriptCapabilityTest, NonNodeBaseIsRejected) {
  const auto r = classify(true, false, true, "GDScript", "Resource", false,
                          false, true, false);
  EXPECT_FALSE(r.runtime_mountable);
  EXPECT_EQ(r.code, Code::kNotANode);
}

TEST(ScriptCapabilityTest, AbstractScriptIsRejected) {
  const auto r = classify(true, false, true, "GDScript", "Node", true, false,
                          false, true);
  EXPECT_FALSE(r.runtime_mountable);
  EXPECT_EQ(r.code, Code::kAbstract);
}

TEST(ScriptCapabilityTest, PackedSceneNeedsNodeRoot) {
  const auto ok = classify(true, true, false, "PackedScene", "Node", false,
                           false, true, true);
  EXPECT_TRUE(ok.runtime_mountable);
  EXPECT_EQ(ok.code, Code::kMountable);
  const auto bad = classify(true, true, false, "PackedScene", "", false, false,
                            true, false);
  EXPECT_FALSE(bad.runtime_mountable);
  EXPECT_EQ(bad.code, Code::kNotANode);
}

TEST(ScriptCapabilityTest, ToolScriptWithoutInstanceIsRejected) {
  const auto r = classify(true, false, true, "GDScript", "Node", false, true,
                          false, true);
  EXPECT_FALSE(r.runtime_mountable);
  EXPECT_EQ(r.code, Code::kInvalidScript);
}

TEST(ScriptCapabilityTest, NonToolGdScriptIsRuntimeOnly) {
  const auto r = classify(true, false, true, "GDScript", "Node", false, false,
                          false, true);
  EXPECT_TRUE(r.runtime_mountable);
  EXPECT_FALSE(r.editor_runnable);
  EXPECT_EQ(r.code, Code::kRuntimeOnly);
}
