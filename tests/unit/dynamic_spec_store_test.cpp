#include "tools/base/dynamic_spec_store.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <thread>
#include <vector>

using godot_autopilot::dynamic_specs::NodeOwnerRegistry;

namespace {

class DynamicEnabledGuard {
public:
  DynamicEnabledGuard() : previous_(godot_autopilot::dynamic_specs::enabled().load()) {}
  ~DynamicEnabledGuard() {
    godot_autopilot::dynamic_specs::enabled().store(previous_);
  }

private:
  bool previous_;
};

} // namespace

TEST(DynamicSpecStoreTest, NodeOwnerSurvivesUntilLastRegistrationReleasesIt) {
  NodeOwnerRegistry owners;
  constexpr uint64_t kNodeId = 41;

  owners.retain(kNodeId);
  owners.retain(kNodeId);

  EXPECT_TRUE(owners.owns(kNodeId));
  EXPECT_FALSE(owners.release(kNodeId));
  EXPECT_TRUE(owners.owns(kNodeId));
  EXPECT_TRUE(owners.release(kNodeId));
  EXPECT_FALSE(owners.owns(kNodeId));
  EXPECT_FALSE(owners.release(kNodeId));
}

TEST(DynamicSpecStoreTest, ReleaseAllReturnsEveryManagedNodeOnce) {
  NodeOwnerRegistry owners;
  owners.retain(7);
  owners.retain(8);
  owners.retain(7);

  const std::vector<uint64_t> released = owners.release_all();
  EXPECT_EQ(released.size(), 2u);
  EXPECT_NE(std::find(released.begin(), released.end(), 7u), released.end());
  EXPECT_NE(std::find(released.begin(), released.end(), 8u), released.end());
  EXPECT_EQ(owners.size(), 0u);
  EXPECT_TRUE(owners.release_all().empty());
}

TEST(DynamicSpecStoreTest, ConcurrentRetainsKeepNodeOwnedUntilAllReleases) {
  NodeOwnerRegistry owners;
  constexpr uint64_t kNodeId = 99;
  constexpr int kThreads = 8;
  std::vector<std::thread> threads;
  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([&owners] { owners.retain(kNodeId); });
  }
  for (auto &thread : threads) {
    thread.join();
  }

  for (int i = 0; i < kThreads - 1; ++i) {
    EXPECT_FALSE(owners.release(kNodeId));
  }
  EXPECT_TRUE(owners.release(kNodeId));
}

TEST(DynamicSpecStoreTest, EnabledStateIsAtomicAndRestorable) {
  DynamicEnabledGuard guard;
  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back([i] {
      for (int j = 0; j < 100; ++j) {
        godot_autopilot::dynamic_specs::enabled().store((i + j) % 2 == 0);
        (void)godot_autopilot::dynamic_specs::enabled().load();
      }
    });
  }
  for (auto &thread : threads) {
    thread.join();
  }
  godot_autopilot::dynamic_specs::enabled().store(false);
  EXPECT_FALSE(godot_autopilot::dynamic_specs::enabled().load());
  godot_autopilot::dynamic_specs::enabled().store(true);
  EXPECT_TRUE(godot_autopilot::dynamic_specs::enabled().load());
}
