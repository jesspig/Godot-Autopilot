#ifndef GODOT_AUTOPILOT_DOTNET_ENV_HPP
#define GODOT_AUTOPILOT_DOTNET_ENV_HPP

#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/core/version.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

namespace godot_autopilot {
namespace dotnet_env {

struct DotnetProbe {
  bool mono_feature = false;
  bool dotnet_feature = false;
  bool csharp_script_class = false;
  int engine_major = 0;
  int engine_minor = 0;
};

inline bool feature_name_is_dotnet(int engine_major, int engine_minor) {
  if (engine_major <= 0)
    return false;
  if (engine_major > 4)
    return true;
  if (engine_major < 4)
    return false;
  return engine_minor >= 4;
}

inline bool is_dotnet_environment(const DotnetProbe &probe) {
  if (probe.csharp_script_class)
    return true;
  if (probe.engine_major <= 0)
    return probe.mono_feature || probe.dotnet_feature;
  if (feature_name_is_dotnet(probe.engine_major, probe.engine_minor))
    return probe.dotnet_feature;
  return probe.mono_feature;
}

inline DotnetProbe probe_current() {
  DotnetProbe probe;
  probe.engine_major = GODOT_VERSION_MAJOR;
  probe.engine_minor = GODOT_VERSION_MINOR;
  if (godot::OS *os = godot::OS::get_singleton()) {
    probe.mono_feature = os->has_feature(godot::String("mono"));
    probe.dotnet_feature = os->has_feature(godot::String("dotnet"));
  }
  if (godot::ClassDBSingleton *cdbs =
          godot::ClassDBSingleton::get_singleton()) {
    probe.csharp_script_class =
        cdbs->class_exists(godot::StringName("CSharpScript"));
  }
  if (godot::Engine *engine = godot::Engine::get_singleton()) {
    godot::Dictionary info = engine->get_version_info();
    if (info.has("major"))
      probe.engine_major =
          static_cast<int>(static_cast<int64_t>(info["major"]));
    if (info.has("minor"))
      probe.engine_minor =
          static_cast<int>(static_cast<int64_t>(info["minor"]));
  }
  return probe;
}

} // namespace dotnet_env
} // namespace godot_autopilot

#endif