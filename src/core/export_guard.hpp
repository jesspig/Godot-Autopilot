#ifndef GODOT_AUTOPILOT_EXPORT_GUARD_HPP
#define GODOT_AUTOPILOT_EXPORT_GUARD_HPP

#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

#include <godot_cpp/classes/editor_export_plugin.hpp>

#include "config.hpp"

namespace godot_autopilot {

class ExportState {
public:
  class Scope {
  public:
    explicit Scope(ExportState &owner);
    ~Scope();
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

  private:
    ExportState &owner_;
  };

  static constexpr int64_t kHoldBudgetMs = 1800000;
  static constexpr int64_t kHoldBudgetNs = kHoldBudgetMs * 1000000LL;

  void begin();
  void end();
  bool exporting() const;
  bool claim_stale_reset(int64_t now_ns);
  int64_t held_ms(int64_t now_ns) const;

private:
  std::atomic<bool> exporting_{false};
  std::atomic<int64_t> began_ns_{0};
};

static_assert(ExportState::kHoldBudgetMs > GDA_TRANSPORT_TIMEOUT_MS,
              "the export hold budget must exceed every bounded wait in the "
              "process, otherwise a legitimately long export is mistaken for a "
              "leaked export flag and the entry points unblock mid export");
static_assert(ExportState::kHoldBudgetMs > GDA_MAX_TIMEOUT_MS,
              "the export hold budget must exceed the generic tool timeout");
static_assert(ExportState::kHoldBudgetMs * 1000000LL / 1000000LL ==
                  ExportState::kHoldBudgetMs,
              "the export hold budget must convert to nanoseconds without "
              "overflowing the monotonic clock unit");

class ExportGuard : public godot::EditorExportPlugin {
  GDCLASS(ExportGuard, godot::EditorExportPlugin)

public:
  struct ScanResult {
    bool exporting = false;
    int stale_resets = 0;
    int64_t max_held_ms = 0;
  };

  static bool is_exporting();
  static ScanResult scan_states(const std::vector<ExportState *> &states,
                                int64_t now_ns);
  void _export_begin(const godot::PackedStringArray &p_features,
                     bool p_is_debug, const godot::String &p_path,
                     uint32_t p_flags) override;
  void _export_end() override;

  ExportGuard();
  ~ExportGuard();

protected:
  static void _bind_methods() {}

private:
  void register_instance();
  void unregister_instance();

  ExportState state_;
  std::optional<ExportState::Scope> export_scope_;
};

} // namespace godot_autopilot

#endif
