
#include "tools/code_exec_ops.hpp"
#include "tools/dispatch.hpp"
#include "tools/runtime_ops.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::code_exec_ops::handle_batch_execute;

namespace {

std::string status_of(const mcp::JsonValue &item) {
  const mcp::JsonValue *st = item.Find("status");
  if (st == nullptr || !st->IsString())
    return "<no status>";
  return st->GetString();
}

std::string error_of(const mcp::JsonValue &item) {
  const mcp::JsonValue *err = item.Find("error");
  if (err == nullptr || !err->IsString())
    return "<no error>";
  return err->GetString();
}

void register_basic_handlers(mcp::JsonValue *echo_seen, bool *echo_called) {
  godot_autopilot::dispatch::replace_handlers(
      {{"const_v1",
        [](const mcp::JsonValue &) {
          return mcp::JsonValue::Parse(R"({"v":1})");
        }},
       {"fail",
        [](const mcp::JsonValue &) {
          return mcp::JsonValue::Parse(R"({"error":"boom"})");
        }},
       {"echo",
        [echo_seen, echo_called](const mcp::JsonValue &args) {
          if (echo_called)
            *echo_called = true;
          if (echo_seen)
            *echo_seen = args;
          return mcp::JsonValue::Parse(R"({"seen":true})");
        }}},
      {});
}

} // namespace

class BatchRefsTest : public ::testing::Test {
protected:
  void SetUp() override {
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
    godot_autopilot::dispatch::replace_handlers({}, {});
  }

  void TearDown() override {
    godot_autopilot::dispatch::clear_handlers();
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
  }
};

TEST_F(BatchRefsTest, PrevChain) {
  mcp::JsonValue seen_args;
  bool echo_called = false;
  register_basic_handlers(&seen_args, &echo_called);

  const mcp::JsonValue result = handle_batch_execute(
      mcp::JsonValue::Parse(R"({
        "operations": [
          {"tool": "const_v1"},
          {"tool": "echo", "args": {"x": "$prev"}}
        ],
        "stop_on_error": false
      })"));

  ASSERT_TRUE(echo_called);
  ASSERT_TRUE(seen_args.IsObject());
  const mcp::JsonValue *x = seen_args.Find("x");
  ASSERT_NE(x, nullptr);
  ASSERT_TRUE(x->IsObject());
  const mcp::JsonValue *v = x->Find("v");
  ASSERT_NE(v, nullptr);
  ASSERT_TRUE(v->IsInt());
  EXPECT_EQ(v->GetInt(), 1);

  const mcp::JsonValue *results = result.Find("results");
  ASSERT_NE(results, nullptr);
  ASSERT_TRUE(results->IsArray());
  ASSERT_EQ(results->Size(), 2u);
  EXPECT_EQ(status_of((*results)[0]), "ok");
  EXPECT_EQ(status_of((*results)[1]), "ok");
}

TEST_F(BatchRefsTest, StepsRef) {
  mcp::JsonValue seen_args;
  bool echo_called = false;
  register_basic_handlers(&seen_args, &echo_called);

  const mcp::JsonValue result = handle_batch_execute(
      mcp::JsonValue::Parse(R"({
        "operations": [
          {"tool": "const_v1"},
          {"tool": "echo", "args": {
            "nested": {"list": ["$steps[0].result"]},
            "literal": "prefix $prev suffix"
          }}
        ],
        "stop_on_error": false
      })"));

  ASSERT_TRUE(echo_called);
  const mcp::JsonValue *nested = seen_args.Find("nested");
  ASSERT_NE(nested, nullptr);
  ASSERT_TRUE(nested->IsObject());
  const mcp::JsonValue *list = nested->Find("list");
  ASSERT_NE(list, nullptr);
  ASSERT_TRUE(list->IsArray());
  ASSERT_EQ(list->Size(), 1u);
  ASSERT_TRUE((*list)[0].IsObject());
  const mcp::JsonValue *inner_v = (*list)[0].Find("v");
  ASSERT_NE(inner_v, nullptr);
  EXPECT_EQ(inner_v->GetInt(), 1);
  const mcp::JsonValue *literal = seen_args.Find("literal");
  ASSERT_NE(literal, nullptr);
  EXPECT_EQ(literal->GetString(), "prefix $prev suffix");

  const mcp::JsonValue *results = result.Find("results");
  ASSERT_NE(results, nullptr);
  ASSERT_EQ(results->Size(), 2u);
  EXPECT_EQ(status_of((*results)[1]), "ok");
}

TEST_F(BatchRefsTest, ForwardRefError) {
  mcp::JsonValue seen_args;
  bool echo_called = false;
  register_basic_handlers(&seen_args, &echo_called);

  const mcp::JsonValue result = handle_batch_execute(
      mcp::JsonValue::Parse(R"({
        "operations": [
          {"tool": "echo", "args": {"x": "$steps[1].result"}},
          {"tool": "const_v1"}
        ],
        "stop_on_error": false
      })"));

  const mcp::JsonValue *results = result.Find("results");
  ASSERT_NE(results, nullptr);
  ASSERT_EQ(results->Size(), 2u);
  EXPECT_EQ(status_of((*results)[0]), "error");
  const std::string err = error_of((*results)[0]);
  EXPECT_NE(err.find("unresolvable reference"), std::string::npos);
  EXPECT_NE(err.find("$steps[1].result"), std::string::npos);
  EXPECT_NE(err.find("forward reference"), std::string::npos);
  EXPECT_EQ(status_of((*results)[1]), "ok");
}

TEST_F(BatchRefsTest, PrevOnFirstOpError) {
  register_basic_handlers(nullptr, nullptr);

  const mcp::JsonValue result = handle_batch_execute(
      mcp::JsonValue::Parse(R"({
        "operations": [
          {"tool": "const_v1", "args": {"x": "$prev"}}
        ],
        "stop_on_error": false
      })"));

  const mcp::JsonValue *results = result.Find("results");
  ASSERT_NE(results, nullptr);
  ASSERT_EQ(results->Size(), 1u);
  EXPECT_EQ(status_of((*results)[0]), "error");
  const std::string err = error_of((*results)[0]);
  EXPECT_NE(err.find("unresolvable reference"), std::string::npos);
  EXPECT_NE(err.find("$prev"), std::string::npos);
  EXPECT_NE(err.find("no previous operation"), std::string::npos);
}

TEST_F(BatchRefsTest, WrongStatusError) {
  register_basic_handlers(nullptr, nullptr);

  const mcp::JsonValue error_as_result = handle_batch_execute(
      mcp::JsonValue::Parse(R"({
        "operations": [
          {"tool": "const_v1"},
          {"tool": "const_v1", "args": {"x": "$steps[0].error"}}
        ],
        "stop_on_error": false
      })"));
  const mcp::JsonValue *results_a = error_as_result.Find("results");
  ASSERT_NE(results_a, nullptr);
  ASSERT_EQ(results_a->Size(), 2u);
  EXPECT_EQ(status_of((*results_a)[0]), "ok");
  EXPECT_EQ(status_of((*results_a)[1]), "error");
  const std::string err_a = error_of((*results_a)[1]);
  EXPECT_NE(err_a.find("unresolvable reference"), std::string::npos);
  EXPECT_NE(err_a.find("wrong status"), std::string::npos);

  const mcp::JsonValue result_as_error = handle_batch_execute(
      mcp::JsonValue::Parse(R"({
        "operations": [
          {"tool": "fail"},
          {"tool": "const_v1", "args": {"x": "$steps[0].result"}}
        ],
        "stop_on_error": false
      })"));
  const mcp::JsonValue *results_b = result_as_error.Find("results");
  ASSERT_NE(results_b, nullptr);
  ASSERT_EQ(results_b->Size(), 2u);
  EXPECT_EQ(status_of((*results_b)[0]), "error");
  EXPECT_EQ(status_of((*results_b)[1]), "error");
  const std::string err_b = error_of((*results_b)[1]);
  EXPECT_NE(err_b.find("unresolvable reference"), std::string::npos);
  EXPECT_NE(err_b.find("wrong status"), std::string::npos);
}

TEST_F(BatchRefsTest, StopOnErrorRespected) {
  mcp::JsonValue seen_args;
  bool echo_called = false;
  register_basic_handlers(&seen_args, &echo_called);

  const mcp::JsonValue result = handle_batch_execute(
      mcp::JsonValue::Parse(R"({
        "operations": [
          {"tool": "const_v1"},
          {"tool": "const_v1", "args": {"x": "$steps[5].result"}},
          {"tool": "echo", "args": {"x": 1}}
        ],
        "stop_on_error": true
      })"));

  EXPECT_FALSE(echo_called);
  const mcp::JsonValue *results = result.Find("results");
  ASSERT_NE(results, nullptr);
  ASSERT_EQ(results->Size(), 2u);
  EXPECT_EQ(status_of((*results)[0]), "ok");
  EXPECT_EQ(status_of((*results)[1]), "error");
  EXPECT_NE(error_of((*results)[1]).find("unresolvable reference"),
            std::string::npos);
  EXPECT_NE(error_of((*results)[1]).find("index out of range"),
            std::string::npos);

  const mcp::JsonValue *total = result.Find("total");
  ASSERT_NE(total, nullptr);
  EXPECT_EQ(total->GetInt(), 2);
  const mcp::JsonValue *succeeded = result.Find("succeeded");
  const mcp::JsonValue *failed = result.Find("failed");
  const mcp::JsonValue *skipped = result.Find("skipped");
  ASSERT_NE(succeeded, nullptr);
  ASSERT_NE(failed, nullptr);
  ASSERT_NE(skipped, nullptr);
  EXPECT_EQ(succeeded->GetInt(), 1);
  EXPECT_EQ(failed->GetInt(), 1);
  EXPECT_EQ(skipped->GetInt(), 1);
  const mcp::JsonValue *note = result.Find("note");
  ASSERT_NE(note, nullptr);
  EXPECT_NE(note->GetString().find("stopped at operation 2/3"),
            std::string::npos);
}
