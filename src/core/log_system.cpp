#include "log_system.hpp"

#include <algorithm>
#include <cctype>

namespace godot_self_driving {

void LogSystem::log(LogLevel level, LogCategory category,
                    const std::string &message) {
  LogEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.level = level;
  entry.category = category;
  entry.message = message;

  OnNewEntryCallback cb;
  LogEntry saved;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (entries_.size() >= MAX_ENTRIES) {
      entries_.erase(entries_.begin());
    }
    entries_.push_back(std::move(entry));
    saved = entries_.back();
    cb = on_new_entry_;
  }

  if (cb) {
    cb(saved);
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

LogSystem &LogSystem::instance() {
  static LogSystem inst;
  return inst;
}

void LogSystem::set_on_new_entry(OnNewEntryCallback callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  on_new_entry_ = std::move(callback);
}

} // namespace godot_self_driving
