#include "tools/base/batch_verdict.hpp"

#include <gtest/gtest.h>

#include "util/json_value.hpp"

using godot_autopilot::batch_detail::op_succeeded;

namespace {
godot_autopilot::util::JsonValue make(const char *key, const char *val) {
  godot_autopilot::util::JsonValue j(godot_autopilot::util::JsonValue::object_tag);
  j[key] = godot_autopilot::util::JsonValue(val);
  return j;
}
} // namespace

TEST(BatchVerdictTest, PlainOkSucceeds) {
  EXPECT_TRUE(op_succeeded(make("result", "ok")));
}

TEST(BatchVerdictTest, TopLevelErrorFails) {
  EXPECT_FALSE(op_succeeded(make("error", "boom")));
}

TEST(BatchVerdictTest, StructuredErrorFailsWithoutTopLevelError) {
  godot_autopilot::util::JsonValue j(godot_autopilot::util::JsonValue::object_tag);
  godot_autopilot::util::JsonValue se(godot_autopilot::util::JsonValue::object_tag);
  se["code"] = godot_autopilot::util::JsonValue("backend_down");
  j["structured_error"] = std::move(se);
  EXPECT_FALSE(op_succeeded(j));
}

TEST(BatchVerdictTest, RuntimeErrorFails) {
  godot_autopilot::util::JsonValue j(godot_autopilot::util::JsonValue::object_tag);
  j["runtime_error"] = godot_autopilot::util::JsonValue(true);
  EXPECT_FALSE(op_succeeded(j));
}
