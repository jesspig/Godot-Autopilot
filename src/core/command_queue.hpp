#ifndef GODOT_AUTOPILOT_COMMAND_QUEUE_HPP
#define GODOT_AUTOPILOT_COMMAND_QUEUE_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace godot_autopilot {

class CommandQueue {
  struct TaskBase {
    uint64_t task_id = 0;
    int64_t enqueued_ns = 0;
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
  std::deque<std::unique_ptr<TaskBase>> tasks_;
  mutable std::mutex mutex_;
  std::thread::id main_thread_id_;
  size_t capacity_;
  bool closed_ = false;
  std::atomic<uint64_t> next_task_id_{1};
  std::atomic<uint64_t> submitted_{0};
  std::atomic<uint64_t> executed_{0};
  std::atomic<uint64_t> rejected_closed_{0};
  std::atomic<uint64_t> rejected_full_{0};
  std::atomic<uint64_t> dropped_on_close_{0};
  std::atomic<uint64_t> cancelled_{0};
  std::atomic<uint64_t> lock_wait_ns_{0};
  std::atomic<int64_t> last_drain_ns_{0};

  static uint64_t elapsed_ns(const std::chrono::steady_clock::time_point &start) {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - start)
            .count());
  }

  static int64_t now_ns() {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
  }

  uint64_t enqueue(std::unique_ptr<TaskBase> task) {
    const uint64_t id = next_task_id_.fetch_add(1, std::memory_order_relaxed);
    task->task_id = id;
    task->enqueued_ns = now_ns();
    const auto lock_start = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    lock_wait_ns_.fetch_add(elapsed_ns(lock_start), std::memory_order_relaxed);
    if (closed_) {
      rejected_closed_.fetch_add(1, std::memory_order_relaxed);
      task->reject(std::make_exception_ptr(
          std::runtime_error("CommandQueue is closed; task was not submitted")));
    } else if (tasks_.size() >= capacity_) {
      rejected_full_.fetch_add(1, std::memory_order_relaxed);
      task->reject(std::make_exception_ptr(
          std::runtime_error("CommandQueue is full; task was not submitted")));
    } else {
      submitted_.fetch_add(1, std::memory_order_relaxed);
      tasks_.push_back(std::move(task));
    }
    return id;
  }

 public:
  struct Stats {
    uint64_t submitted = 0;
    uint64_t executed = 0;
    uint64_t rejected_closed = 0;
    uint64_t rejected_full = 0;
    uint64_t dropped_on_close = 0;
    uint64_t cancelled = 0;
    uint64_t lock_wait_ns = 0;
    size_t pending = 0;
    size_t capacity = 0;
  };

  template <typename Result> struct TrackedSubmission {
    uint64_t id = 0;
    std::future<Result> future;
  };

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
    std::deque<std::unique_ptr<TaskBase>> pending;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (closed_ && tasks_.empty()) {
        return;
      }
      closed_ = true;
      dropped_on_close_.fetch_add(static_cast<uint64_t>(tasks_.size()),
                                  std::memory_order_relaxed);
      pending.swap(tasks_);
    }

    auto error = std::make_exception_ptr(
        std::runtime_error("CommandQueue is closed; task was not executed"));
    while (!pending.empty()) {
      pending.front()->reject(error);
      pending.pop_front();
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

  Stats stats() const {
    Stats out;
    out.submitted = submitted_.load(std::memory_order_relaxed);
    out.executed = executed_.load(std::memory_order_relaxed);
    out.rejected_closed = rejected_closed_.load(std::memory_order_relaxed);
    out.rejected_full = rejected_full_.load(std::memory_order_relaxed);
    out.dropped_on_close = dropped_on_close_.load(std::memory_order_relaxed);
    out.cancelled = cancelled_.load(std::memory_order_relaxed);
    uint64_t wait_ns = lock_wait_ns_.load(std::memory_order_relaxed);
    const auto lock_start = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    wait_ns += elapsed_ns(lock_start);
    out.lock_wait_ns = wait_ns;
    out.pending = tasks_.size();
    out.capacity = capacity_;
    return out;
  }

  int64_t last_drain_age_ms() const {
    const int64_t last = last_drain_ns_.load(std::memory_order_relaxed);
    if (last <= 0) {
      return -1;
    }
    return (now_ns() - last) / 1000000;
  }

  int64_t oldest_pending_age_ms() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tasks_.empty()) {
      return -1;
    }
    return (now_ns() - tasks_.front()->enqueued_ns) / 1000000;
  }

  template <typename Fn>
  auto submit(Fn &&fn) -> std::future<std::invoke_result_t<Fn>> {
    auto task = std::make_unique<Task<Fn>>(std::forward<Fn>(fn));
    auto future = task->promise.get_future();
    enqueue(std::move(task));
    return future;
  }

  template <typename Fn>
  auto submit_tracked(Fn &&fn) -> TrackedSubmission<std::invoke_result_t<Fn>> {
    auto task = std::make_unique<Task<Fn>>(std::forward<Fn>(fn));
    TrackedSubmission<std::invoke_result_t<Fn>> out;
    out.future = task->promise.get_future();
    out.id = enqueue(std::move(task));
    return out;
  }

  bool cancel(uint64_t id) {
    std::unique_ptr<TaskBase> removed;
    {
      const auto lock_start = std::chrono::steady_clock::now();
      std::lock_guard<std::mutex> lock(mutex_);
      lock_wait_ns_.fetch_add(elapsed_ns(lock_start), std::memory_order_relaxed);
      for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
        if ((*it)->task_id == id) {
          removed = std::move(*it);
          tasks_.erase(it);
          break;
        }
      }
    }
    if (!removed) {
      return false;
    }
    cancelled_.fetch_add(1, std::memory_order_relaxed);
    removed->reject(std::make_exception_ptr(std::runtime_error(
        "CommandQueue task cancelled before execution")));
    return true;
  }

  template <typename Fn>
  auto execute_sync(Fn &&fn) -> std::invoke_result_t<Fn> {
    if (is_main_thread()) {
      return std::forward<Fn>(fn)();
    }
    return submit(std::forward<Fn>(fn)).get();
  }

  bool drain() {
    last_drain_ns_.store(now_ns(), std::memory_order_relaxed);
    std::deque<std::unique_ptr<TaskBase>> batch;
    {
      const auto lock_start = std::chrono::steady_clock::now();
      std::lock_guard<std::mutex> lock(mutex_);
      lock_wait_ns_.fetch_add(elapsed_ns(lock_start), std::memory_order_relaxed);
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
    executed_.fetch_add(static_cast<uint64_t>(batch.size()), std::memory_order_relaxed);
    while (!batch.empty()) {
      batch.front()->execute();
      batch.pop_front();
    }
    return true;
  }
};

} // namespace godot_autopilot

#endif
