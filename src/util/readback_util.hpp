#ifndef GODOT_AUTOPILOT_READBACK_UTIL_HPP
#define GODOT_AUTOPILOT_READBACK_UTIL_HPP

#include <godot_cpp/variant/variant.hpp>
#include <string>

namespace godot_autopilot {
namespace util {

enum class ReadbackStatus { MATCHED, REJECTED, CONVERTED, NOOP };

ReadbackStatus check_readback(const godot::Variant &expected_value,
                              const godot::Variant &old_value,
                              const godot::Variant &actual_value,
                              std::string &out_detail,
                              bool type_sensitive = false);

} // namespace util
} // namespace godot_autopilot

#endif
