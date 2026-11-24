#ifndef GODOT_AUTOPILOT_EXPORT_GUARD_HPP
#define GODOT_AUTOPILOT_EXPORT_GUARD_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include <godot_cpp/classes/editor_export_plugin.hpp>

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

  ExportState();
  ~ExportState();
  ExportState(const ExportState &) = delete;
  ExportState &operator=(const ExportState &) = delete;

  bool exporting() const;
  int64_t held_ms(int64_t now_ns) const;

private:
  friend class Scope;

  void acquire();
  void release();

  mutable std::mutex mutex_;
  size_t active_scopes_ = 0;
  int64_t began_ns_ = 0;
};

class ExportGuard : public godot::EditorExportPlugin {
  GDCLASS(ExportGuard, godot::EditorExportPlugin)

public:
  struct ScanResult {
    bool exporting = false;
    size_t active_states = 0;
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
  ExportState state_;
  std::vector<std::unique_ptr<ExportState::Scope>> export_scopes_;
};

} // namespace godot_autopilot

#endif
