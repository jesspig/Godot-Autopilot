#ifndef GODOT_AUTOPILOT_CODE_EXEC_OPS_HPP
#define GODOT_AUTOPILOT_CODE_EXEC_OPS_HPP

#include <cstdint>
#include <string>

#include "util/json_value.hpp"

namespace godot {

class UndoRedo;

}

namespace godot_autopilot {
namespace code_exec_ops {

int max_timeout_ms();

godot_autopilot::util::JsonValue handle_batch_execute(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_code_execute(const godot_autopilot::util::JsonValue &args);

void batch_undo_snapshot(godot::UndoRedo *&history, uint64_t &version_before,
                         std::string &error);

int32_t batch_resolve_history_id(int32_t object_history_id,
                                 bool has_target_object);

struct BatchRollbackReport {
  int64_t expected_actions{0};
  bool partial{false};
};

BatchRollbackReport batch_rollback_report(int64_t actions_created,
                                          int64_t undoable_successes,
                                          int64_t rolled_back);

struct BatchUndoRollback {
  int64_t actions_created{0};
  int64_t rolled_back{0};
};

bool batch_rollback_undo(godot::UndoRedo *history, uint64_t version_before,
                         BatchUndoRollback &rollback);

} // namespace code_exec_ops
} // namespace godot_autopilot
#endif
