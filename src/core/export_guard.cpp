#include "export_guard.hpp"

#include <mutex>
#include <vector>

#include "core/observability/trace_recorder.hpp"

namespace godot_autopilot {

namespace {
std::mutex g_registry_mutex;
std::vector<ExportState *> g_registry;

void register_state(ExportState *state) {
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  g_registry.push_back(state);
}

void unregister_state(ExportState *state) {
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  for (auto it = g_registry.begin(); it != g_registry.end(); ++it) {
    if (*it == state) {
      g_registry.erase(it);
      return;
    }
  }
}
} // namespace

ExportState::ExportState() { register_state(this); }

ExportState::~ExportState() { unregister_state(this); }

ExportState::Scope::Scope(ExportState &owner) : owner_(owner) {
  owner_.acquire();
}

ExportState::Scope::~Scope() { owner_.release(); }

void ExportState::acquire() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (active_scopes_ == 0) {
    began_ns_ = TraceRecorder::monotonic_now_ns();
  }
  ++active_scopes_;
}

void ExportState::release() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (active_scopes_ > 0) {
    --active_scopes_;
  }
}

bool ExportState::exporting() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return active_scopes_ > 0;
}

int64_t ExportState::held_ms(int64_t now_ns) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (active_scopes_ == 0) {
    return 0;
  }
  const int64_t held_ns = now_ns - began_ns_;
  return held_ns > 0 ? held_ns / 1000000 : 0;
}

ExportGuard::ExportGuard() = default;

ExportGuard::~ExportGuard() { export_scopes_.clear(); }

ExportGuard::ScanResult
ExportGuard::scan_states(const std::vector<ExportState *> &states,
                         int64_t now_ns) {
  ScanResult result;
  for (ExportState *state : states) {
    if (state == nullptr || !state->exporting()) {
      continue;
    }
    result.exporting = true;
    ++result.active_states;
    const int64_t held = state->held_ms(now_ns);
    if (held > result.max_held_ms) {
      result.max_held_ms = held;
    }
  }
  return result;
}

bool ExportGuard::is_exporting() {
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  return scan_states(g_registry, TraceRecorder::monotonic_now_ns()).exporting;
}

void ExportGuard::_export_begin(const godot::PackedStringArray &, bool,
                                 const godot::String &, uint32_t) {
  export_scopes_.push_back(std::make_unique<ExportState::Scope>(state_));
}

void ExportGuard::_export_end() {
  if (!export_scopes_.empty()) {
    export_scopes_.pop_back();
  }
}

} // namespace godot_autopilot
