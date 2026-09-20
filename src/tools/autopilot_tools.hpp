#ifndef GODOT_AUTOPILOT_AUTOPILOT_TOOLS_HPP
#define GODOT_AUTOPILOT_AUTOPILOT_TOOLS_HPP

#include <cstdint>

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot_autopilot {

class AutopilotTools : public godot::Object {
  GDCLASS(AutopilotTools, godot::Object)

public:
  AutopilotTools() = default;
  ~AutopilotTools() override;

  int64_t register_tool(const godot::Dictionary &definition,
                        const godot::Callable &callable);
  bool unregister_tool(int64_t handle);
  bool has_tool(const godot::String &name) const;
  godot::Array list_tools() const;
  godot::Dictionary get_tool(const godot::String &name) const;
  godot::Dictionary call_tool(const godot::String &name,
                              const godot::Dictionary &args);
  godot::Dictionary rescan(const godot::String &directory);

  bool is_enabled() const;
  void set_enabled(bool enabled);

protected:
  static void _bind_methods();
};

} // namespace godot_autopilot

#endif
