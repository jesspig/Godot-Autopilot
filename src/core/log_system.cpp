#include "log_system.hpp"

#include <algorithm>
#include <cctype>

namespace godot_autopilot {

namespace {

bool contains_ci(const std::string &haystack, const std::string &needle) {
  if (needle.empty()) {
    return true;
  }
  auto it = std::search(haystack.begin(), haystack.end(), needle.begin(),
                        needle.end(), [](char a, char b) {
                          return std::tolower(static_cast<unsigned char>(a)) ==
                                 std::tolower(static_cast<unsigned char>(b));
                        });
  return it != haystack.end();
}

} // namespace

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

void LogSystem::log_detailed(LogLevel level, LogCategory category,
                             const std::string &summary,
                             const std::string &detail) {
  log_detailed(level, category, summary, detail, std::string(), std::string());
}

void LogSystem::log_detailed(LogLevel level, LogCategory category,
                             const std::string &summary,
                             const std::string &detail,
                             const std::string &trace_id,
                             const std::string &span_id) {
  LogEntry entry;
  entry.timestamp = std::chrono::system_clock::now();
  entry.level = level;
  entry.category = category;
  entry.message = summary;
  if (detail.size() > MAX_DETAIL_CHARS) {
    entry.detail = detail.substr(0, MAX_DETAIL_CHARS);
    entry.detail += "...[truncated]";
  } else {
    entry.detail = detail;
  }
  entry.trace_id = trace_id;
  entry.span_id = span_id;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (entries_.size() >= MAX_ENTRIES) {
      entries_.pop_front();
    }
    entry.serial = next_serial_++;
    entries_.push_back(std::move(entry));
  }
}

std::vector<LogEntry> LogSystem::query(const Query &q) const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<LogEntry> result;
  for (const auto &e : entries_) {
    if (e.level < q.min_level) {
      continue;
    }
    if (q.category.has_value() && e.category != *q.category) {
      continue;
    }
    if (!q.filter_text.empty() && !contains_ci(e.message, q.filter_text) &&
        !contains_ci(e.detail, q.filter_text) &&
        !contains_ci(e.trace_id, q.filter_text) &&
        !contains_ci(e.span_id, q.filter_text)) {
      continue;
    }
    result.push_back(e);
  }

  return result;
}

std::vector<LogEntry> LogSystem::query_recent(size_t limit) const {
  std::lock_guard<std::mutex> lock(mutex_);

  const size_t start = entries_.size() > limit ? entries_.size() - limit : 0;
  std::vector<LogEntry> result;
  result.reserve(entries_.size() - start);
  for (size_t i = start; i < entries_.size(); ++i) {
    result.push_back(entries_[i]);
  }
  return result;
}

std::vector<LogEntry> LogSystem::query_from(size_t start_index,
                                            size_t *next_index) const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<LogEntry> result;
  for (const auto &e : entries_) {
    if (e.serial >= start_index) {
      result.push_back(e);
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
