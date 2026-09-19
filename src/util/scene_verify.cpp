#include "util/scene_verify.hpp"

#include "util/error_util.hpp"

#include <algorithm>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/string.hpp>
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

void collect_memory_paths(godot::Node *root, std::vector<std::string> &out_paths) {
  if (!root)
    return;
  const std::string root_name = util::to_std(root->get_name());
  out_paths.push_back(root_name);
  const int child_count = root->get_child_count();
  for (int i = 0; i < child_count; i++) {
    godot::Node *child = root->get_child(i);
    if (!child)
      continue;
    std::vector<std::string> sub;
    collect_memory_paths(child, sub);
    for (std::string &p : sub) {
      out_paths.push_back(root_name + "/" + p);
    }
  }
}

int64_t count_memory_nodes(godot::Node *root) {
  if (!root)
    return 0;
  int64_t count = 1;
  const int child_count = root->get_child_count();
  for (int i = 0; i < child_count; i++) {
    count += count_memory_nodes(root->get_child(i));
  }
  return count;
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

VerifyResult compare_tree_with_text(godot::Node *root, const std::string &tscn_text,
                                    bool compute_hash) {
  VerifyResult result;
  std::vector<std::string> memory_paths;
  collect_memory_paths(root, memory_paths);
  result.memory_nodes = static_cast<int64_t>(memory_paths.size());
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
                 result.memory_nodes == result.disk_nodes && result.memory_nodes > 0;

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
