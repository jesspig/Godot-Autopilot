#ifndef GODOT_AUTOPILOT_EXPORT_GUARD_HPP
#define GODOT_AUTOPILOT_EXPORT_GUARD_HPP

#include <atomic>

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
    void release();
    void reset();

  private:
    ExportState &owner_;
    bool armed_ = true;
  };

  void begin();
  void end();
  bool exporting() const;

private:
  std::atomic<bool> exporting_{false};
};

class ExportGuard : public godot::EditorExportPlugin {
  GDCLASS(ExportGuard, godot::EditorExportPlugin)

public:
  static bool is_exporting();
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
};

} // namespace godot_autopilot

#endif
