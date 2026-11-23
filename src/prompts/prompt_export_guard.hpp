#ifndef GODOT_AUTOPILOT_PROMPT_EXPORT_GUARD_HPP
#define GODOT_AUTOPILOT_PROMPT_EXPORT_GUARD_HPP

#include "core/export_guard.hpp"
#include "tools/base/dispatch.hpp"

#include <mcp/McpError.hpp>

#include <string>

namespace godot_autopilot {
namespace prompts_detail {

inline bool should_block_prompt(bool exporting) {
  return exporting;
}

inline void reject_if_exporting() {
  if (should_block_prompt(godot_autopilot::ExportGuard::is_exporting())) {
    throw mcp::McpError(mcp::McpErrorCode::HandlerError,
                        dispatch::export_blocked_error_text());
  }
}

} // namespace prompts_detail
} // namespace godot_autopilot

#endif
