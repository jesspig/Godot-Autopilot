#ifndef GODOT_AUTOPILOT_PORT_RECOVERY_HPP
#define GODOT_AUTOPILOT_PORT_RECOVERY_HPP

#include <string>
#include <utility>
#include <vector>

#include "core/instance_registry.hpp"
#include "core/path_util.hpp"

namespace godot_autopilot {
namespace port_recovery {

struct ResidualInstance {
  std::string instance_id;
  int port{0};
  int pid{0};
  bool alive{false};
};

struct RecoveryPlan {
  std::vector<ResidualInstance> stale;
  std::vector<ResidualInstance> live;
};

inline bool same_project_path(const std::string &left,
                              const std::string &right) {
  if (left.empty() || right.empty()) {
    return false;
  }
  return trim_path_trailing_separators(left) ==
         trim_path_trailing_separators(right);
}

template <typename AlivePredicate>
RecoveryPlan analyze(const std::vector<InstanceRecord> &records,
                     const std::string &project_path, int current_pid,
                     AlivePredicate is_alive) {
  RecoveryPlan plan;
  for (const InstanceRecord &record : records) {
    if (!same_project_path(record.project_path, project_path)) {
      continue;
    }
    if (record.pid <= 0 || record.pid == current_pid) {
      continue;
    }
    ResidualInstance residual;
    residual.instance_id = record.instance_id;
    residual.port = record.port;
    residual.pid = record.pid;
    residual.alive = is_alive(record.pid);
    if (residual.alive) {
      plan.live.push_back(std::move(residual));
    } else {
      plan.stale.push_back(std::move(residual));
    }
  }
  return plan;
}

inline std::string describe(const ResidualInstance &residual) {
  return "instance " + residual.instance_id + " (pid " +
         std::to_string(residual.pid) + ", port " +
         std::to_string(residual.port) + ")";
}

inline std::string build_hint(const RecoveryPlan &plan, int port) {
  const size_t total = plan.stale.size() + plan.live.size();
  if (total == 0) {
    return std::string();
  }
  std::string hint = "port " + std::to_string(port) +
                     " is unavailable; this project has " +
                     std::to_string(total) +
                     " residual instance registration(s) from an earlier run";
  if (!plan.stale.empty()) {
    hint += "; " + std::to_string(plan.stale.size()) +
            " stale registration(s) were removed:";
    for (const ResidualInstance &residual : plan.stale) {
      hint += " " + describe(residual);
    }
  }
  if (!plan.live.empty()) {
    hint += "; " + std::to_string(plan.live.size()) +
            " registration(s) belong to a running process:";
    for (const ResidualInstance &residual : plan.live) {
      hint += " " + describe(residual);
    }
    hint += "; stop that process or choose another port";
  }
  hint +=
      "; the registration files live in the instances directory of the GDA "
      "home and can be deleted by hand";
  return hint;
}

} // namespace port_recovery
} // namespace godot_autopilot

#endif
