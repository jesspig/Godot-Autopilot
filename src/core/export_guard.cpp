#include "export_guard.hpp"

#include <atomic>

namespace godot_autopilot {

namespace {
std::atomic<bool> g_exporting{false};
} // namespace

bool ExportGuard::is_exporting() {
  return g_exporting.load(std::memory_order_relaxed);
}

void ExportGuard::_export_begin(const godot::PackedStringArray &, bool,
                                const godot::String &, uint32_t) {
  g_exporting.store(true, std::memory_order_relaxed);
}

void ExportGuard::_export_end() {
  g_exporting.store(false, std::memory_order_relaxed);
}

} // namespace godot_autopilot
