#ifndef GODOT_SELF_DRIVING_COMMAND_QUEUE_HPP
#define GODOT_SELF_DRIVING_COMMAND_QUEUE_HPP

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>

namespace godot_self_driving {

class CommandQueue {
  struct TaskBase {
    virtual ~TaskBase() = default;
    virtual void execute() = 0;
  };

  template <typename Fn> struct Task : TaskBase {
    Fn fn;
    std::promise<std::invoke_result_t<Fn>> promise;

    Task(Fn &&f) : fn(std::forward<Fn>(f)) {}
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
  std::atomic<std::thread::id> main_thread_id_{};

public:
  CommandQueue() = default;
  ~CommandQueue() = default;

  bool is_main_thread() const {
    return main_thread_id_.load(std::memory_order_relaxed) ==
           std::this_thread::get_id();
  }

  template <typename Fn>
  auto submit(Fn &&fn) -> std::future<std::invoke_result_t<Fn>> {
    auto task = std::make_unique<Task<Fn>>(std::forward<Fn>(fn));
    auto future = task->promise.get_future();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks_.push(std::move(task));
    }
    return future;
  }

  void drain() {
    if (main_thread_id_.load(std::memory_order_relaxed) ==
        std::thread::id()) {
      main_thread_id_.store(std::this_thread::get_id(),
                            std::memory_order_relaxed);
    }
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
