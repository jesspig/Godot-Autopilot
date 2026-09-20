#include "core/monitor.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using godot_autopilot::LogEntry;
using godot_autopilot::LogLevel;
using godot_autopilot::LogSystem;
using godot_autopilot::TraceEvent;
using godot_autopilot::TraceKind;
using godot_autopilot::TraceRecorder;

TEST(MonitorTest, TraceKindNameCoversAllKinds) {
    using godot_autopilot::trace_kind_name;
    EXPECT_STREQ(trace_kind_name(TraceKind::ToolCall), "tool_call");
    EXPECT_STREQ(trace_kind_name(TraceKind::ProtocolRequest), "protocol_request");
    EXPECT_STREQ(trace_kind_name(TraceKind::ProtocolResponse), "protocol_response");
    EXPECT_STREQ(trace_kind_name(TraceKind::ProtocolError), "protocol_error");
    EXPECT_STREQ(trace_kind_name(TraceKind::ProtocolNotification),
                 "protocol_notification");
    EXPECT_STREQ(trace_kind_name(TraceKind::Lifecycle), "lifecycle");
    EXPECT_STREQ(trace_kind_name(TraceKind::DataFlow), "data_flow");
    EXPECT_STREQ(trace_kind_name(TraceKind::ExecutionState), "execution_state");
    EXPECT_STREQ(trace_kind_name(TraceKind::Concurrency), "concurrency");
    EXPECT_STREQ(trace_kind_name(TraceKind::Perf), "perf");
    EXPECT_STREQ(trace_kind_name(TraceKind::Snapshot), "snapshot");
    EXPECT_STREQ(trace_kind_name(TraceKind::Error), "error");
    EXPECT_STREQ(trace_kind_name(TraceKind::PersistHealth), "persist_health");
    EXPECT_STREQ(trace_kind_name(TraceKind::UiAction), "ui_action");
    EXPECT_STREQ(trace_kind_name(TraceKind::Security), "security");
}

TEST(MonitorTest, RequestRegistryPutGetTakeAndClear) {
    using godot_autopilot::monitor::registry;
    using godot_autopilot::monitor::RequestRecord;
    registry().clear();
    EXPECT_EQ(registry().size(), 0u);

    RequestRecord record;
    record.request_id = "monitor_reg_req_1";
    record.trace_id = "monitor_reg_trace_1";
    record.method = "tools/call";
    record.begin_monotonic_ns = 100;
    registry().put(record);
    EXPECT_EQ(registry().size(), 1u);

    RequestRecord out;
    EXPECT_TRUE(registry().get("monitor_reg_req_1", &out));
    EXPECT_EQ(out.trace_id, "monitor_reg_trace_1");
    EXPECT_EQ(out.method, "tools/call");
    EXPECT_FALSE(registry().get("monitor_reg_missing", &out));

    EXPECT_TRUE(registry().take("monitor_reg_req_1", &out));
    EXPECT_EQ(out.request_id, "monitor_reg_req_1");
    EXPECT_FALSE(registry().get("monitor_reg_req_1", &out));
    EXPECT_EQ(registry().size(), 0u);

    registry().put(record);
    EXPECT_EQ(registry().size(), 1u);
    registry().clear();
    EXPECT_EQ(registry().size(), 0u);
}

TEST(MonitorTest, RequestRegistryExpireBoundaries) {
    using godot_autopilot::monitor::registry;
    using godot_autopilot::monitor::RequestRecord;
    registry().clear();

    RequestRecord record;
    record.request_id = "monitor_exp_req_1";
    record.begin_monotonic_ns = 100;
    registry().put(record);

    const std::vector<RequestRecord> at_boundary = registry().expire(100, 0);
    EXPECT_TRUE(at_boundary.empty());
    EXPECT_EQ(registry().size(), 1u);

    const std::vector<RequestRecord> past_boundary = registry().expire(101, 0);
    ASSERT_EQ(past_boundary.size(), 1u);
    EXPECT_EQ(past_boundary[0].request_id, "monitor_exp_req_1");
    EXPECT_EQ(registry().size(), 0u);

    registry().put(record);
    const std::vector<RequestRecord> huge_timeout =
        registry().expire(1000000000, 1000000000000LL);
    EXPECT_TRUE(huge_timeout.empty());
    EXPECT_EQ(registry().size(), 1u);

    registry().clear();
    EXPECT_EQ(registry().size(), 0u);
}

TEST(MonitorTest, EmitAssignsStrictlyIncreasingSeqAndIsQueryable) {
    using godot_autopilot::monitor::emit;
    TraceRecorder::instance().clear_for_test();

    const uint64_t first = emit(TraceEvent{});
    const uint64_t second = emit(TraceEvent{});
    const uint64_t third = emit(TraceEvent{});
    EXPECT_LT(first, second);
    EXPECT_LT(second, third);

    const std::vector<TraceEvent> recent = TraceRecorder::instance().query_recent(3);
    ASSERT_EQ(recent.size(), 3u);
    EXPECT_EQ(recent.front().seq, first);
    EXPECT_EQ(recent.back().seq, third);
    EXPECT_FALSE(recent.back().thread_id.empty());
    EXPECT_FALSE(recent.back().monotonic_ns == 0);

    TraceRecorder::instance().clear_for_test();
    EXPECT_EQ(TraceRecorder::instance().size(), 0u);
}

TEST(MonitorTest, ToolCallRecordsFailedStateAndWritesTraceableLog) {
    using godot_autopilot::monitor::tool_call;
    TraceRecorder::instance().clear_for_test();

    const std::string trace_marker = "MONITOR_TOOLCALL_TRACE_93f1";
    TraceEvent event;
    event.tool = "monitor_probe_tool";
    event.ok = false;
    event.trace_id = trace_marker;
    event.span_id = "MONITOR_TOOLCALL_SPAN_93f1";

    const uint64_t seq = tool_call(event, LogLevel::Error,
                                   "MONITOR_TOOLCALL_SUMMARY_93f1",
                                   "MONITOR_TOOLCALL_DETAIL_93f1");
    EXPECT_GT(seq, 0u);

    const std::vector<TraceEvent> events = TraceRecorder::instance().query_recent(5);
    ASSERT_FALSE(events.empty());
    const TraceEvent &recorded = events.back();
    EXPECT_EQ(recorded.kind, TraceKind::ToolCall);
    EXPECT_EQ(recorded.state, "failed");
    EXPECT_EQ(recorded.name, "monitor_probe_tool");
    EXPECT_EQ(recorded.trace_id, trace_marker);

    LogSystem::Query query;
    query.filter_text = trace_marker;
    const std::vector<LogEntry> hits = LogSystem::instance().query(query);
    ASSERT_FALSE(hits.empty());
    EXPECT_EQ(hits[0].trace_id, trace_marker);

    TraceRecorder::instance().clear_for_test();
}

TEST(MonitorTest, BeginEndRequestLinksProtocolEventsByTraceparent) {
    using godot_autopilot::monitor::begin_request;
    using godot_autopilot::monitor::end_request;
    using godot_autopilot::monitor::registry;
    using godot_autopilot::monitor::RequestRecord;
    TraceRecorder::instance().clear_for_test();
    registry().clear();

    const std::string request_id = "monitor_req_7c31";
    const std::string traceparent =
        "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01";
    const RequestRecord record =
        begin_request(request_id, "tools/call", "params_digest", traceparent);
    EXPECT_EQ(record.trace_id, "4bf92f3577b34da6a3ce929d0e0e4736");
    EXPECT_EQ(record.correlation_id, traceparent);
    EXPECT_EQ(registry().size(), 1u);

    const std::vector<TraceEvent> begin_events =
        TraceRecorder::instance().query_by_request(request_id);
    ASSERT_FALSE(begin_events.empty());
    bool saw_request = false;
    for (const TraceEvent &event : begin_events) {
        if (event.kind == TraceKind::ProtocolRequest) {
            saw_request = true;
        }
    }
    EXPECT_TRUE(saw_request);

    end_request(request_id, true, "", 42);
    EXPECT_EQ(registry().size(), 0u);

    const std::vector<TraceEvent> all =
        TraceRecorder::instance().query_by_request(request_id);
    bool saw_response = false;
    for (const TraceEvent &event : all) {
        if (event.kind == TraceKind::ProtocolResponse &&
            event.trace_id == record.trace_id) {
            saw_response = true;
        }
    }
    EXPECT_TRUE(saw_response);

    TraceRecorder::instance().clear_for_test();
    registry().clear();
}

TEST(MonitorTest, RequestStackAndScopeRestore) {
    using godot_autopilot::monitor::current_request_id;
    using godot_autopilot::monitor::pop_request;
    using godot_autopilot::monitor::push_request;
    using godot_autopilot::monitor::RequestScope;

    while (!current_request_id().empty()) {
        pop_request();
    }
    EXPECT_EQ(current_request_id(), "");

    push_request("monitor_stack_a");
    EXPECT_EQ(current_request_id(), "monitor_stack_a");
    push_request("monitor_stack_b");
    EXPECT_EQ(current_request_id(), "monitor_stack_b");
    pop_request();
    EXPECT_EQ(current_request_id(), "monitor_stack_a");

    {
        RequestScope scope("monitor_stack_scope");
        EXPECT_EQ(current_request_id(), "monitor_stack_scope");
        push_request("monitor_stack_inner");
        EXPECT_EQ(current_request_id(), "monitor_stack_inner");
        pop_request();
        EXPECT_EQ(current_request_id(), "monitor_stack_scope");
    }
    EXPECT_EQ(current_request_id(), "monitor_stack_a");

    pop_request();
    EXPECT_EQ(current_request_id(), "");

    {
        RequestScope empty_scope("");
        EXPECT_EQ(current_request_id(), "");
    }
    EXPECT_EQ(current_request_id(), "");
}

TEST(MonitorTest, BuildAttrsAndJsonEscapeHandleQuotes) {
    using godot_autopilot::monitor::build_attrs;
    using godot_autopilot::monitor::json_escape;

    const std::string attrs = build_attrs({{"a", "b\"c"}});
    EXPECT_NE(attrs.find("\"a\""), std::string::npos);
    EXPECT_NE(attrs.find("\\\""), std::string::npos);
    EXPECT_EQ(attrs, "{\"a\":\"b\\\"c\"}");

    EXPECT_EQ(json_escape("line1\nline2"), "line1\\nline2");
    EXPECT_EQ(json_escape("q\"q"), "q\\\"q");
    EXPECT_EQ(json_escape("tab\tx"), "tab\\tx");
    EXPECT_EQ(json_escape("back\\slash"), "back\\\\slash");
}

TEST(MonitorTest, SanitizeFieldTruncatesAndStripsTokens) {
    using godot_autopilot::monitor::sanitize_field;

    const std::string long_text(5000, 'x');
    const std::string truncated = sanitize_field(long_text);
    const std::size_t marker_len = std::string("...[truncated]").size();
    EXPECT_LE(truncated.size(), TraceRecorder::kAttrsMaxChars + marker_len + 1u);
    EXPECT_NE(truncated.find("...[truncated]"), std::string::npos);

    const std::string upper = sanitize_field("{\"TOKEN\":\"tokValueSecret\"}");
    EXPECT_EQ(upper.find("tokValueSecret"), std::string::npos);
    EXPECT_NE(upper.find("<stripped"), std::string::npos);

    const std::string mixed = sanitize_field("{\"PassWord\":\"hunterValue\"}");
    EXPECT_EQ(mixed.find("hunterValue"), std::string::npos);
    EXPECT_NE(mixed.find("<stripped"), std::string::npos);

    const std::string kept = sanitize_field("{\"name\":\"keptValue\"}");
    EXPECT_NE(kept.find("keptValue"), std::string::npos);
}
