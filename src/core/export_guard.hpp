#ifndef GODOT_AUTOPILOT_EXPORT_GUARD_HPP
#define GODOT_AUTOPILOT_EXPORT_GUARD_HPP

#include <godot_cpp/classes/editor_export_plugin.hpp>

namespace godot_autopilot {

class ExportGuard : public godot::EditorExportPlugin {
  GDCLASS(ExportGuard, godot::EditorExportPlugin)

public:
  static bool is_exporting();
  void _export_begin(const godot::PackedStringArray &p_features,
                     bool p_is_debug, const godot::String &p_path,
                     uint32_t p_flags) override;
  void _export_end() override;

protected:
  static void _bind_methods() {}
};

} // namespace godot_autopilot

#endif
