#ifndef GODOT_AUTOPILOT_PROPERTY_ROLLBACK_HPP
#define GODOT_AUTOPILOT_PROPERTY_ROLLBACK_HPP

#include <string>
#include <vector>

namespace godot_autopilot {
namespace property_ops_detail {

struct RollbackOutcome {
  std::vector<std::string> rolled_back;
  std::vector<std::string> unrestored;
};

inline RollbackOutcome partition_rollback(const std::vector<std::string> &names,
                                          const std::vector<bool> &restored) {
  RollbackOutcome outcome;
  for (std::size_t i = 0; i < names.size(); ++i) {
    const bool confirmed = i < restored.size() && restored[i];
    if (confirmed) {
      outcome.rolled_back.push_back(names[i]);
    } else {
      outcome.unrestored.push_back(names[i]);
    }
  }
  return outcome;
}

} // namespace property_ops_detail
} // namespace godot_autopilot

#endif
