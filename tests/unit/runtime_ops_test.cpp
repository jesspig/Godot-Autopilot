#include "core/command_queue.hpp"
#include "tools/runtime_ops.hpp"

#include <gtest/gtest.h>

using godot_self_driving::CommandQueue;
using godot_self_driving::runtime_ops::has_editor_queue;
using godot_self_driving::runtime_ops::set_editor_queue;

TEST(RuntimeOpsTest, EditorQueueInjectionRoundTrip) {
    CommandQueue q;
    EXPECT_FALSE(has_editor_queue());
    set_editor_queue(&q);
    EXPECT_TRUE(has_editor_queue());
    set_editor_queue(nullptr);
    EXPECT_FALSE(has_editor_queue());
    set_editor_queue(nullptr);
}
