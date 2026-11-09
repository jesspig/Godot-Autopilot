#ifndef GODOT_AUTOPILOT_CODE_EXEC_OPS_HPP
#define GODOT_AUTOPILOT_CODE_EXEC_OPS_HPP

#include <cstdint>
#include <string>

#include <mcp/JsonValue.hpp>

namespace godot {

class UndoRedo;

}

namespace godot_autopilot {
namespace code_exec_ops {

int max_timeout_ms();

mcp::JsonValue handle_batch_execute(const mcp::JsonValue &args);
mcp::JsonValue handle_code_execute(const mcp::JsonValue &args);

void batch_undo_snapshot(godot::UndoRedo *&history, uint64_t &version_before,
                         std::string &error);

struct BatchUndoRollback {
  int64_t actions_created{0};
  int64_t rolled_back{0};
};

bool batch_rollback_undo(godot::UndoRedo *history, uint64_t version_before,
                         BatchUndoRollback &rollback);

} // namespace code_exec_ops
} // namespace godot_autopilot
#endif
