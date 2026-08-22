#include "log_system.hpp"

#include <algorithm>
#include <cctype>

namespace godot_autopilot {

void LogSystem::log(LogLevel level, LogCategory category,
                    const std::string &message) {
  LogEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.level = level;
  entry.category = category;
  entry.message = message;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (entries_.size() >= MAX_ENTRIES) {
      entries_.pop_front();
    }
    entry.serial = next_serial_++;
    entries_.push_back(std::move(entry));
  }
}

std::vector<const LogEntry *> LogSystem::query(const Query &q) const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<const LogEntry *> result;
  result.reserve(entries_.size());

  for (const auto &e : entries_) {
    if (e.level < q.min_level) {
      continue;
    }
    if (q.category.has_value() && e.category != *q.category) {
      continue;
    }
    if (!q.filter_text.empty()) {
      auto it =
          std::search(e.message.begin(), e.message.end(), q.filter_text.begin(),
                      q.filter_text.end(), [](char a, char b) {
                        return std::tolower(static_cast<unsigned char>(a)) ==
                               std::tolower(static_cast<unsigned char>(b));
                      });
      if (it == e.message.end()) {
        continue;
      }
    }
    result.push_back(&e);
  }

  return result;
}

std::vector<const LogEntry *> LogSystem::query_from(size_t start_index,
                                                    size_t *next_index) const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<const LogEntry *> result;
  for (const auto &e : entries_) {
    if (e.serial >= start_index) {
      result.push_back(&e);
    }
  }
  if (next_index) {
    *next_index = next_serial_;
  }
  return result;
}

size_t LogSystem::next_index() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return next_serial_;
}

LogSystem &LogSystem::instance() {
  static LogSystem inst;
  return inst;
}

} // namespace godot_autopilot
