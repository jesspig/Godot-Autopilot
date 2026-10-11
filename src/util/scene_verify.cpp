#include "util/scene_verify.hpp"

#include "util/error_util.hpp"

#include <algorithm>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/signal.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <unordered_set>

namespace godot_autopilot {
namespace scene_verify {

uint64_t fnv1a_64(const std::string &data) {
  uint64_t hash = 14695981039346656037ULL;
  for (unsigned char c : data) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  return hash;
}

uint64_t hash_sorted_paths(const std::vector<std::string> &sorted_paths) {
  uint64_t hash = 14695981039346656037ULL;
  for (const std::string &p : sorted_paths) {
    for (unsigned char c : p) {
      hash ^= static_cast<uint64_t>(c);
      hash *= 1099511628211ULL;
    }
    hash ^= static_cast<uint64_t>('\n');
    hash *= 1099511628211ULL;
  }
  return hash;
}

namespace {

bool node_is_persisted(godot::Node *node, godot::Node *root) {
  return node == root || (node != nullptr && node->get_owner() == root);
}

void collect_paths_recursive(godot::Node *node, godot::Node *root,
                             const std::string &parent_path, bool node_persisted,
                             bool collect_persisted,
                             std::vector<std::string> &out_paths) {
  if (!node)
    return;
  const std::string name = util::to_std(node->get_name());
  const std::string path = parent_path.empty() ? name : parent_path + "/" + name;
  if (node_persisted == collect_persisted)
    out_paths.push_back(path);
  const int child_count = node->get_child_count();
  for (int i = 0; i < child_count; i++) {
    godot::Node *child = node->get_child(i);
    if (!child)
      continue;
    collect_paths_recursive(child, root, path, node_is_persisted(child, root),
                            collect_persisted, out_paths);
  }
}

bool connection_source_is_persisted(const godot::Dictionary &connection,
                                    godot::Node *root) {
  const godot::Signal signal = connection["signal"].operator godot::Signal();
  if (signal.is_null())
    return false;
  auto *source = godot::Object::cast_to<godot::Node>(signal.get_object());
  return node_is_persisted(source, root);
}

void count_connections_recursive(godot::Node *node, godot::Node *root,
                                 int64_t &out_persisted, int64_t &out_filtered) {
  if (!node)
    return;
  const bool target_persisted = node_is_persisted(node, root);
  const godot::TypedArray<godot::Dictionary> connections =
      node->get_incoming_connections();
  const int64_t connection_count = connections.size();
  for (int64_t i = 0; i < connection_count; i++) {
    const godot::Dictionary connection = connections[i];
    if (target_persisted && connection_source_is_persisted(connection, root))
      out_persisted++;
    else
      out_filtered++;
  }
  const int child_count = node->get_child_count();
  for (int i = 0; i < child_count; i++)
    count_connections_recursive(node->get_child(i), root, out_persisted,
                                out_filtered);
}

} // namespace

void collect_memory_paths(godot::Node *root, std::vector<std::string> &out_paths) {
  if (!root)
    return;
  collect_paths_recursive(root, root, "", true, true, out_paths);
}

void collect_inherited_paths(godot::Node *root,
                             std::vector<std::string> &out_paths) {
  if (!root)
    return;
  collect_paths_recursive(root, root, "", true, false, out_paths);
}

int64_t count_memory_connections(godot::Node *root, int64_t &out_filtered) {
  out_filtered = 0;
  if (!root)
    return 0;
  int64_t persisted = 0;
  count_connections_recursive(root, root, persisted, out_filtered);
  return persisted;
}

namespace {

std::string trim_left(const std::string &s) {
  std::size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r'))
    i++;
  return s.substr(i);
}

bool extract_attr(const std::string &section, const std::string &attr, std::string &out) {
  const std::string key = attr + "=\"";
  std::size_t pos = section.find(key);
  if (pos == std::string::npos)
    return false;
  pos += key.size();
  std::size_t end = section.find('"', pos);
  if (end == std::string::npos)
    return false;
  out = section.substr(pos, end - pos);
  return true;
}

} // namespace

std::vector<std::string> parse_tscn_paths(const std::string &tscn_text, int64_t &out_count) {
  std::vector<std::string> paths;
  out_count = 0;
  std::string root_name;
  std::size_t pos = 0;
  while (pos < tscn_text.size()) {
    std::size_t line_end = tscn_text.find('\n', pos);
    std::string line = tscn_text.substr(pos, line_end == std::string::npos
                                                   ? std::string::npos
                                                   : line_end - pos);
    pos = (line_end == std::string::npos) ? tscn_text.size() : line_end + 1;
    std::string t = trim_left(line);
    if (t.compare(0, 6, "[node ") != 0)
      continue;
    out_count++;
    std::string name;
    if (!extract_attr(t, "name", name) || name.empty())
      continue;
    std::string parent;
    const bool has_parent = extract_attr(t, "parent", parent);
    if (!has_parent) {
      root_name = name;
      paths.push_back(name);
    } else if (parent == ".") {
      paths.push_back(root_name.empty() ? name : root_name + "/" + name);
    } else {
      paths.push_back(root_name.empty() ? parent + "/" + name
                                        : root_name + "/" + parent + "/" + name);
    }
  }
  return paths;
}

int64_t count_tscn_connections(const std::string &tscn_text) {
  int64_t count = 0;
  std::size_t pos = 0;
  while (pos < tscn_text.size()) {
    std::size_t line_end = tscn_text.find('\n', pos);
    std::string line = tscn_text.substr(pos, line_end == std::string::npos
                                                   ? std::string::npos
                                                   : line_end - pos);
    pos = (line_end == std::string::npos) ? tscn_text.size() : line_end + 1;
    if (trim_left(line).compare(0, 12, "[connection ") == 0)
      count++;
  }
  return count;
}

VerifyResult compare_tree_with_text(godot::Node *root, const std::string &tscn_text,
                                    bool compute_hash) {
  VerifyResult result;
  std::vector<std::string> memory_paths;
  collect_memory_paths(root, memory_paths);
  std::vector<std::string> inherited_paths;
  collect_inherited_paths(root, inherited_paths);
  result.memory_nodes = static_cast<int64_t>(memory_paths.size());
  result.inherited_nodes = static_cast<int64_t>(inherited_paths.size());
  const std::size_t inherited_keep =
      std::min(inherited_paths.size(), kMaxMissingPaths);
  for (std::size_t i = 0; i < inherited_keep; i++)
    result.inherited_paths.push_back(inherited_paths[i]);
  result.inherited_truncated = inherited_paths.size() > kMaxMissingPaths;
  result.memory_connections =
      count_memory_connections(root, result.inherited_connections);
  result.disk_connections = count_tscn_connections(tscn_text);
  std::vector<std::string> disk_paths = parse_tscn_paths(tscn_text, result.disk_nodes);

  std::unordered_set<std::string> disk_set(disk_paths.begin(), disk_paths.end());
  for (const std::string &p : memory_paths) {
    if (disk_set.find(p) == disk_set.end()) {
      if (result.missing_paths.size() < kMaxMissingPaths)
        result.missing_paths.push_back(p);
      else
        result.missing_truncated = true;
    }
  }
  result.match = result.missing_paths.empty() && !result.missing_truncated &&
                 result.memory_nodes == result.disk_nodes && result.memory_nodes > 0 &&
                 result.memory_connections == result.disk_connections;

  if (compute_hash) {
    std::vector<std::string> mem_sorted = memory_paths;
    std::vector<std::string> disk_sorted = disk_paths;
    std::sort(mem_sorted.begin(), mem_sorted.end());
    std::sort(disk_sorted.begin(), disk_sorted.end());
    result.memory_hash = hash_sorted_paths(mem_sorted);
    result.disk_hash = hash_sorted_paths(disk_sorted);
    result.hash_computed = true;
  }
  return result;
}

std::string read_text_file(const std::string &res_path, bool &ok) {
  ok = false;
  godot::Ref<godot::FileAccess> fa = godot::FileAccess::open(
      godot::String(res_path.c_str()), godot::FileAccess::READ);
  if (fa.is_null())
    return "";
  ok = true;
  std::string text = util::to_std(fa->get_as_text());
  fa->close();
  return text;
}

} // namespace scene_verify
} // namespace godot_autopilot
