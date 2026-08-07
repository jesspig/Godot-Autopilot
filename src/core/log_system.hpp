#ifndef GODOT_SELF_DRIVING_LOG_SYSTEM_HPP
#define GODOT_SELF_DRIVING_LOG_SYSTEM_HPP

#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace godot_self_driving {

enum class LogLevel { Debug, Info, Warning, Error };
enum class LogCategory { System, Transport, Tools, Resources, Prompts };

struct LogEntry {
  std::chrono::system_clock::time_point timestamp;
  LogLevel level;
  LogCategory category;
  std::string message;
  size_t serial = 0;
};

class LogSystem {
public:
  static constexpr int MAX_ENTRIES = 10000;

  void log(LogLevel level, LogCategory category, const std::string &message);

  struct Query {
    LogLevel min_level = LogLevel::Debug;
    std::string filter_text;
    std::optional<LogCategory> category;
  };
  std::vector<const LogEntry *> query(const Query &q) const;

  std::vector<const LogEntry *> query_from(size_t start_index,
                                           size_t *next_index) const;
  size_t next_index() const;

  static LogSystem &instance();

  using OnNewEntryCallback = std::function<void(const LogEntry &)>;
  void set_on_new_entry(OnNewEntryCallback callback);

private:
  std::deque<LogEntry> entries_;
  mutable std::mutex mutex_;
  size_t next_serial_ = 0;
  OnNewEntryCallback on_new_entry_;
};

} // namespace godot_self_driving

#endif
