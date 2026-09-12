#ifndef GODOT_AUTOPILOT_COMMAND_QUEUE_HPP
#define GODOT_AUTOPILOT_COMMAND_QUEUE_HPP

#include <cstddef>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace godot_autopilot {

class CommandQueue {
  struct TaskBase {
    virtual ~TaskBase() = default;
    virtual void execute() = 0;
    virtual void reject(std::exception_ptr error) noexcept = 0;
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
    void reject(std::exception_ptr error) noexcept override {
      try {
        promise.set_exception(error);
      } catch (...) {
      }
    }
  };

  static constexpr size_t DEFAULT_CAPACITY = 1024;
  std::queue<std::unique_ptr<TaskBase>> tasks_;
  mutable std::mutex mutex_;
  std::thread::id main_thread_id_;
  size_t capacity_;
  bool closed_ = false;

 public:
  explicit CommandQueue(size_t capacity = DEFAULT_CAPACITY) : capacity_(capacity) {
    if (capacity == 0) {
      throw std::invalid_argument("CommandQueue capacity must be greater than zero");
    }
  }
  ~CommandQueue() { close(); }

  void open() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!tasks_.empty()) {
      throw std::logic_error("cannot open CommandQueue with pending tasks");
    }
    closed_ = false;
    main_thread_id_ = std::thread::id();
  }

  void close() noexcept {
    std::queue<std::unique_ptr<TaskBase>> pending;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (closed_ && tasks_.empty()) {
        return;
      }
      closed_ = true;
      pending.swap(tasks_);
    }

    auto error = std::make_exception_ptr(
        std::runtime_error("CommandQueue is closed; task was not executed"));
    while (!pending.empty()) {
      pending.front()->reject(error);
      pending.pop();
    }
  }

  bool is_closed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
  }

  bool is_main_thread() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return main_thread_id_ == std::this_thread::get_id();
  }

  template <typename Fn>
  auto submit(Fn &&fn) -> std::future<std::invoke_result_t<Fn>> {
    auto task = std::make_unique<Task<Fn>>(std::forward<Fn>(fn));
    auto future = task->promise.get_future();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (closed_) {
        task->reject(std::make_exception_ptr(
            std::runtime_error("CommandQueue is closed; task was not submitted")));
      } else if (tasks_.size() >= capacity_) {
        task->reject(std::make_exception_ptr(
            std::runtime_error("CommandQueue is full; task was not submitted")));
      } else {
        tasks_.push(std::move(task));
      }
    }
    return future;
  }

  template <typename Fn>
  auto execute_sync(Fn &&fn) -> std::invoke_result_t<Fn> {
    if (is_main_thread()) {
      return std::forward<Fn>(fn)();
    }
    return submit(std::forward<Fn>(fn)).get();
  }

  bool drain() {
    std::queue<std::unique_ptr<TaskBase>> batch;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (closed_) {
        return false;
      }
      if (main_thread_id_ == std::thread::id()) {
        main_thread_id_ = std::this_thread::get_id();
      } else if (main_thread_id_ != std::this_thread::get_id()) {
        return false;
      }
      batch.swap(tasks_);
    }
    while (!batch.empty()) {
      batch.front()->execute();
      batch.pop();
    }
    return true;
  }
};

} // namespace godot_autopilot

#endif
