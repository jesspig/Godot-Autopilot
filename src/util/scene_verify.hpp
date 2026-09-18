#ifndef GODOT_AUTOPILOT_SCENE_VERIFY_HPP
#define GODOT_AUTOPILOT_SCENE_VERIFY_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace godot {
class Node;
}

namespace godot_autopilot {
namespace scene_verify {

constexpr std::size_t kMaxMissingPaths = 50;

// FNV-1a 64 位哈希（仓内无现成哈希工具时使用，无外部依赖）。
uint64_t fnv1a_64(const std::string &data);
uint64_t hash_sorted_paths(const std::vector<std::string> &sorted_paths);

// 以场景根名为起点收集内存树相对路径（根记为根名，子记为 parent_rel/child）。
void collect_memory_paths(godot::Node *root, std::vector<std::string> &out_paths);
int64_t count_memory_nodes(godot::Node *root);

// 解析 .tscn 文本中的 [node name=".." parent=".."] 节，重建相对路径清单。
// out_count 为 [node ...] 节总数（含根）。
std::vector<std::string> parse_tscn_paths(const std::string &tscn_text, int64_t &out_count);

struct VerifyResult {
  int64_t memory_nodes = 0;
  int64_t disk_nodes = 0;
  bool match = false;
  std::vector<std::string> missing_paths;
  bool missing_truncated = false;
  uint64_t memory_hash = 0;
  uint64_t disk_hash = 0;
  bool hash_computed = false;
};

// 只读比对：内存树 vs 落盘文本。compute_hash 为真时才计算哈希（默认轻量计数）。
VerifyResult compare_tree_with_text(godot::Node *root, const std::string &tscn_text,
                                    bool compute_hash);

// 经 FileAccess 读取 res:// 文本文件，失败时 ok=false。
std::string read_text_file(const std::string &res_path, bool &ok);

} // namespace scene_verify
} // namespace godot_autopilot

#endif
