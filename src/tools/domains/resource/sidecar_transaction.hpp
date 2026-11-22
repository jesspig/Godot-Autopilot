#ifndef GODOT_AUTOPILOT_SIDECAR_TRANSACTION_HPP
#define GODOT_AUTOPILOT_SIDECAR_TRANSACTION_HPP

#include <vector>

namespace godot_autopilot {
namespace resource_ops_detail {

enum class SidecarKind { kUid, kImport };

struct SidecarMove {
  SidecarKind kind;
  bool moved{false};
};

inline std::vector<SidecarKind> rollback_order(const std::vector<SidecarMove> &moves) {
  std::vector<SidecarKind> order;
  for (auto it = moves.rbegin(); it != moves.rend(); ++it) {
    if (it->moved) order.push_back(it->kind);
  }
  return order;
}

} // namespace resource_ops_detail
} // namespace godot_autopilot

#endif
