#ifndef GODOT_AUTOPILOT_DOC_OPS_HPP
#define GODOT_AUTOPILOT_DOC_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace doc_ops {

godot_autopilot::util::JsonValue handle_get_class(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_search(const godot_autopilot::util::JsonValue &args);

} // namespace doc_ops
} // namespace godot_autopilot
#endif
