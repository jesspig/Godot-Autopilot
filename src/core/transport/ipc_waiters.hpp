#ifndef GODOT_AUTOPILOT_IPC_WAITERS_HPP
#define GODOT_AUTOPILOT_IPC_WAITERS_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace godot_autopilot {

class IpcWaiterSet {
public:
  using CancellationToken = std::shared_ptr<std::atomic<bool>>;
  using Task = std::function<void(const CancellationToken &)>;

  void open();
  bool spawn(Task task);
  void cancel();
  void join();
  std::size_t size() const;

private:
  struct Waiter {
    std::thread thread;
    CancellationToken cancelled;
    bool done{false};
  };

  void reap_finished();

  mutable std::mutex mutex_;
  std::unordered_map<std::uint64_t, Waiter> waiters_;
  std::uint64_t next_id_{1};
  bool stopping_{true};
};

inline void IpcWaiterSet::open() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!waiters_.empty()) {
    throw std::logic_error("cannot reopen IPC waiter set with active waiters");
  }
  stopping_ = false;
}

inline bool IpcWaiterSet::spawn(Task task) {
  if (!task) {
    throw std::invalid_argument("IPC waiter task must not be empty");
  }
  reap_finished();
  std::lock_guard<std::mutex> lock(mutex_);
  if (stopping_) {
    return false;
  }
  const std::uint64_t id = next_id_++;
  CancellationToken cancelled = std::make_shared<std::atomic<bool>>(false);
  waiters_.emplace(id, Waiter{std::thread{}, cancelled, false});
  auto it = waiters_.find(id);
  try {
    it->second.thread = std::thread(
        [this, id, cancelled, task = std::move(task)]() mutable {
          try {
            task(cancelled);
          } catch (...) {
          }
          std::lock_guard<std::mutex> lock(mutex_);
          const auto entry = waiters_.find(id);
          if (entry != waiters_.end()) {
            entry->second.done = true;
          }
        });
  } catch (...) {
    waiters_.erase(it);
    throw;
  }
  return true;
}

inline void IpcWaiterSet::cancel() {
  std::lock_guard<std::mutex> lock(mutex_);
  stopping_ = true;
  for (auto &entry : waiters_) {
    entry.second.cancelled->store(true, std::memory_order_release);
  }
}

inline void IpcWaiterSet::join() {
  std::unordered_map<std::uint64_t, Waiter> pending;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
    pending.swap(waiters_);
  }
  for (auto &entry : pending) {
    if (entry.second.thread.joinable()) {
      entry.second.thread.join();
    }
  }
}

inline std::size_t IpcWaiterSet::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return waiters_.size();
}

inline void IpcWaiterSet::reap_finished() {
  std::vector<std::thread> finished;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = waiters_.begin(); it != waiters_.end();) {
      if (!it->second.done) {
        ++it;
        continue;
      }
      finished.push_back(std::move(it->second.thread));
      it = waiters_.erase(it);
    }
  }
  for (std::thread &thread : finished) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

} // namespace godot_autopilot

#endif
