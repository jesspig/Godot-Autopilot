#ifndef GODOT_AUTOPILOT_CSHARP_BUILD_RESULT_HPP
#define GODOT_AUTOPILOT_CSHARP_BUILD_RESULT_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace editor_ops_detail {

struct BuildDigest {
  std::string text;
  bool truncated{false};
};

inline BuildDigest truncate_digest(const std::string &combined, std::size_t limit) {
  BuildDigest result;
  if (combined.size() <= limit) {
    result.text = combined;
    return result;
  }
  result.text = combined.substr(0, limit) + "\n...(truncated, total " +
                std::to_string(combined.size()) + " chars)";
  result.truncated = true;
  return result;
}

struct ErrorScan {
  std::vector<std::string> errors;
  bool truncated{false};
};

inline ErrorScan scan_error_lines(const std::vector<std::string> &lines,
                                  std::size_t max_errors) {
  ErrorScan result;
  for (const std::string &line : lines) {
    std::string lowered;
    lowered.reserve(line.size());
    for (char c : line) {
      lowered.push_back(static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c));
    }
    if (lowered.find("error") == std::string::npos) continue;
    if (result.errors.size() < max_errors) {
      result.errors.push_back(line);
    } else {
      result.truncated = true;
    }
  }
  return result;
}

} // namespace editor_ops_detail
} // namespace godot_autopilot

#endif
