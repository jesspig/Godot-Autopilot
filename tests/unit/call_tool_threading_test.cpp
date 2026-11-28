#include <gtest/gtest.h>

#include <functional>
#include <mutex>
#include <string>

#include "core/command_queue.hpp"
#include "core/control/native_call.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/dynamic_spec_store.hpp"
#include "tools/base/register_all.hpp"
#include "tools/base/runtime_ops.hpp"
#include "tools/base/tool_catalog.hpp"
#include "tools/base/tool_spec.hpp"
#include "util/bm25_index.hpp"
#include "util/json_value.hpp"

namespace {

using godot_autopilot::ToolCatalog;
using godot_autopilot::ToolSpec;
using godot_autopilot::ToolSurface;
using godot_autopilot::util::JsonValue;

class CallToolThreadingFixture : public ::testing::Test {
protected:
  void SetUp() override {
    queue_.drain();
    godot_autopilot::install_tool_handlers(queue_, catalog_, index_, 9541,
                                           ToolSurface::Meta);
    godot_autopilot::runtime_ops::set_editor_queue(&queue_);
    godot_autopilot::dynamic_specs::enabled().store(true);
  }

  void TearDown() override {
    {
      std::lock_guard<std::mutex> lock(godot_autopilot::dynamic_specs::mutex());
      godot_autopilot::dynamic_specs::store().clear();
    }
    godot_autopilot::refresh_dynamic_tools();
    godot_autopilot::clear_active_registry();
    godot_autopilot::dispatch::clear_handlers();
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
  }

  void add_dynamic(const std::string &name,
                   std::function<JsonValue(const JsonValue &)> handler) {
    ToolSpec spec{name, "Deferred threading test tool", "Testing", {"dynamic"}};
    spec.side_effect = godot_autopilot::SideEffect::None;
    spec.flags = godot_autopilot::tool_flags::kDynamic;
    spec.handler = std::move(handler);
    std::lock_guard<std::mutex> lock(godot_autopilot::dynamic_specs::mutex());
    godot_autopilot::dynamic_specs::store().push_back(std::move(spec));
  }

  godot_autopilot::CommandQueue queue_;
  ToolCatalog catalog_;
  godot_autopilot::Bm25Index index_;
};

} // namespace

TEST_F(CallToolThreadingFixture, CallToolKeepsDeferredMarkerForTransportThread) {
  add_dynamic("call_tool_threading_pending", [](const JsonValue &) {
    JsonValue marker(JsonValue::object_tag);
    marker["__gda_pending"] = JsonValue(4242);
    marker["timeout_ms"] = JsonValue(100);
    return marker;
  });
  godot_autopilot::refresh_dynamic_tools();

  const JsonValue args = JsonValue::Parse(
      R"({"name":"call_tool_threading_pending","arguments":{}})");
  const JsonValue result = godot_autopilot::dispatch::call_handler(
      "call_tool", args, godot_autopilot::dispatch::capture_depth_context());

  ASSERT_TRUE(result.IsObject());
  EXPECT_NE(result.Find("__gda_pending"), nullptr);
  EXPECT_EQ(result.Dump().find("pending request not found"), std::string::npos);
}

TEST(CallToolThreadingTest, AwaitPendingUsesInnerToolNameAndKeepsImageContent) {
  gda::control::native::NativeCallResult call;
  call.tool = "call_tool";
  call.deferred_tool = "capture_game_viewport";
  call.pending = true;
  JsonValue payload(JsonValue::object_tag);
  JsonValue inner(JsonValue::object_tag);
  inner["data"] = JsonValue("aGVsbG8=");
  inner["format"] = JsonValue("png");
  payload["result"] = std::move(inner);
  call.payload = std::move(payload);

  const gda::control::native::NativeCallResult resolved =
      gda::control::native::await_pending(call, 100);

  EXPECT_FALSE(resolved.pending);
  const JsonValue *content = resolved.result.Find("content");
  ASSERT_NE(content, nullptr);
  ASSERT_TRUE(content->IsArray());
  bool has_image = false;
  for (const JsonValue &item : content->GetArray()) {
    const JsonValue *type = item.Find("type");
    if (type != nullptr && type->IsString() && type->GetString() == "image") {
      has_image = true;
    }
  }
  EXPECT_TRUE(has_image);
}
