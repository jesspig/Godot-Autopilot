#include "tools/base/confirmation.hpp"

#include <gtest/gtest.h>

using godot_autopilot::confirmation::ConfirmationDecision;
using godot_autopilot::confirmation::resolve_confirmation;

namespace {

godot_autopilot::util::JsonValue object() { return godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag); }

} // namespace

TEST(ConfirmationResolutionTest, DirectArguments) {
  godot_autopilot::util::JsonValue args = object();
  args["confirm"] = godot_autopilot::util::JsonValue(true);
  const auto r = resolve_confirmation(args, false, nullptr, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kAccepted);
  EXPECT_TRUE(r.from_arguments);
}

TEST(ConfirmationResolutionTest, DeclinedArgument) {
  godot_autopilot::util::JsonValue args = object();
  args["confirm"] = godot_autopilot::util::JsonValue(false);
  const auto r = resolve_confirmation(args, false, nullptr, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kDeclined);
}

TEST(ConfirmationResolutionTest, NestedCallToolArgument) {
  godot_autopilot::util::JsonValue nested = object();
  nested["confirm"] = godot_autopilot::util::JsonValue(true);
  godot_autopilot::util::JsonValue args = object();
  args["arguments"] = std::move(nested);
  const auto r = resolve_confirmation(args, true, nullptr, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kAccepted);
}

TEST(ConfirmationResolutionTest, WrapperTrueWinsOverNestedFalse) {
  godot_autopilot::util::JsonValue nested = object();
  nested["confirm"] = godot_autopilot::util::JsonValue(false);
  godot_autopilot::util::JsonValue args = object();
  args["confirm"] = godot_autopilot::util::JsonValue(true);
  args["arguments"] = std::move(nested);
  const auto r = resolve_confirmation(args, true, nullptr, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kAccepted);
}

TEST(ConfirmationResolutionTest, InputResponseBoolShorthand) {
  godot_autopilot::util::JsonValue args = object();
  godot_autopilot::util::JsonValue response = object();
  response["confirm"] = godot_autopilot::util::JsonValue(true);
  const auto r = resolve_confirmation(args, false, &response, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kAccepted);
  EXPECT_TRUE(r.from_input_responses);
}

TEST(ConfirmationResolutionTest, InputResponseObjectAction) {
  godot_autopilot::util::JsonValue args = object();
  godot_autopilot::util::JsonValue confirm = object();
  confirm["action"] = godot_autopilot::util::JsonValue(std::string("accept"));
  godot_autopilot::util::JsonValue response = object();
  response["confirm"] = std::move(confirm);
  const auto r = resolve_confirmation(args, false, &response, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kAccepted);
}

TEST(ConfirmationResolutionTest, MalformedInputResponse) {
  godot_autopilot::util::JsonValue args = object();
  godot_autopilot::util::JsonValue confirm = object();
  confirm["action"] = godot_autopilot::util::JsonValue(std::string("maybe"));
  godot_autopilot::util::JsonValue response = object();
  response["confirm"] = std::move(confirm);
  std::string error;
  const auto r = resolve_confirmation(args, false, &response, &error);
  EXPECT_EQ(r.decision, ConfirmationDecision::kMalformed);
  EXPECT_FALSE(error.empty());
}

TEST(ConfirmationResolutionTest, MissingConfirmation) {
  godot_autopilot::util::JsonValue args = object();
  const auto r = resolve_confirmation(args, false, nullptr, nullptr);
  EXPECT_EQ(r.decision, ConfirmationDecision::kMissing);
}
