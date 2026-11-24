#include "core/command_queue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <thread>

using godot_autopilot::CommandQueue;

namespace {

class QueueDrainer {
 public:
  explicit QueueDrainer(CommandQueue &queue) : queue_(queue) {
    worker_ = std::thread([this] {
      while (!stop_.load(std::memory_order_acquire)) {
        queue_.drain();
        std::this_thread::sleep_for(kPollInterval);
      }
    });
  }

  ~QueueDrainer() {
    stop_.store(true, std::memory_order_release);
    worker_.join();
  }

  QueueDrainer(const QueueDrainer &) = delete;
  QueueDrainer &operator=(const QueueDrainer &) = delete;

 private:
  static constexpr std::chrono::milliseconds kPollInterval{1};
  CommandQueue &queue_;
  std::atomic<bool> stop_{false};
  std::thread worker_;
};

} // namespace

TEST(CommandQueueTest, SubmitDrainReturnsValuesInOrder) {
    CommandQueue q;
    auto f1 = q.submit([] { return 1; });
    auto f2 = q.submit([] { return 2; });
    auto f3 = q.submit([] { return 3; });
    q.drain();
    EXPECT_EQ(f1.get(), 1);
    EXPECT_EQ(f2.get(), 2);
    EXPECT_EQ(f3.get(), 3);
}

TEST(CommandQueueTest, VoidTaskRunsAndCompletes) {
    CommandQueue q;
    bool ran = false;
    auto f = q.submit([&ran] { ran = true; });
    q.drain();
    f.get();
    EXPECT_TRUE(ran);
}

TEST(CommandQueueTest, ExceptionPropagatesThroughFuture) {
    CommandQueue q;
    auto f = q.submit([]() -> int { throw std::runtime_error("boom"); });
    q.drain();
    EXPECT_THROW(f.get(), std::runtime_error);
}

TEST(CommandQueueTest, CrossThreadSubmitDrainedOnMainThread) {
    CommandQueue q;
    std::future<int> f1;
    std::future<int> f2;
    std::thread worker([&] {
        f1 = q.submit([] { return 10; });
        f2 = q.submit([] { return 20; });
    });
    worker.join();
    q.drain();
    EXPECT_EQ(f1.get(), 10);
    EXPECT_EQ(f2.get(), 20);
}

TEST(CommandQueueTest, TasksExecuteOnDrainingThread) {
    CommandQueue q;
    std::thread::id executed;
    auto f = q.submit([&executed] { executed = std::this_thread::get_id(); });
    std::thread drainer([&] { q.drain(); });
    const auto drainer_id = drainer.get_id();
    drainer.join();
    f.get();
    EXPECT_EQ(executed, drainer_id);
    EXPECT_NE(executed, std::this_thread::get_id());
}

TEST(CommandQueueTest, IsMainThreadFlipsAfterDrain) {
    CommandQueue q;
    EXPECT_FALSE(q.is_main_thread());
    q.drain();
    EXPECT_TRUE(q.is_main_thread());
    std::thread other([&] { EXPECT_FALSE(q.is_main_thread()); });
    other.join();
}

TEST(CommandQueueTest, EmptyDrainIsHarmless) {
    CommandQueue q;
    q.drain();
    q.drain();
    EXPECT_TRUE(q.is_main_thread());
}

TEST(CommandQueueTest, CancelRemovesQueuedTaskAndRejectsFuture) {
    CommandQueue q;
    bool ran = false;
    auto tracked = q.submit_tracked([&ran] { ran = true; });
    EXPECT_TRUE(q.cancel(tracked.id));
    q.drain();
    EXPECT_FALSE(ran);
    EXPECT_THROW(tracked.future.get(), std::runtime_error);
    EXPECT_EQ(q.stats().cancelled, 1u);
}

TEST(CommandQueueTest, CancelExecutedTaskReturnsFalse) {
    CommandQueue q;
    bool ran = false;
    auto tracked = q.submit_tracked([&ran] { ran = true; });
    q.drain();
    EXPECT_TRUE(ran);
    EXPECT_FALSE(q.cancel(tracked.id));
    tracked.future.get();
}

TEST(CommandQueueTest, CancelUnknownIdReturnsFalse) {
    CommandQueue q;
    EXPECT_FALSE(q.cancel(12345));
    EXPECT_FALSE(q.cancel(0));
}

TEST(CommandQueueTest, LastDrainAgeAdvancesAfterDrain) {
    CommandQueue q;
    EXPECT_EQ(q.last_drain_age_ms(), -1);
    q.drain();
    EXPECT_GE(q.last_drain_age_ms(), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_GE(q.last_drain_age_ms(), 1);
}

TEST(CommandQueueTest, BoundedSubmitCompletesWithinBudget) {
    constexpr int64_t kBudgetMs = 1000;
    CommandQueue q;
    QueueDrainer drainer(q);
    std::thread::id executed_on;
    auto submission = q.submit_tracked_bounded(
        [&executed_on] {
          executed_on = std::this_thread::get_id();
          return 42;
        },
        kBudgetMs);
    EXPECT_FALSE(submission.timed_out);
    EXPECT_FALSE(submission.cancelled);
    EXPECT_GE(submission.waited_ms, 0);
    EXPECT_LE(submission.waited_ms, kBudgetMs);
    EXPECT_EQ(submission.future.get(), 42);
    EXPECT_NE(executed_on, std::thread::id());
    EXPECT_NE(executed_on, std::this_thread::get_id());
}

TEST(CommandQueueTest, BoundedSubmitOwnerThreadRejectsWithoutRunning) {
    constexpr int64_t kBudgetMs = 1000;
    CommandQueue q;
    q.drain();
    ASSERT_TRUE(q.is_main_thread());
    bool ran = false;
    const CommandQueue::Stats before = q.stats();
    auto submission = q.submit_tracked_bounded(
        [&ran] {
          ran = true;
          return 7;
        },
        kBudgetMs);
    EXPECT_FALSE(submission.timed_out);
    EXPECT_FALSE(submission.cancelled);
    EXPECT_FALSE(ran);
    EXPECT_GE(submission.waited_ms, 0);
    EXPECT_THROW(submission.future.get(), CommandQueue::Rejected);
    EXPECT_EQ(q.stats().pending, 0u);
    EXPECT_EQ(q.stats().submitted, before.submitted);
    EXPECT_EQ(q.stats().executed, before.executed);
}

TEST(CommandQueueTest, BoundedSubmitNonPositiveBudgetRejectsImmediately) {
    CommandQueue q;
    const uint64_t cancelled_before = q.stats().cancelled;
    auto submission = q.submit_tracked_bounded([] { return 5; }, 0);
    EXPECT_TRUE(submission.timed_out);
    EXPECT_TRUE(submission.cancelled);
    EXPECT_GE(submission.waited_ms, 0);
    EXPECT_EQ(q.stats().cancelled, cancelled_before + 1);
    EXPECT_THROW(submission.future.get(), std::runtime_error);
}

TEST(CommandQueueTest, BoundedSubmitNegativeBudgetRejectsImmediately) {
    CommandQueue q;
    auto submission = q.submit_tracked_bounded([] { return 5; }, -5);
    EXPECT_TRUE(submission.timed_out);
    EXPECT_TRUE(submission.cancelled);
    EXPECT_THROW(submission.future.get(), std::runtime_error);
}

TEST(CommandQueueTest, BoundedSubmitTimeoutCancelsPendingTask) {
    CommandQueue q;
    bool ran = false;
    auto submission = q.submit_tracked_bounded([&ran] { ran = true; }, 50);
    EXPECT_TRUE(submission.timed_out);
    EXPECT_TRUE(submission.cancelled);
    EXPECT_GE(submission.waited_ms, 0);
    q.drain();
    EXPECT_FALSE(ran);
}

TEST(CommandQueueTest, BoundedSubmitTimeoutIncrementsCancelledCounter) {
    CommandQueue q;
    const uint64_t cancelled_before = q.stats().cancelled;
    auto submission = q.submit_tracked_bounded([] { return 7; }, 30);
    EXPECT_TRUE(submission.timed_out);
    EXPECT_EQ(q.stats().cancelled, cancelled_before + 1);
}

TEST(CommandQueueTest, CloseClearsMainThreadBinding) {
    CommandQueue q;
    q.drain();
    ASSERT_TRUE(q.is_main_thread());
    q.close();
    EXPECT_TRUE(q.is_closed());
    EXPECT_FALSE(q.is_main_thread());
    EXPECT_FALSE(q.drain());
    EXPECT_FALSE(q.is_main_thread());
}

TEST(CommandQueueTest, ClosedQueueRejectsSubmitOnFormerMainThread) {
    CommandQueue q;
    q.drain();
    q.close();
    bool ran = false;
    auto future = q.submit([&ran] { ran = true; });
    EXPECT_FALSE(ran);
    EXPECT_THROW(future.get(), CommandQueue::Rejected);
    EXPECT_EQ(q.stats().rejected_closed, 1u);
}

TEST(CommandQueueTest, ClosedQueueRejectsBoundedSubmitOnFormerMainThread) {
    CommandQueue q;
    q.drain();
    q.close();
    bool ran = false;
    auto submission = q.submit_tracked_bounded([&ran] { ran = true; }, 1000);
    EXPECT_FALSE(submission.timed_out);
    EXPECT_FALSE(submission.cancelled);
    EXPECT_FALSE(ran);
    EXPECT_THROW(submission.future.get(), CommandQueue::Rejected);
    EXPECT_EQ(q.stats().rejected_closed, 1u);
}

TEST(CommandQueueTest, ClosedQueueRejectsZeroBudgetBoundedSubmit) {
    CommandQueue q;
    q.drain();
    q.close();
    bool ran = false;
    auto submission = q.submit_tracked_bounded([&ran] { ran = true; }, 0);
    EXPECT_FALSE(submission.timed_out);
    EXPECT_FALSE(submission.cancelled);
    EXPECT_FALSE(ran);
    EXPECT_THROW(submission.future.get(), CommandQueue::Rejected);
    EXPECT_EQ(q.stats().rejected_closed, 1u);
}

TEST(CommandQueueTest, MainThreadZeroBudgetBoundedSubmitRejectsWithoutRunning) {
    CommandQueue q;
    q.drain();
    bool ran = false;
    const uint64_t cancelled_before = q.stats().cancelled;
    auto submission = q.submit_tracked_bounded([&ran] { ran = true; }, 0);
    EXPECT_FALSE(submission.timed_out);
    EXPECT_FALSE(submission.cancelled);
    EXPECT_FALSE(ran);
    EXPECT_EQ(q.stats().cancelled, cancelled_before);
    EXPECT_THROW(submission.future.get(), CommandQueue::Rejected);
}

TEST(CommandQueueTest, MainThreadNegativeBudgetBoundedSubmitRejectsWithoutRunning) {
    CommandQueue q;
    q.drain();
    bool ran = false;
    auto submission = q.submit_tracked_bounded([&ran] { ran = true; }, -1);
    EXPECT_FALSE(submission.timed_out);
    EXPECT_FALSE(submission.cancelled);
    EXPECT_FALSE(ran);
    EXPECT_THROW(submission.future.get(), CommandQueue::Rejected);
}

TEST(CommandQueueTest, FullQueueDoesNotChangeOwnerThreadBoundedRejection) {
    CommandQueue q(1);
    q.drain();
    auto pending = q.submit([] { return 11; });
    bool ran = false;
    const uint64_t rejected_before = q.stats().rejected_full;
    auto submission = q.submit_tracked_bounded([&ran] { ran = true; }, 1000);
    EXPECT_FALSE(submission.timed_out);
    EXPECT_FALSE(submission.cancelled);
    EXPECT_FALSE(ran);
    EXPECT_EQ(q.stats().rejected_full, rejected_before);
    EXPECT_THROW(submission.future.get(), CommandQueue::Rejected);
    EXPECT_EQ(q.stats().pending, 1u);
    q.drain();
    EXPECT_EQ(pending.get(), 11);
}

TEST(CommandQueueTest, CloseRejectsPendingTasksAndClearsBinding) {
    CommandQueue q;
    auto pending = q.submit([] { return 13; });
    q.close();
    EXPECT_FALSE(q.is_main_thread());
    EXPECT_EQ(q.stats().pending, 0u);
    EXPECT_EQ(q.stats().dropped_on_close, 1u);
    EXPECT_THROW(pending.get(), CommandQueue::Rejected);
}
