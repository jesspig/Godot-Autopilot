#ifndef GODOT_SELF_DRIVING_COMMAND_QUEUE_HPP
#define GODOT_SELF_DRIVING_COMMAND_QUEUE_HPP

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <type_traits>

namespace godot_self_driving {

class CommandQueue {
    struct TaskBase {
        virtual ~TaskBase() = default;
        virtual void execute() = 0;
    };

    template <typename Fn>
    struct Task : TaskBase {
        Fn fn;
        std::promise<std::invoke_result_t<Fn>> promise;

        Task(Fn&& f) : fn(std::forward<Fn>(f)) {}
        void execute() override {
            try {
                if constexpr (std::is_void_v<std::invoke_result_t<Fn>>) {
                    fn();
                    promise.set_value();
                } else {
                    promise.set_value(fn());
                }
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        }
    };

    std::queue<std::unique_ptr<TaskBase>> tasks_;
    std::mutex mutex_;

public:
    CommandQueue() = default;
    ~CommandQueue() = default;

    template <typename Fn>
    auto submit(Fn&& fn) -> std::future<std::invoke_result_t<Fn>> {
        auto task = std::make_unique<Task<Fn>>(std::forward<Fn>(fn));
        auto future = task->promise.get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }
        return future;
    }

    void drain() {
        std::queue<std::unique_ptr<TaskBase>> batch;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            batch.swap(tasks_);
        }
        while (!batch.empty()) {
            batch.front()->execute();
            batch.pop();
        }
    }
};

} // namespace godot_self_driving

#endif
