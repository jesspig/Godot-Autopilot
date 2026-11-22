#include "tools/domains/os/os_process_timeout.hpp"

#include <gtest/gtest.h>

using godot_autopilot::os_ops_detail::termination_detail;

TEST(OsProcessTimeoutTest, ConfirmedTerminationSaysTerminated) {
  EXPECT_NE(termination_detail(true).find("terminated"), std::string::npos);
  EXPECT_EQ(termination_detail(true).find("not confirmed"), std::string::npos);
}

TEST(OsProcessTimeoutTest, UnconfirmedTerminationDoesNotClaimTerminated) {
  const std::string detail = termination_detail(false);
  EXPECT_NE(detail.find("not confirmed"), std::string::npos);
  EXPECT_EQ(detail.find("it was terminated"), std::string::npos);
}
