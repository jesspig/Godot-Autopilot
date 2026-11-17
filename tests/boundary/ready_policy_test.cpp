#include "tools/base/ready_policy.hpp"

#include <gtest/gtest.h>

using godot_autopilot::decide_game_request;
using godot_autopilot::is_broadcast_eligible;
using godot_autopilot::ReadyDecision;
using godot_autopilot::should_queue_for_ready;

TEST(ReadyPolicyTest, SceneReadySessionSendsNow) {
    EXPECT_EQ(decide_game_request(1, 1, 0, true), ReadyDecision::SendNow);
    EXPECT_EQ(decide_game_request(1, 1, 0, false), ReadyDecision::SendNow);
    EXPECT_EQ(decide_game_request(2, 1, 1, true), ReadyDecision::SendNow);
    EXPECT_EQ(decide_game_request(2, 2, 1, false), ReadyDecision::SendNow);
    EXPECT_EQ(decide_game_request(3, 3, -1, true), ReadyDecision::SendNow);
}

TEST(ReadyPolicyTest, ReadyWithoutSceneWaitsForScene) {
    EXPECT_EQ(decide_game_request(1, 0, 1, true), ReadyDecision::QueueForReady);
    EXPECT_EQ(decide_game_request(1, 0, 2, true), ReadyDecision::QueueForReady);
    EXPECT_EQ(decide_game_request(2, 0, 3, true), ReadyDecision::QueueForReady);
    EXPECT_EQ(decide_game_request(1, 0, 1, false), ReadyDecision::FailNotReady);
    EXPECT_EQ(decide_game_request(1, 0, 2, false), ReadyDecision::FailNotReady);
    EXPECT_EQ(decide_game_request(2, 0, 3, false), ReadyDecision::FailNotReady);
}

TEST(ReadyPolicyTest, ConnectedSessionQueuesWhenWaiting) {
    EXPECT_EQ(decide_game_request(0, 0, 1, true), ReadyDecision::QueueForReady);
    EXPECT_EQ(decide_game_request(0, 0, 2, true), ReadyDecision::QueueForReady);
    EXPECT_EQ(decide_game_request(0, 0, 3, true), ReadyDecision::QueueForReady);
}

TEST(ReadyPolicyTest, ConnectedSessionFailsWhenNotWaiting) {
    EXPECT_EQ(decide_game_request(0, 0, 1, false), ReadyDecision::FailNotReady);
    EXPECT_EQ(decide_game_request(0, 0, 2, false), ReadyDecision::FailNotReady);
}

TEST(ReadyPolicyTest, NoSessionFailsNotRunning) {
    EXPECT_EQ(decide_game_request(0, 0, 0, true), ReadyDecision::FailNotRunning);
    EXPECT_EQ(decide_game_request(0, 0, 0, false), ReadyDecision::FailNotRunning);
    EXPECT_EQ(decide_game_request(1, 1, 0, true), ReadyDecision::SendNow);
    EXPECT_EQ(decide_game_request(0, 0, -1, true), ReadyDecision::FailNotRunning);
}

TEST(ReadyPolicyTest, NegativeCountsTreatedAsZero) {
    EXPECT_EQ(decide_game_request(-5, -5, 1, true), ReadyDecision::QueueForReady);
    EXPECT_EQ(decide_game_request(-5, -5, 1, false), ReadyDecision::FailNotReady);
    EXPECT_EQ(decide_game_request(0, -3, 0, true), ReadyDecision::FailNotRunning);
    EXPECT_EQ(decide_game_request(-1, 2, 0, true), ReadyDecision::FailNotRunning);
    EXPECT_EQ(decide_game_request(1, -1, 2, true), ReadyDecision::QueueForReady);
    EXPECT_EQ(decide_game_request(1, 1, 2, true), ReadyDecision::SendNow);
}

TEST(ReadyPolicyTest, ReadyWithoutSceneReadyQueuesInsteadOfSending) {
    const ReadyDecision decision = decide_game_request(1, 0, 1, true);
    EXPECT_EQ(decision, ReadyDecision::QueueForReady);
    EXPECT_TRUE(should_queue_for_ready(decision, true, false));
}

TEST(ReadyPolicyTest, SceneReadySendsNowWithoutQueueing) {
    const ReadyDecision decision = decide_game_request(1, 1, 1, true);
    EXPECT_EQ(decision, ReadyDecision::SendNow);
    EXPECT_FALSE(should_queue_for_ready(decision, true, false));
    EXPECT_FALSE(should_queue_for_ready(decision, false, false));
}

TEST(ReadyPolicyTest, NotWaitingNeverQueues) {
    EXPECT_FALSE(should_queue_for_ready(
        decide_game_request(1, 0, 1, false), false, false));
    EXPECT_FALSE(should_queue_for_ready(
        decide_game_request(0, 0, 0, false), false, true));
}

TEST(ReadyPolicyTest, StartingGameWithoutSessionQueuesOnlyWhenPlaying) {
    const ReadyDecision decision = decide_game_request(0, 0, 0, true);
    EXPECT_EQ(decision, ReadyDecision::FailNotRunning);
    EXPECT_FALSE(should_queue_for_ready(decision, true, false));
    EXPECT_TRUE(should_queue_for_ready(decision, true, true));
    EXPECT_FALSE(should_queue_for_ready(decision, false, true));
}

TEST(ReadyPolicyTest, BroadcastEligibilityRequiresSceneReady) {
    EXPECT_TRUE(is_broadcast_eligible(true, true));
    EXPECT_FALSE(is_broadcast_eligible(true, false));
    EXPECT_FALSE(is_broadcast_eligible(false, true));
    EXPECT_FALSE(is_broadcast_eligible(false, false));
}

TEST(ReadyPolicyTest, BroadcastFilterMatchesSendNowCriterion) {
    for (int ready = 0; ready <= 1; ++ready) {
        for (int scene = 0; scene <= 1; ++scene) {
            const bool eligible = is_broadcast_eligible(ready > 0, scene > 0);
            const ReadyDecision decision = decide_game_request(ready, scene, 1, true);
            EXPECT_EQ(eligible, decision == ReadyDecision::SendNow)
                << "ready=" << ready << " scene=" << scene;
        }
    }
}