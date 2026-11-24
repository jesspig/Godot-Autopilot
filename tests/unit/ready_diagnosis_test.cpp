#include "tools/domains/debugger/debugger_ops.hpp"

#include <gtest/gtest.h>

using godot_autopilot::debugger_ops::ready_diagnosis;
using godot_autopilot::debugger_ops::SessionReadiness;

TEST(ReadyDiagnosisTest, NoResponseMeansExtensionMissing) {
  SessionReadiness state;
  EXPECT_STREQ(ready_diagnosis(state), "extension_not_loaded");
}

TEST(ReadyDiagnosisTest, ReadyWithoutScene) {
  SessionReadiness state;
  state.extension_responded = true;
  state.ready = true;
  EXPECT_STREQ(ready_diagnosis(state), "scene_not_ready");
}

TEST(ReadyDiagnosisTest, ReadyWithScene) {
  SessionReadiness state;
  state.extension_responded = true;
  state.ready = true;
  state.scene_ready = true;
  EXPECT_STREQ(ready_diagnosis(state), "ready");
}

TEST(ReadyDiagnosisTest, RespondedWithoutReady) {
  SessionReadiness state;
  state.extension_responded = true;
  EXPECT_STREQ(ready_diagnosis(state), "ready_lost");
}
