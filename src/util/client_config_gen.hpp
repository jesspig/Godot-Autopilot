#ifndef GODOT_AUTOPILOT_CLIENT_CONFIG_GEN_HPP
#define GODOT_AUTOPILOT_CLIENT_CONFIG_GEN_HPP

#include <string>

namespace godot_autopilot::client_config_gen {

inline constexpr const char *kServerName = "godot-autopilot";

enum class ClientId {
  OpenCode,
  ClaudeCode,
  Codex,
  Cursor,
  Copilot,
  Trae,
  Qoder,
  WorkBuddy,
  COUNT
};

const char *display_name(ClientId id);
const char *file_path(ClientId id);
const char *description(ClientId id);
std::string render_config(ClientId id, int port);

struct MergeResult {
  enum class Status { Merged, Unparsable };
  Status status;
  std::string content;
};

MergeResult merge_json_config(ClientId id, int port,
                              const std::string &existing);

struct TomlMergeResult {
  enum class Status { Merged, AlreadyConfigured };
  Status status;
  std::string content;
};

TomlMergeResult merge_toml_config(int port, const std::string &existing);

} // namespace godot_autopilot::client_config_gen

#endif
