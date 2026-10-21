#ifndef GODOT_AUTOPILOT_INSTANCE_GUARD_HPP
#define GODOT_AUTOPILOT_INSTANCE_GUARD_HPP

#include <string>
#include <vector>

namespace godot_autopilot {

struct InstanceCandidate {
  std::string instance_id;
  std::string project_path;
  int pid{0};
  int port{0};
  std::string host;
  bool process_alive{false};
  bool port_in_use{false};
};

struct InstanceConflict {
  bool conflict{false};
  InstanceCandidate candidate;
};

class InstanceGuard {
public:
  static bool multi_instance_allowed();
  static std::string multi_instance_env_name();
  static bool same_project(const std::string &left, const std::string &right);
  static InstanceConflict
  evaluate(const std::vector<InstanceCandidate> &candidates,
           const std::string &current_project_path);
  static InstanceConflict detect(const std::string &current_project_path,
                                 int current_pid);
  static std::string conflict_message(const InstanceConflict &conflict);
};

bool process_is_alive(int pid);

bool port_is_in_use(int port, const std::string &host);

} // namespace godot_autopilot

#endif
