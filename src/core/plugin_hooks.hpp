#ifndef GODOT_AUTOPILOT_PLUGIN_HOOKS_HPP
#define GODOT_AUTOPILOT_PLUGIN_HOOKS_HPP

#include <functional>

namespace godot_autopilot {

void register_startup_hook(std::function<void()> hook);
void register_shutdown_hook(std::function<void()> hook);
void run_startup_hooks();
void run_shutdown_hooks();

} // namespace godot_autopilot

#endif
