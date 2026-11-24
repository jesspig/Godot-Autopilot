#include "tools/base/confirmation.hpp"

#include <gtest/gtest.h>

using godot_autopilot::confirmation::ConfirmationDecision;
using godot_autopilot::confirmation::resolve_confirmation;

namespace {

mcp::JsonValue object() { return mcp::JsonValue(mcp::JsonValue::object_tag); }

} // namespace

TEST(ConfirmationResolutionTest, DirectArguments) {
  mcp::JsonValue args = object();
  args["confirm"] = mcp::JsonValue(true);
  const auto r = resolve_confirmation(args, false, nullptr, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kAccepted);
  EXPECT_TRUE(r.from_arguments);
}

TEST(ConfirmationResolutionTest, DeclinedArgument) {
  mcp::JsonValue args = object();
  args["confirm"] = mcp::JsonValue(false);
  const auto r = resolve_confirmation(args, false, nullptr, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kDeclined);
}

TEST(ConfirmationResolutionTest, NestedCallToolArgument) {
  mcp::JsonValue nested = object();
  nested["confirm"] = mcp::JsonValue(true);
  mcp::JsonValue args = object();
  args["arguments"] = std::move(nested);
  const auto r = resolve_confirmation(args, true, nullptr, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kAccepted);
}

TEST(ConfirmationResolutionTest, WrapperTrueWinsOverNestedFalse) {
  mcp::JsonValue nested = object();
  nested["confirm"] = mcp::JsonValue(false);
  mcp::JsonValue args = object();
  args["confirm"] = mcp::JsonValue(true);
  args["arguments"] = std::move(nested);
  const auto r = resolve_confirmation(args, true, nullptr, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kAccepted);
}

TEST(ConfirmationResolutionTest, InputResponseBoolShorthand) {
  mcp::JsonValue args = object();
  mcp::JsonValue response = object();
  response["confirm"] = mcp::JsonValue(true);
  const auto r = resolve_confirmation(args, false, &response, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kAccepted);
  EXPECT_TRUE(r.from_input_responses);
}

TEST(ConfirmationResolutionTest, InputResponseObjectAction) {
  mcp::JsonValue args = object();
  mcp::JsonValue confirm = object();
  confirm["action"] = mcp::JsonValue(std::string("accept"));
  mcp::JsonValue response = object();
  response["confirm"] = std::move(confirm);
  const auto r = resolve_confirmation(args, false, &response, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kAccepted);
}

TEST(ConfirmationResolutionTest, MalformedInputResponse) {
  mcp::JsonValue args = object();
  mcp::JsonValue confirm = object();
  confirm["action"] = mcp::JsonValue(std::string("maybe"));
  mcp::JsonValue response = object();
  response["confirm"] = std::move(confirm);
  std::string error;
  const auto r = resolve_confirmation(args, false, &response, &error);
  EXPECT_EQ(r.decision, ConfirmationDecision::kMalformed);
  EXPECT_FALSE(error.empty());
}

TEST(ConfirmationResolutionTest, MissingConfirmation) {
  mcp::JsonValue args = object();
  const auto r = resolve_confirmation(args, false, nullptr, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kMissing);
}
