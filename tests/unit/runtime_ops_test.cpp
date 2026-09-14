#include "core/command_queue.hpp"
#include "tools/runtime_ops.hpp"

#include <gtest/gtest.h>

using godot_autopilot::CommandQueue;
using godot_autopilot::runtime_ops::discard_pending;
using godot_autopilot::runtime_ops::has_editor_queue;
using godot_autopilot::runtime_ops::payload_is_pending;
using godot_autopilot::runtime_ops::set_editor_queue;

TEST(RuntimeOpsTest, EditorQueueInjectionRoundTrip) {
    CommandQueue q;
    EXPECT_FALSE(has_editor_queue());
    set_editor_queue(&q);
    EXPECT_TRUE(has_editor_queue());
    set_editor_queue(nullptr);
    EXPECT_FALSE(has_editor_queue());
    set_editor_queue(nullptr);
}

TEST(RuntimeOpsTest, PayloadIsPending) {
    int64_t request_id = 0;
    mcp::JsonValue pending = mcp::JsonValue::Parse("{\"__gda_pending\": 7}");
    EXPECT_TRUE(payload_is_pending(pending, &request_id));
    EXPECT_EQ(request_id, 7);

    mcp::JsonValue empty(mcp::JsonValue::object_tag);
    EXPECT_FALSE(payload_is_pending(empty, &request_id));

    mcp::JsonValue non_integer =
        mcp::JsonValue::Parse("{\"__gda_pending\": \"x\"}");
    EXPECT_FALSE(payload_is_pending(non_integer, &request_id));

    mcp::JsonValue string_value = mcp::JsonValue::Parse("\"not-an-object\"");
    EXPECT_FALSE(payload_is_pending(string_value, &request_id));

    mcp::JsonValue array_value = mcp::JsonValue::Parse("[1, 2]");
    EXPECT_FALSE(payload_is_pending(array_value, &request_id));
}

TEST(RuntimeOpsTest, DiscardMissingPendingReturnsFalse) {
    std::string detail;
    EXPECT_FALSE(discard_pending(123, detail));
    EXPECT_FALSE(detail.empty());
}
