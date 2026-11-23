#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

#include "core/observability/trace_recorder.hpp"
#include "tools/base/tool_args.hpp"
#include "tools/base/tool_registry.hpp"
#include "tools/base/tool_spec.hpp"

namespace {

using godot_autopilot::TraceRecorder;

mcp::JsonValue parse(const std::string &text) {
  return mcp::JsonValue::Parse(text);
}

} // namespace

TEST(CodeQualityC5Test, FullMapEvictsOldestInsertionNotArbitrary) {
  namespace detail = godot_autopilot::spec_detail;
  const std::string first_key = "pending:first";
  detail::DeferredTrace record;
  record.event.tool = "first_tool";
  detail::stash_deferred_trace(first_key, record);
  for (size_t i = 0; i < detail::kDeferredTraceCap; ++i) {
    detail::DeferredTrace filler;
    filler.event.tool = "filler_" + std::to_string(i);
    detail::stash_deferred_trace("pending:filler_" + std::to_string(i), filler);
  }
  detail::DeferredTrace taken;
  EXPECT_FALSE(detail::take_deferred_trace(first_key, &taken));
  for (size_t i = 0; i < detail::kDeferredTraceCap; ++i) {
    EXPECT_TRUE(detail::take_deferred_trace("pending:filler_" +
                                                std::to_string(i),
                                            &taken));
  }
}

TEST(CodeQualityC5Test, RestoringKeyDoesNotCreateDuplicateOrder) {
  namespace detail = godot_autopilot::spec_detail;
  const std::string key = "pending:reinsert";
  detail::DeferredTrace record;
  record.event.tool = "v1";
  detail::stash_deferred_trace(key, record);
  record.event.tool = "v2";
  detail::stash_deferred_trace(key, record);
  detail::DeferredTrace taken;
  ASSERT_TRUE(detail::take_deferred_trace(key, &taken));
  EXPECT_EQ(taken.event.tool, "v2");
  EXPECT_FALSE(detail::take_deferred_trace(key, &taken));
}

TEST(CodeQualityC3Test, DeferredTraceIsEmittedWhenResolved) {
  namespace detail = godot_autopilot::spec_detail;
  TraceRecorder &recorder = TraceRecorder::instance();
  recorder.clear_for_test();
  const std::size_t before = recorder.size();

  detail::DeferredTrace record;
  record.event.tool = "deferred_probe";
  record.event.name = "deferred_probe";
  record.event.wall_start_ms = TraceRecorder::wall_now_ms();
  detail::stash_deferred_trace("pending:c3_probe", std::move(record));

  const mcp::JsonValue final_result =
      parse(R"({"result":{"ok":true},"new_errors_since_last_call":0})");
  EXPECT_TRUE(detail::finish_deferred_trace("pending:c3_probe", final_result));
  EXPECT_FALSE(detail::finish_deferred_trace("pending:c3_probe", final_result));

  const auto recent = recorder.query_recent(1000);
  ASSERT_EQ(recent.size(), before + 1);
  EXPECT_EQ(recent.back().name, "deferred_probe");
  EXPECT_TRUE(recent.back().ok);
  recorder.clear_for_test();
}

TEST(CodeQualityC6Test, AddRejectsDuplicateToolName) {
  godot_autopilot::ToolRegistry registry;
  auto make = [](const std::string &name) {
    godot_autopilot::ToolSpec spec;
    spec.name = name;
    spec.description = "desc";
    spec.category = "Test";
    spec.handler = [](const mcp::JsonValue &) {
      return mcp::JsonValue(mcp::JsonValue::object_tag);
    };
    return godot_autopilot::make_spec_tool(std::move(spec));
  };
  registry.add(make("dup_tool"));
  EXPECT_THROW(registry.add(make("dup_tool")), std::invalid_argument);
  EXPECT_EQ(registry.size(), 1u);
}

TEST(CodeQualityC6Test, AddRejectsNameCollidingWithMetaTool) {
  godot_autopilot::ToolRegistry registry;
  auto make = [](const std::string &name, uint32_t flags) {
    godot_autopilot::ToolSpec spec;
    spec.name = name;
    spec.description = "desc";
    spec.category = "Test";
    spec.flags = flags;
    spec.handler = [](const mcp::JsonValue &) {
      return mcp::JsonValue(mcp::JsonValue::object_tag);
    };
    return godot_autopilot::make_spec_tool(std::move(spec));
  };
  registry.add(make("shared_name", godot_autopilot::tool_flags::kMeta));
  EXPECT_THROW(registry.add(make("shared_name", godot_autopilot::tool_flags::kNone)),
               std::invalid_argument);
  EXPECT_EQ(registry.meta_size(), 1u);
  EXPECT_EQ(registry.size(), 0u);
}

namespace {

std::string dynamic_validation_error(
    const mcp::JsonValue &args,
    const std::vector<godot_autopilot::ParamSpec> &params) {
  const mcp::JsonValue result =
      godot_autopilot::spec_detail::validate_dynamic_args(args, params);
  const mcp::JsonValue *error = result.Find("error");
  return error != nullptr && error->IsString() ? error->GetString()
                                               : std::string("<no error>");
}

} // namespace

TEST(CodeQualityC4Test, ValidateDeclaredChecksRequiredAndTypes) {
  const std::vector<godot_autopilot::ParamSpec> params = {
      {"path", "string", "", true},
      {"count", "integer", "", false},
      {"scale", "number", "", false},
      {"flag", "boolean", "", false},
      {"payload", "object", "", false},
      {"items", "array", "", false},
  };

  godot_autopilot::Args missing(parse("{}"), params);
  EXPECT_EQ(dynamic_validation_error(parse("{}"), params),
            "missing required parameter: path");
  EXPECT_THROW(missing.validate_declared(), godot_autopilot::ToolArgError);

  godot_autopilot::Args wrong(parse(R"({"path":3,"count":1.5})"), params);
  EXPECT_THROW(wrong.validate_declared(), godot_autopilot::ToolArgError);

  godot_autopilot::Args valid(
      parse(R"({"path":"res://x","count":2.0,"scale":3,"flag":true,"payload":{},"items":[1]})"),
      params);
  EXPECT_NO_THROW(valid.validate_declared());
}

TEST(CodeQualityC4Test, DynamicToolRuntimeMatchesSchemaDeclaration) {
  const std::vector<godot_autopilot::ParamSpec> params = {
      {"path", "string", "", true},
      {"count", "integer", "", false},
  };

  EXPECT_EQ(dynamic_validation_error(parse("{}"), params),
            "missing required parameter: path");
  EXPECT_EQ(dynamic_validation_error(parse(R"({"path":"res://x","count":"3"})"), params),
            "invalid parameter: count must be a integer");
  EXPECT_EQ(dynamic_validation_error(parse(R"({"path":"res://x"})"), params),
            "<no error>");
  EXPECT_EQ(dynamic_validation_error(parse(R"({"path":"res://x","count":3.0})"), params),
            "<no error>");
}

TEST(CodeQualityC4Test, DynamicToolRejectsUndeclaredParameter) {
  const std::vector<godot_autopilot::ParamSpec> params = {
      {"path", "string", "", true},
  };
  EXPECT_EQ(dynamic_validation_error(
                parse(R"({"path":"res://x","bogus":1})"), params),
            "unknown parameter: bogus");
  EXPECT_EQ(dynamic_validation_error(parse(R"({"path":"res://x"})"), params),
            "<no error>");
}

TEST(CodeQualityC4Test, DynamicToolAllowsConfirmMetaField) {
  const std::vector<godot_autopilot::ParamSpec> params = {
      {"path", "string", "", true},
  };
  EXPECT_EQ(dynamic_validation_error(
                parse(R"({"path":"res://x","confirm":true})"), params),
            "<no error>");
  EXPECT_EQ(dynamic_validation_error(
                parse(R"({"path":"res://x","confirm":true,"bogus":1})"), params),
            "unknown parameter: bogus");
}

TEST(CodeQualityC4Test, DynamicToolWithoutParamsIsUnchanged) {
  EXPECT_EQ(dynamic_validation_error(parse(R"({"arbitrary":{"nested":1}})"), {}),
            "<no error>");
}

TEST(CodeQualityC4Test, NullTreatedAsAbsentForDeclaredParam) {
  const std::vector<godot_autopilot::ParamSpec> params = {
      {"path", "string", "", true},
  };
  EXPECT_EQ(dynamic_validation_error(parse(R"({"path":null})"), params),
            "missing required parameter: path");
}
