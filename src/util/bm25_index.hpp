#ifndef GODOT_AUTOPILOT_BM25_INDEX_HPP
#define GODOT_AUTOPILOT_BM25_INDEX_HPP

#include <cmath>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace godot_autopilot {

struct Bm25Result {
  std::string name;
  double score;
};

inline double bm25_average_document_length(
    const std::vector<size_t> &token_counts) {
  if (token_counts.empty()) {
    return 1.0;
  }
  double sum = 0.0;
  for (size_t count : token_counts) {
    sum += static_cast<double>(count);
  }
  const double avg = sum / static_cast<double>(token_counts.size());
  return avg == 0.0 ? 1.0 : avg;
}

class Bm25Index {
public:
  void add_entry(const std::string &name, const std::string &description,
                 const std::string &category,
                 const std::vector<std::string> &tags);

  struct SearchQuery {
    std::string text;
    std::optional<std::string> category;
    std::optional<std::vector<std::string>> tags;
    int max_results = 10;
  };
  std::vector<Bm25Result> search(const SearchQuery &query) const;
  std::vector<Bm25Result> search(const std::string &query,
                                 int max_results = 10) const;

  void clear();
  struct Entry {
    std::string name;
    std::string description;
    std::string category;
    std::vector<std::string> tags;
  };
  void replace_entries(const std::vector<Entry> &entries);
  [[nodiscard]] size_t size() const;

private:
  struct Document {
    std::string name;
    std::string description;
    std::string category;
    std::vector<std::string> tags;
    std::vector<std::string> tokens;
  };

  std::vector<std::string> tokenize(const std::string &text) const;
  double compute_bm25(const std::vector<std::string> &query_tokens,
                      const Document &doc, size_t total_docs,
                      const std::unordered_map<std::string, size_t> &df,
                      double avg_dl) const;

  std::vector<Document> docs_;
  mutable std::mutex mutex_;

  static constexpr double K1 = 1.5;
  static constexpr double B = 0.75;
};

} // namespace godot_autopilot

#endif
