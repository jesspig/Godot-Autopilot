#include "tools/domains/resource/uid_validation.hpp"

#include <gtest/gtest.h>

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
