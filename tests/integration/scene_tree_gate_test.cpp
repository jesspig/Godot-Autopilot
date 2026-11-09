#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class SceneTreeGateDenied : public IntegrationHarnessWithAllow {
 public:
  SceneTreeGateDenied() : IntegrationHarnessWithAllow("none") {}
};

class SceneTreeGateAllowed : public IntegrationHarnessWithAllow {
 public:
  SceneTreeGateAllowed() : IntegrationHarnessWithAllow("game_runtime") {}
};

mcp::JsonValue make_args() {
  return mcp::JsonValue(mcp::JsonValue::object_tag);
}

void assert_authorization_error(const mcp::JsonValue &result) {
  const auto *v = result.Find("error");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsString()) << result.Dump();
  EXPECT_FALSE(v->GetString().empty());
  EXPECT_TRUE(v->GetString().find("authorization required") != std::string::npos)
      << result.Dump();
}

void assert_channel_error_not_authorization(const mcp::JsonValue &result) {
  const auto *v = result.Find("error");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsString()) << result.Dump();
  EXPECT_FALSE(v->GetString().empty());
  EXPECT_TRUE(v->GetString().find("authorization required") == std::string::npos)
      << result.Dump();
}

TEST_F(SceneTreeGateDenied, SetSceneTreePauseDeniedWithoutAllow) {
  mcp::JsonValue args = make_args();
  args["paused"] = mcp::JsonValue(true);
  mcp::JsonValue result = call("set_scene_tree_pause", args);
  assert_authorization_error(result);
}

TEST_F(SceneTreeGateDenied, IsSceneTreePausedDeniedWithoutAllow) {
  mcp::JsonValue result = call("is_scene_tree_paused", make_args());
  assert_authorization_error(result);
}

TEST_F(SceneTreeGateDenied, ReloadSceneTreeCurrentSceneDeniedWithoutAllow) {
  mcp::JsonValue result = call("reload_scene_tree_current_scene", make_args());
  assert_authorization_error(result);
}

TEST_F(SceneTreeGateDenied, SetSceneTreeDebugCollisionsHintDeniedWithoutAllow) {
  mcp::JsonValue args = make_args();
  args["enabled"] = mcp::JsonValue(true);
  mcp::JsonValue result = call("set_scene_tree_debug_collisions_hint", args);
  assert_authorization_error(result);
}

TEST_F(SceneTreeGateDenied, CallSceneTreeGroupDeniedWithoutAllow) {
  mcp::JsonValue args = make_args();
  args["group_name"] = mcp::JsonValue("gda_scene_tree_gate_probe");
  args["method"] = mcp::JsonValue("gda_probe_method");
  mcp::JsonValue result = call("call_scene_tree_group", args);
  assert_authorization_error(result);
}

TEST_F(SceneTreeGateDenied, NotifySceneTreeGroupDeniedWithoutAllow) {
  mcp::JsonValue args = make_args();
  args["group_name"] = mcp::JsonValue("gda_scene_tree_gate_probe");
  args["notification"] = mcp::JsonValue(13);
  mcp::JsonValue result = call("notify_scene_tree_group", args);
  assert_authorization_error(result);
}

TEST_F(SceneTreeGateDenied, GetSceneTreeNodesInGroupDeniedWithoutAllow) {
  mcp::JsonValue args = make_args();
  args["group_name"] = mcp::JsonValue("gda_scene_tree_gate_probe");
  mcp::JsonValue result = call("get_scene_tree_nodes_in_group", args);
  assert_authorization_error(result);
}

TEST_F(SceneTreeGateDenied, CreateSceneTreeTimerDeniedWithoutAllow) {
  mcp::JsonValue args = make_args();
  args["delay_sec"] = mcp::JsonValue(0.05);
  args["process_always"] = mcp::JsonValue(true);
  args["process_in_physics"] = mcp::JsonValue(false);
  mcp::JsonValue result = call("create_scene_tree_timer", args);
  assert_authorization_error(result);
}

TEST_F(SceneTreeGateAllowed, SetSceneTreePauseChannelErrorWhenNoGame) {
  mcp::JsonValue args = make_args();
  args["paused"] = mcp::JsonValue(true);
  mcp::JsonValue result = call("set_scene_tree_pause", args);
  assert_channel_error_not_authorization(result);
}

TEST_F(SceneTreeGateAllowed, IsSceneTreePausedChannelErrorWhenNoGame) {
  mcp::JsonValue result = call("is_scene_tree_paused", make_args());
  assert_channel_error_not_authorization(result);
}

TEST_F(SceneTreeGateAllowed, ReloadSceneTreeCurrentSceneChannelErrorWhenNoGame) {
  mcp::JsonValue result = call("reload_scene_tree_current_scene", make_args());
  assert_channel_error_not_authorization(result);
}

TEST_F(SceneTreeGateAllowed,
       SetSceneTreeDebugCollisionsHintChannelErrorWhenNoGame) {
  mcp::JsonValue args = make_args();
  args["enabled"] = mcp::JsonValue(true);
  mcp::JsonValue result = call("set_scene_tree_debug_collisions_hint", args);
  assert_channel_error_not_authorization(result);
}

TEST_F(SceneTreeGateAllowed, CallSceneTreeGroupChannelErrorWhenNoGame) {
  mcp::JsonValue args = make_args();
  args["group_name"] = mcp::JsonValue("gda_scene_tree_gate_probe");
  args["method"] = mcp::JsonValue("gda_probe_method");
  mcp::JsonValue result = call("call_scene_tree_group", args);
  assert_channel_error_not_authorization(result);
}

TEST_F(SceneTreeGateAllowed, NotifySceneTreeGroupChannelErrorWhenNoGame) {
  mcp::JsonValue args = make_args();
  args["group_name"] = mcp::JsonValue("gda_scene_tree_gate_probe");
  args["notification"] = mcp::JsonValue(13);
  mcp::JsonValue result = call("notify_scene_tree_group", args);
  assert_channel_error_not_authorization(result);
}

TEST_F(SceneTreeGateAllowed, GetSceneTreeNodesInGroupChannelErrorWhenNoGame) {
  mcp::JsonValue args = make_args();
  args["group_name"] = mcp::JsonValue("gda_scene_tree_gate_probe");
  mcp::JsonValue result = call("get_scene_tree_nodes_in_group", args);
  assert_channel_error_not_authorization(result);
}

TEST_F(SceneTreeGateAllowed, CreateSceneTreeTimerChannelErrorWhenNoGame) {
  mcp::JsonValue args = make_args();
  args["delay_sec"] = mcp::JsonValue(0.05);
  args["process_always"] = mcp::JsonValue(true);
  args["process_in_physics"] = mcp::JsonValue(false);
  mcp::JsonValue result = call("create_scene_tree_timer", args);
  assert_channel_error_not_authorization(result);
}

} // namespace
