#include "core/command_queue.hpp"
#include "tools/runtime_ops.hpp"

#include <deque>
#include <gtest/gtest.h>
#include <string>

using godot_autopilot::CommandQueue;
using godot_autopilot::runtime_ops::discard_pending;
using godot_autopilot::runtime_ops::GameJobTable;
using godot_autopilot::runtime_ops::has_editor_queue;
using godot_autopilot::runtime_ops::LateResult;
using godot_autopilot::runtime_ops::late_result_summary;
using godot_autopilot::runtime_ops::needs_error_break_suppression;
using godot_autopilot::runtime_ops::payload_is_pending;
using godot_autopilot::runtime_ops::push_late_result;
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

TEST(RuntimeOpsTest, NeedsErrorBreakSuppressionOnlyForEvalScript) {
    EXPECT_TRUE(needs_error_break_suppression("eval", "script"));
    EXPECT_FALSE(needs_error_break_suppression("eval", "get_property"));
    EXPECT_FALSE(needs_error_break_suppression("eval", "set_property"));
    EXPECT_FALSE(needs_error_break_suppression("eval", "call_method"));
    EXPECT_FALSE(needs_error_break_suppression("status", "script"));
    EXPECT_FALSE(needs_error_break_suppression("input", "script"));
    EXPECT_FALSE(needs_error_break_suppression("", "script"));
    EXPECT_FALSE(needs_error_break_suppression("eval", ""));
    EXPECT_FALSE(needs_error_break_suppression("eval", "unknown"));
}

TEST(RuntimeOpsTest, LateResultBufferKeepsNewestFiveInOrder) {
    std::deque<LateResult> buffer;
    for (int64_t i = 1; i <= 6; ++i) {
        LateResult entry;
        entry.request_id = i;
        entry.op = "status";
        entry.age_ms = i * 10;
        entry.summary = "summary-" + std::to_string(i);
        push_late_result(buffer, entry, 5u);
    }
    ASSERT_EQ(buffer.size(), 5u);
    EXPECT_EQ(buffer.front().request_id, 2);
    EXPECT_EQ(buffer.back().request_id, 6);
    for (size_t i = 0; i < buffer.size(); ++i) {
        EXPECT_EQ(buffer[i].request_id, static_cast<int64_t>(i + 2));
        EXPECT_EQ(buffer[i].op, "status");
        EXPECT_EQ(buffer[i].age_ms, static_cast<int64_t>(i + 2) * 10);
        EXPECT_EQ(buffer[i].summary, "summary-" + std::to_string(i + 2));
    }
}

TEST(RuntimeOpsTest, LateResultBufferAppendsFifoAndIgnoresZeroCap) {
    std::deque<LateResult> buffer;
    LateResult first;
    first.request_id = 1;
    first.op = "eval";
    first.age_ms = 10;
    first.summary = "a";
    push_late_result(buffer, first, 5u);
    LateResult second;
    second.request_id = 2;
    second.op = "input";
    second.age_ms = 20;
    second.summary = "b";
    push_late_result(buffer, second, 5u);
    ASSERT_EQ(buffer.size(), 2u);
    EXPECT_EQ(buffer.front().request_id, 1);
    EXPECT_EQ(buffer.front().op, "eval");
    EXPECT_EQ(buffer.front().age_ms, 10);
    EXPECT_EQ(buffer.back().request_id, 2);
    EXPECT_EQ(buffer.back().op, "input");
    EXPECT_EQ(buffer.back().age_ms, 20);
    push_late_result(buffer, first, 0u);
    EXPECT_EQ(buffer.size(), 2u);
}

TEST(RuntimeOpsTest, LateResultSummaryPrefersErrorText) {
    mcp::JsonValue both = mcp::JsonValue::Parse(
        "{\"error\":\"boom\",\"result\":{\"x\":1}}");
    EXPECT_EQ(late_result_summary(both), "\"boom\"");

    mcp::JsonValue only_result = mcp::JsonValue::Parse("{\"result\":{\"x\":1}}");
    EXPECT_EQ(late_result_summary(only_result), "{\"x\":1}");

    mcp::JsonValue neither = mcp::JsonValue::Parse("{\"other\":1}");
    EXPECT_TRUE(late_result_summary(neither).empty());
}

TEST(RuntimeOpsTest, LateResultSummaryTruncatesAtTwoHundredChars) {
    const std::string exact_payload(198, 'a');
    mcp::JsonValue exact =
        mcp::JsonValue::Parse("{\"error\":\"" + exact_payload + "\"}");
    EXPECT_EQ(late_result_summary(exact), "\"" + exact_payload + "\"");
    EXPECT_EQ(late_result_summary(exact).size(), 200u);

    const std::string over_payload(200, 'b');
    mcp::JsonValue over =
        mcp::JsonValue::Parse("{\"error\":\"" + over_payload + "\"}");
    EXPECT_EQ(late_result_summary(over).size(), 200u);
    EXPECT_EQ(late_result_summary(over), "\"" + over_payload.substr(0, 199));

    const std::string long_result(300, 'c');
    mcp::JsonValue long_result_json =
        mcp::JsonValue::Parse("{\"result\":\"" + long_result + "\"}");
    EXPECT_EQ(late_result_summary(long_result_json).size(), 200u);
    EXPECT_EQ(late_result_summary(long_result_json),
              "\"" + long_result.substr(0, 199));
}

TEST(RuntimeOpsTest, GameJobTableRegisterCapacity) {
    GameJobTable table;
    for (int64_t i = 0; i < 16; ++i) {
        int64_t job_id = table.register_job(100 + i, "status", 1000, 0);
        EXPECT_EQ(job_id, i + 1);
        EXPECT_TRUE(table.contains(job_id));
    }
    EXPECT_EQ(table.register_job(999, "status", 1000, 0), -1);
}

TEST(RuntimeOpsTest, GameJobTableCollectRemoves) {
    GameJobTable table;
    int64_t first = table.register_job(42, "eval", 1000, 100);
    int64_t second = table.register_job(43, "status", 1000, 100);
    GameJobTable::CollectOutcome outcome = table.collect(first, 200);
    EXPECT_TRUE(outcome.found);
    EXPECT_FALSE(outcome.expired);
    EXPECT_EQ(outcome.request_id, 42);
    EXPECT_FALSE(outcome.had_suppression);
    EXPECT_FALSE(table.contains(first));
    EXPECT_TRUE(table.contains(second));
    GameJobTable::CollectOutcome missing = table.collect(999, 200);
    EXPECT_FALSE(missing.found);
}

TEST(RuntimeOpsTest, GameJobTableExpiryBoundary) {
    GameJobTable table;
    int64_t exact = table.register_job(1, "status", 1000, 0);
    GameJobTable::CollectOutcome at_boundary = table.collect(exact, 3000);
    EXPECT_TRUE(at_boundary.found);
    EXPECT_FALSE(at_boundary.expired);
    int64_t over = table.register_job(2, "status", 1000, 0);
    GameJobTable::CollectOutcome past_boundary = table.collect(over, 3001);
    EXPECT_TRUE(past_boundary.found);
    EXPECT_TRUE(past_boundary.expired);
    EXPECT_EQ(past_boundary.request_id, 2);
}

TEST(RuntimeOpsTest, GameJobTableSweepOnStartRemovesOnlyExpired) {
    GameJobTable table;
    table.register_job(1, "status", 1000, 0);
    table.register_job(2, "status", 1000, 0);
    table.register_job(3, "status", 100000, 0);
    EXPECT_EQ(table.sweep_on_start(3001), 2u);
    EXPECT_FALSE(table.contains(1));
    EXPECT_FALSE(table.contains(2));
    EXPECT_TRUE(table.contains(3));
}
