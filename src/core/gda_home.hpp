#ifndef GODOT_AUTOPILOT_GDA_HOME_HPP
#define GODOT_AUTOPILOT_GDA_HOME_HPP

#include <functional>
#include <string>

namespace godot_autopilot {

class HomeResolver {
public:
  enum class HostPlatform { kWindows, kPosix };

  using DirectoryProbe = std::function<bool(const std::string &)>;
  using DataDirProvider = std::function<std::string()>;

  static std::string resolve(const std::string &env_home,
                             const std::string &env_userprofile,
                             const std::string &env_appdata,
                             const std::string &data_dir);
  static std::string resolve(const std::string &env_home,
                             const std::string &env_userprofile,
                             const std::string &env_appdata,
                             const std::string &data_dir,
                             HostPlatform platform,
                             const DirectoryProbe &probe);
  static std::string gda_home();
  static bool validate(const std::string &path);
  static bool validate_shape(const std::string &path);
  static bool validate_exists(const std::string &path);
  static HostPlatform current_platform();
  static void set_data_dir_provider(DataDirProvider provider);

private:
  static DataDirProvider &data_dir_provider();
};

} // namespace godot_autopilot

#endif
