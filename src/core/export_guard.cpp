#include "export_guard.hpp"

#include <mutex>
#include <string>
#include <vector>

#include "monitor.hpp"
#include "trace_recorder.hpp"

namespace godot_autopilot {

namespace {
std::mutex g_registry_mutex;
std::vector<ExportState *> g_registry;
} // namespace

ExportState::Scope::Scope(ExportState &owner) : owner_(owner) {
  owner_.begin();
}

ExportState::Scope::~Scope() { owner_.end(); }

void ExportState::begin() {
  began_ns_.store(TraceRecorder::monotonic_now_ns(), std::memory_order_relaxed);
  exporting_.store(true, std::memory_order_release);
}

void ExportState::end() {
  exporting_.store(false, std::memory_order_release);
}

bool ExportState::exporting() const {
  return exporting_.load(std::memory_order_acquire);
}

bool ExportState::claim_stale_reset(int64_t now_ns) {
  if (!exporting_.load(std::memory_order_acquire)) {
    return false;
  }
  if (now_ns - began_ns_.load(std::memory_order_relaxed) <= kHoldBudgetNs) {
    return false;
  }
  return exporting_.exchange(false, std::memory_order_acq_rel);
}

int64_t ExportState::held_ms(int64_t now_ns) const {
  const int64_t held_ns = now_ns - began_ns_.load(std::memory_order_relaxed);
  return held_ns > 0 ? held_ns / 1000000 : 0;
}

ExportGuard::ExportGuard() { register_instance(); }

ExportGuard::~ExportGuard() { unregister_instance(); }

void ExportGuard::register_instance() {
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  g_registry.push_back(&state_);
}

void ExportGuard::unregister_instance() {
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  for (auto it = g_registry.begin(); it != g_registry.end(); ++it) {
    if (*it == &state_) {
      g_registry.erase(it);
      return;
    }
  }
}

ExportGuard::ScanResult
ExportGuard::scan_states(const std::vector<ExportState *> &states,
                        int64_t now_ns) {
  ScanResult result;
  for (ExportState *state : states) {
    if (state == nullptr) {
      continue;
    }
    if (state->claim_stale_reset(now_ns)) {
      ++result.stale_resets;
      const int64_t held = state->held_ms(now_ns);
      if (held > result.max_held_ms) {
        result.max_held_ms = held;
      }
      continue;
    }
    if (state->exporting()) {
      result.exporting = true;
    }
  }
  return result;
}

bool ExportGuard::is_exporting() {
  ScanResult result;
  {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    result = scan_states(g_registry, TraceRecorder::monotonic_now_ns());
  }
  if (result.stale_resets > 0) {
    const std::string attrs = monitor::build_attrs(
        {{"reset_count", std::to_string(result.stale_resets)},
         {"held_ms", std::to_string(result.max_held_ms)},
         {"budget_ms", std::to_string(ExportState::kHoldBudgetMs)}});
    monitor::security("export_flag_auto_reset", attrs);
  }
  return result.exporting;
}

void ExportGuard::_export_begin(const godot::PackedStringArray &, bool,
                                const godot::String &, uint32_t) {
  export_scope_.emplace(state_);
}

void ExportGuard::_export_end() {
  export_scope_.reset();
  state_.end();
}

} // namespace godot_autopilot
