#include "tools/base/batch_verdict.hpp"

#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

using godot_autopilot::batch_detail::op_succeeded;

namespace {
mcp::JsonValue make(const char *key, const char *val) {
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j[key] = mcp::JsonValue(val);
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
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  mcp::JsonValue se(mcp::JsonValue::object_tag);
  se["code"] = mcp::JsonValue("backend_down");
  j["structured_error"] = std::move(se);
  EXPECT_FALSE(op_succeeded(j));
}

TEST(BatchVerdictTest, RuntimeErrorFails) {
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["runtime_error"] = mcp::JsonValue(true);
  EXPECT_FALSE(op_succeeded(j));
}
