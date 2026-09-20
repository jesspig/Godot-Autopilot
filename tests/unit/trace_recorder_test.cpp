#include "core/trace_recorder.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using godot_autopilot::SanitizeResult;
using godot_autopilot::TraceEvent;
using godot_autopilot::TraceRecorder;

TEST(TraceRecorderTest, NewIdsAreUniqueAndPrefixed) {
    const std::string trace_a = TraceRecorder::new_trace_id();
    const std::string trace_b = TraceRecorder::new_trace_id();
    const std::string span_a = TraceRecorder::new_span_id();
    const std::string span_b = TraceRecorder::new_span_id();
    EXPECT_NE(trace_a, trace_b);
    EXPECT_NE(span_a, span_b);
    EXPECT_EQ(trace_a.rfind("tr_", 0), 0u);
    EXPECT_EQ(span_a.rfind("sp_", 0), 0u);
}

TEST(TraceRecorderTest, RecordAssignsStrictlyIncreasingSeq) {
    TraceRecorder& recorder = TraceRecorder::instance();
    recorder.clear_for_test();
    const uint64_t first = recorder.record(TraceEvent{});
    const uint64_t second = recorder.record(TraceEvent{});
    const uint64_t third = recorder.record(TraceEvent{});
    EXPECT_EQ(first, 1u);
    EXPECT_EQ(second, 2u);
    EXPECT_EQ(third, 3u);
    EXPECT_LT(first, second);
    EXPECT_LT(second, third);
    EXPECT_EQ(recorder.next_seq(), 4u);
    EXPECT_EQ(recorder.size(), 3u);
}

TEST(TraceRecorderTest, QueryRecentReturnsTail) {
    TraceRecorder& recorder = TraceRecorder::instance();
    recorder.clear_for_test();
    TraceEvent event_a;
    event_a.tool = "tool_a";
    TraceEvent event_b;
    event_b.tool = "tool_b";
    TraceEvent event_c;
    event_c.tool = "tool_c";
    recorder.record(event_a);
    recorder.record(event_b);
    recorder.record(event_c);
    const std::vector<TraceEvent> last = recorder.query_recent(1);
    ASSERT_EQ(last.size(), 1u);
    EXPECT_EQ(last[0].tool, "tool_c");
    EXPECT_EQ(last[0].seq, 3u);
    EXPECT_TRUE(recorder.query_recent(0).empty());
    EXPECT_EQ(recorder.query_recent(10).size(), 3u);
}

TEST(TraceRecorderTest, QueryByTraceIdFilters) {
    TraceRecorder& recorder = TraceRecorder::instance();
    recorder.clear_for_test();
    TraceEvent first;
    first.trace_id = "tr_a";
    first.tool = "first";
    TraceEvent second;
    second.trace_id = "tr_a";
    second.tool = "second";
    TraceEvent other;
    other.trace_id = "tr_b";
    other.tool = "other";
    recorder.record(first);
    recorder.record(second);
    recorder.record(other);
    const std::vector<TraceEvent> a = recorder.query_by_trace("tr_a");
    ASSERT_EQ(a.size(), 2u);
    EXPECT_EQ(a[0].tool, "first");
    EXPECT_EQ(a[1].tool, "second");
    const std::vector<TraceEvent> b = recorder.query_by_trace("tr_b");
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(b[0].tool, "other");
    EXPECT_TRUE(recorder.query_by_trace("tr_missing").empty());
}

TEST(TraceRecorderTest, QuerySinceBoundary) {
    TraceRecorder& recorder = TraceRecorder::instance();
    recorder.clear_for_test();
    recorder.record(TraceEvent{});
    recorder.record(TraceEvent{});
    recorder.record(TraceEvent{});
    uint64_t next_seq = 0;
    const std::vector<TraceEvent> all = recorder.query_since(0, &next_seq);
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all.front().seq, 1u);
    EXPECT_EQ(all.back().seq, 3u);
    EXPECT_EQ(next_seq, 4u);
    const std::vector<TraceEvent> none = recorder.query_since(next_seq, &next_seq);
    EXPECT_TRUE(none.empty());
    EXPECT_EQ(next_seq, 4u);
    const std::vector<TraceEvent> tail = recorder.query_since(2, &next_seq);
    ASSERT_EQ(tail.size(), 2u);
    EXPECT_EQ(tail[0].seq, 2u);
    EXPECT_EQ(tail[1].seq, 3u);
}

TEST(TraceRecorderTest, ClearForTestResetsSeqAndStore) {
    TraceRecorder& recorder = TraceRecorder::instance();
    recorder.clear_for_test();
    recorder.record(TraceEvent{});
    recorder.record(TraceEvent{});
    EXPECT_EQ(recorder.size(), 2u);
    recorder.clear_for_test();
    EXPECT_EQ(recorder.size(), 0u);
    EXPECT_EQ(recorder.next_seq(), 1u);
    EXPECT_TRUE(recorder.query_recent(5).empty());
    EXPECT_EQ(recorder.record(TraceEvent{}), 1u);
}

TEST(TraceRecorderTest, SanitizeArgsStripsSensitiveFieldValues) {
    std::string payload = "{\"data\":\"";
    payload += std::string(120, 'A');
    payload += "\",\"base64\":\"";
    payload += std::string(80, 'B');
    payload += "\",\"name\":\"probe\"}";
    const SanitizeResult sensitive = TraceRecorder::sanitize_args(payload, true);
    EXPECT_FALSE(sensitive.truncated);
    EXPECT_EQ(sensitive.text.find(std::string(120, 'A')), std::string::npos);
    EXPECT_EQ(sensitive.text.find(std::string(80, 'B')), std::string::npos);
    EXPECT_NE(sensitive.text.find("<stripped len=120>"), std::string::npos);
    EXPECT_NE(sensitive.text.find("<stripped len=80>"), std::string::npos);
    EXPECT_NE(sensitive.text.find("\"name\":\"probe\""), std::string::npos);
    const SanitizeResult raw = TraceRecorder::sanitize_args(payload, false);
    EXPECT_EQ(raw.text, payload);
    EXPECT_FALSE(raw.truncated);
}

TEST(TraceRecorderTest, SanitizeArgsTruncationFollowsDesensitizeFlag) {
    const std::string long_text(5000, 'x');
    const SanitizeResult sensitive = TraceRecorder::sanitize_args(long_text, true);
    EXPECT_TRUE(sensitive.truncated);
    EXPECT_EQ(sensitive.text.size(), TraceRecorder::kDesensitizedMaxChars);
    EXPECT_EQ(sensitive.text.size(), 4000u);
    const SanitizeResult raw = TraceRecorder::sanitize_args(long_text, false);
    EXPECT_FALSE(raw.truncated);
    EXPECT_EQ(raw.text, long_text);
    EXPECT_EQ(raw.text.size(), 5000u);
}

TEST(TraceRecorderTest, Fnv1aHexIsStableAndDistinct) {
    EXPECT_EQ(TraceRecorder::fnv1a_hex(""), "cbf29ce484222325");
    EXPECT_EQ(TraceRecorder::fnv1a_hex("hello"), "a430d84680aabd0b");
    EXPECT_EQ(TraceRecorder::fnv1a_hex("payload"), TraceRecorder::fnv1a_hex("payload"));
    EXPECT_NE(TraceRecorder::fnv1a_hex("payload"), TraceRecorder::fnv1a_hex("payloaX"));
    EXPECT_EQ(TraceRecorder::fnv1a_hex("payload").size(), 16u);
}

TEST(TraceRecorderTest, DecodeBase64HandlesPaddingNoiseAndPaddinglessInput) {
    EXPECT_EQ(TraceRecorder::decode_base64("aGVsbG8="), "hello");
    EXPECT_EQ(TraceRecorder::decode_base64(""), "");
    EXPECT_EQ(TraceRecorder::decode_base64("aGVsbG8"), "hello");
    EXPECT_EQ(TraceRecorder::decode_base64("aGVs\nbG8!"), "hello");
    EXPECT_EQ(TraceRecorder::decode_base64("aGVsbG8=\n"), "hello");
}

TEST(TraceRecorderTest, ToJsonLineEscapesFieldsAndCarriesCoreKeys) {
    TraceEvent event;
    event.seq = 7;
    event.tool = "unit_tool";
    event.args_digest = "line1\nline2 \"q\"";
    const std::string line = TraceRecorder::to_json_line(event);
    ASSERT_FALSE(line.empty());
    EXPECT_EQ(line.front(), '{');
    EXPECT_EQ(line.back(), '}');
    EXPECT_EQ(line.find('\n'), std::string::npos);
    EXPECT_NE(line.find("\"seq\":7"), std::string::npos);
    EXPECT_NE(line.find("\"tool\":\"unit_tool\""), std::string::npos);
    EXPECT_NE(line.find("line1\\nline2 \\\"q\\\""), std::string::npos);
}
