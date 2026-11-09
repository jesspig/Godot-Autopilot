#include "tools/ready_policy.hpp"

#include <gtest/gtest.h>

using godot_autopilot::decide_game_request;
using godot_autopilot::ReadyDecision;

TEST(ReadyPolicyTest, ReadySessionAlwaysSendsNow) {
    EXPECT_EQ(decide_game_request(1, 0, true), ReadyDecision::SendNow);
    EXPECT_EQ(decide_game_request(1, 0, false), ReadyDecision::SendNow);
    EXPECT_EQ(decide_game_request(2, 1, true), ReadyDecision::SendNow);
    EXPECT_EQ(decide_game_request(2, 1, false), ReadyDecision::SendNow);
    EXPECT_EQ(decide_game_request(3, -1, true), ReadyDecision::SendNow);
}

TEST(ReadyPolicyTest, ConnectedSessionQueuesWhenWaiting) {
    EXPECT_EQ(decide_game_request(0, 1, true), ReadyDecision::QueueForReady);
    EXPECT_EQ(decide_game_request(0, 2, true), ReadyDecision::QueueForReady);
    EXPECT_EQ(decide_game_request(0, 3, true), ReadyDecision::QueueForReady);
}

TEST(ReadyPolicyTest, ConnectedSessionFailsWhenNotWaiting) {
    EXPECT_EQ(decide_game_request(0, 1, false), ReadyDecision::FailNotReady);
    EXPECT_EQ(decide_game_request(0, 2, false), ReadyDecision::FailNotReady);
}

TEST(ReadyPolicyTest, NoSessionFailsNotRunning) {
    EXPECT_EQ(decide_game_request(0, 0, true), ReadyDecision::FailNotRunning);
    EXPECT_EQ(decide_game_request(0, 0, false), ReadyDecision::FailNotRunning);
    EXPECT_EQ(decide_game_request(-1, 0, true), ReadyDecision::FailNotRunning);
    EXPECT_EQ(decide_game_request(-1, 0, false), ReadyDecision::FailNotRunning);
}

TEST(ReadyPolicyTest, NegativeCountsTreatedAsZero) {
    EXPECT_EQ(decide_game_request(-5, 1, true), ReadyDecision::QueueForReady);
    EXPECT_EQ(decide_game_request(-5, 1, false), ReadyDecision::FailNotReady);
    EXPECT_EQ(decide_game_request(0, -3, true), ReadyDecision::FailNotRunning);
    EXPECT_EQ(decide_game_request(0, -3, false), ReadyDecision::FailNotRunning);
    EXPECT_EQ(decide_game_request(-1, -1, true), ReadyDecision::FailNotRunning);
    EXPECT_EQ(decide_game_request(-1, 2, true), ReadyDecision::QueueForReady);
}
