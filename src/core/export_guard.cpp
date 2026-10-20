#include "export_guard.hpp"

#include <mutex>
#include <vector>

namespace godot_autopilot {

namespace {
std::mutex g_registry_mutex;
std::vector<ExportGuard *> g_registry;
} // namespace

ExportState::Scope::Scope(ExportState &owner) : owner_(owner) {
  owner_.begin();
}

ExportState::Scope::~Scope() { reset(); }

void ExportState::Scope::release() { armed_ = false; }

void ExportState::Scope::reset() {
  if (!armed_) {
    return;
  }
  armed_ = false;
  owner_.end();
}

void ExportState::begin() { exporting_.store(true, std::memory_order_relaxed); }

void ExportState::end() { exporting_.store(false, std::memory_order_relaxed); }

bool ExportState::exporting() const {
  return exporting_.load(std::memory_order_relaxed);
}

ExportGuard::ExportGuard() { register_instance(); }

ExportGuard::~ExportGuard() { unregister_instance(); }

void ExportGuard::register_instance() {
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  g_registry.push_back(this);
}

void ExportGuard::unregister_instance() {
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  for (auto it = g_registry.begin(); it != g_registry.end(); ++it) {
    if (*it == this) {
      g_registry.erase(it);
      return;
    }
  }
}

bool ExportGuard::is_exporting() {
  std::lock_guard<std::mutex> lock(g_registry_mutex);
  for (const ExportGuard *guard : g_registry) {
    if (guard->state_.exporting()) {
      return true;
    }
  }
  return false;
}

void ExportGuard::_export_begin(const godot::PackedStringArray &, bool,
                                const godot::String &, uint32_t) {
  ExportState::Scope scope(state_);
  scope.release();
}

void ExportGuard::_export_end() { state_.end(); }

} // namespace godot_autopilot
