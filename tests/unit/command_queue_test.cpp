#include "core/command_queue.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <thread>

using godot_self_driving::CommandQueue;

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
