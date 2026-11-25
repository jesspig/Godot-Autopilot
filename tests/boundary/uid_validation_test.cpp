#include "tools/domains/resource/resource_tools.hpp"
#include "tools/domains/resource/uid_validation.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

using godot_autopilot::resource_ops_detail::check_uid_reassign;
using godot_autopilot::resource_ops_detail::UidReassignFailure;
using godot_autopilot::resource_ops_detail::UidVerdict;
using godot_autopilot::resource_ops_detail::validate_uid_assignment;

TEST(UidValidationTest, ExplicitNonPositiveRejected) {
  EXPECT_EQ(validate_uid_assignment(true, 0, false, false), UidVerdict::kReject);
  EXPECT_EQ(validate_uid_assignment(true, -1, false, false), UidVerdict::kReject);
}

TEST(UidValidationTest, FreshUidAcceptsAdd) {
  EXPECT_EQ(validate_uid_assignment(true, 42, false, false), UidVerdict::kAcceptAdd);
  EXPECT_EQ(validate_uid_assignment(false, 42, false, false), UidVerdict::kAcceptAdd);
}

TEST(UidValidationTest, ExistingUidRequiresOverwrite) {
  EXPECT_EQ(validate_uid_assignment(true, 42, true, false), UidVerdict::kReject);
  EXPECT_EQ(validate_uid_assignment(true, 42, true, true), UidVerdict::kAcceptSet);
}

TEST(UidValidationTest, ReassignRequiresOverwriteAndConfirmation) {
  EXPECT_EQ(check_uid_reassign(false, false, false, false),
            UidReassignFailure::kNone);
  EXPECT_EQ(check_uid_reassign(false, true, true, false),
            UidReassignFailure::kRequiresOverwrite);
  EXPECT_EQ(check_uid_reassign(true, false, true, false),
            UidReassignFailure::kConfirmationRequired);
  EXPECT_EQ(check_uid_reassign(true, true, true, false),
            UidReassignFailure::kNone);
}

TEST(UidValidationTest, TruncatedReferenceScanAlwaysFails) {
  EXPECT_EQ(check_uid_reassign(true, true, true, true),
            UidReassignFailure::kScanTruncated);
  EXPECT_EQ(check_uid_reassign(false, false, false, true),
            UidReassignFailure::kScanTruncated);
}

TEST(UidValidationTest, SchemaExposesReassignGuards) {
  const auto tools = godot_autopilot::resource_tools::make_tools();
  for (const auto &tool : tools) {
    if (tool->meta().name != "resource_uid_set") {
      continue;
    }
    const godot_autopilot::util::JsonValue schema = tool->input_schema();
    const godot_autopilot::util::JsonValue *properties = schema.Find("properties");
    EXPECT_NE(properties, nullptr);
    ASSERT_NE(properties, nullptr);
    ASSERT_TRUE(properties->IsObject());
    const godot_autopilot::util::JsonValue *overwrite = properties->Find("overwrite");
    const godot_autopilot::util::JsonValue *confirm = properties->Find("confirm");
    const godot_autopilot::util::JsonValue *uid = properties->Find("uid");
    ASSERT_NE(overwrite, nullptr);
    ASSERT_NE(confirm, nullptr);
    ASSERT_NE(uid, nullptr);
    EXPECT_TRUE(overwrite->IsObject());
    EXPECT_TRUE(confirm->IsObject());
    EXPECT_EQ(uid->Find("type")->GetString(), "integer");
    return;
  }
  FAIL() << "resource_uid_set tool not found";
}
