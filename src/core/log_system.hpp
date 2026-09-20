#ifndef GODOT_AUTOPILOT_LOG_SYSTEM_HPP
#define GODOT_AUTOPILOT_LOG_SYSTEM_HPP

#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace godot_autopilot {

enum class LogLevel { Debug, Info, Warning, Error };
enum class LogCategory { System, Transport, Tools, Resources, Prompts };

struct LogEntry {
  std::chrono::system_clock::time_point timestamp;
  LogLevel level;
  LogCategory category;
  std::string message;
  std::string detail{};
  size_t serial = 0;
};

class LogSystem {
public:
  static constexpr int MAX_ENTRIES = 10000;
  static constexpr size_t MAX_DETAIL_CHARS = 8192;

  void log(LogLevel level, LogCategory category, const std::string &message);
  void log_detailed(LogLevel level, LogCategory category,
                    const std::string &summary, const std::string &detail);

  struct Query {
    LogLevel min_level = LogLevel::Debug;
    std::string filter_text;
    std::optional<LogCategory> category;
  };
  std::vector<LogEntry> query(const Query &q) const;
  std::vector<LogEntry> query_recent(size_t limit) const;

  std::vector<LogEntry> query_from(size_t start_index,
                                   size_t *next_index) const;
  size_t next_index() const;

  static LogSystem &instance();

private:
  std::deque<LogEntry> entries_;
  mutable std::mutex mutex_;
  size_t next_serial_ = 0;
};

} // namespace godot_autopilot

#endif
