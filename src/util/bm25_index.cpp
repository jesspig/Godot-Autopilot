#include "util/bm25_index.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_set>

namespace {

constexpr char32_t kCjkBlocks[][2] = {{0x3400, 0x4DBF},
                                      {0x4E00, 0x9FFF},
                                      {0xF900, 0xFAFF}};

bool is_cjk_codepoint(char32_t cp) {
  for (const auto &block : kCjkBlocks) {
    if (cp >= block[0] && cp <= block[1]) {
      return true;
    }
  }
  return false;
}

size_t utf8_sequence_length(const std::string &text, size_t pos) {
  const unsigned char lead = static_cast<unsigned char>(text[pos]);
  size_t len = 0;
  if (lead >= 0xC2 && lead <= 0xDF) {
    len = 2;
  } else if (lead >= 0xE0 && lead <= 0xEF) {
    len = 3;
  } else if (lead >= 0xF0 && lead <= 0xF4) {
    len = 4;
  }
  if (len == 0 || pos + len > text.size()) {
    return 0;
  }
  for (size_t i = 1; i < len; ++i) {
    if ((static_cast<unsigned char>(text[pos + i]) & 0xC0) != 0x80) {
      return 0;
    }
  }
  return len;
}

char32_t decode_utf8_codepoint(const std::string &text, size_t pos,
                               size_t len) {
  static constexpr unsigned char kLeadMasks[] = {0x00, 0x7F, 0x1F, 0x0F, 0x07};
  char32_t cp = static_cast<unsigned char>(text[pos]) & kLeadMasks[len];
  for (size_t i = 1; i < len; ++i) {
    cp = (cp << 6) | (static_cast<unsigned char>(text[pos + i]) & 0x3F);
  }
  return cp;
}

} // namespace

namespace godot_autopilot {

std::vector<std::string> Bm25Index::tokenize(const std::string &text) const {
  std::vector<std::string> tokens;
  std::string ascii_word;
  std::vector<std::string> cjk_chars;

  auto flush_ascii_word = [&]() {
    if (!ascii_word.empty()) {
      tokens.push_back(std::move(ascii_word));
      ascii_word.clear();
    }
  };
  auto flush_cjk_bigrams = [&]() {
    for (size_t i = 0; i + 1 < cjk_chars.size(); ++i) {
      tokens.push_back(cjk_chars[i] + cjk_chars[i + 1]);
    }
    cjk_chars.clear();
  };

  size_t i = 0;
  while (i < text.size()) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c < 0x80) {
      flush_cjk_bigrams();
      if (std::isalnum(c)) {
        ascii_word.push_back(static_cast<char>(std::tolower(c)));
      } else {
        flush_ascii_word();
      }
      ++i;
      continue;
    }
    const size_t len = utf8_sequence_length(text, i);
    if (len == 0) {
      flush_ascii_word();
      flush_cjk_bigrams();
      ++i;
      continue;
    }
    if (is_cjk_codepoint(decode_utf8_codepoint(text, i, len))) {
      flush_ascii_word();
      cjk_chars.emplace_back(text, i, len);
    } else {
      flush_ascii_word();
      flush_cjk_bigrams();
    }
    i += len;
  }
  flush_ascii_word();
  flush_cjk_bigrams();
  return tokens;
}

void Bm25Index::add_entry(const std::string &name,
                          const std::string &description,
                          const std::string &category,
                          const std::vector<std::string> &tags) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string combined = name + " " + description + " " + category;
  for (const auto &tag : tags) {
    combined += " " + tag;
  }

  Document doc;
  doc.name = name;
  doc.description = description;
  doc.category = category;
  doc.tags = tags;
  doc.tokens = tokenize(combined);

  docs_.push_back(std::move(doc));
}

std::vector<Bm25Result> Bm25Index::search(const SearchQuery &query) const {
  std::lock_guard<std::mutex> lock(mutex_);

  if (query.max_results <= 0) {
    return {};
  }

  if (docs_.empty()) {
    return {};
  }

  if (query.text.empty()) {
    if (!query.category) {
      return {};
    }
    std::vector<Bm25Result> results;
    for (const auto &doc : docs_) {
      if (doc.category != *query.category) {
        continue;
      }
      results.push_back({doc.name, 0.0});
    }
    std::sort(results.begin(), results.end(),
              [](const Bm25Result &a, const Bm25Result &b) {
                return a.name < b.name;
              });
    if (static_cast<int>(results.size()) > query.max_results) {
      results.resize(static_cast<size_t>(query.max_results));
    }
    return results;
  }

  auto query_tokens = tokenize(query.text);
  if (query_tokens.empty()) {
    return {};
  }

  std::vector<size_t> candidates;
  for (size_t i = 0; i < docs_.size(); ++i) {
    const auto &doc = docs_[i];
    if (query.category && doc.category != *query.category) {
      continue;
    }
    if (query.tags) {
      bool all_found = true;
      for (const auto &tag : *query.tags) {
        if (std::find(doc.tags.begin(), doc.tags.end(), tag) ==
            doc.tags.end()) {
          all_found = false;
          break;
        }
      }
      if (!all_found) {
        continue;
      }
    }
    candidates.push_back(i);
  }

  if (candidates.empty()) {
    return {};
  }

  std::unordered_map<std::string, size_t> df;
  std::unordered_set<std::string> unique_query_tokens(query_tokens.begin(),
                                                      query_tokens.end());
  for (size_t idx : candidates) {
    const auto &tokens = docs_[idx].tokens;
    std::unordered_set<std::string> unique_doc_tokens(tokens.begin(),
                                                      tokens.end());
    for (const auto &qt : unique_query_tokens) {
      if (unique_doc_tokens.find(qt) != unique_doc_tokens.end()) {
        ++df[qt];
      }
    }
  }

  size_t N = candidates.size();

  double avg_dl = 0.0;
  for (const auto &d : docs_) {
    avg_dl += static_cast<double>(d.tokens.size());
  }
  avg_dl /= static_cast<double>(docs_.size());
  if (avg_dl == 0.0) {
    avg_dl = 1.0;
  }

  auto compact = [](const std::string &source) {
    std::string out;
    size_t i = 0;
    while (i < source.size()) {
      const unsigned char c = static_cast<unsigned char>(source[i]);
      if (c < 0x80) {
        if (std::isalnum(c)) {
          out.push_back(static_cast<char>(std::tolower(c)));
        }
        ++i;
        continue;
      }
      const size_t len = utf8_sequence_length(source, i);
      if (len > 0 &&
          is_cjk_codepoint(decode_utf8_codepoint(source, i, len))) {
        out.append(source, i, len);
      }
      i += len > 0 ? len : 1;
    }
    return out;
  };
  const std::string query_compact = compact(query.text);

  std::vector<Bm25Result> results;
  results.reserve(candidates.size());
  for (size_t idx : candidates) {
    double score = compute_bm25(query_tokens, docs_[idx], N, df, avg_dl);

    if (!query_compact.empty()) {
      const std::string name_compact = compact(docs_[idx].name);
      if (!name_compact.empty() &&
          name_compact.find(query_compact) != std::string::npos) {
        score += 2.0;
      }
    }
    if (score > 0.0) {
      results.push_back({docs_[idx].name, score});
    }
  }

  std::sort(results.begin(), results.end(),
            [](const Bm25Result &a, const Bm25Result &b) {
              if (a.score != b.score) {
                return a.score > b.score;
              }
              return a.name < b.name;
            });

  if (static_cast<int>(results.size()) > query.max_results) {
    results.resize(static_cast<size_t>(query.max_results));
  }

  return results;
}

void Bm25Index::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  docs_.clear();
}

void Bm25Index::replace_entries(const std::vector<Entry> &entries) {
  std::vector<Document> replacement;
  replacement.reserve(entries.size());
  for (const auto &entry : entries) {
    std::string combined = entry.name + " " + entry.description + " " + entry.category;
    for (const auto &tag : entry.tags) {
      combined += " " + tag;
    }
    Document doc;
    doc.name = entry.name;
    doc.description = entry.description;
    doc.category = entry.category;
    doc.tags = entry.tags;
    doc.tokens = tokenize(combined);
    replacement.push_back(std::move(doc));
  }
  std::lock_guard<std::mutex> lock(mutex_);
  docs_ = std::move(replacement);
}

size_t Bm25Index::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return docs_.size();
}

double Bm25Index::compute_bm25(
    const std::vector<std::string> &query_tokens, const Document &doc,
    size_t total_docs,
    const std::unordered_map<std::string, size_t> &df,
    double avg_dl) const {
  if (total_docs == 0) {
    return 0.0;
  }

  double doc_len = static_cast<double>(doc.tokens.size());

  std::unordered_map<std::string, int> tf;
  for (const auto &t : doc.tokens) {
    ++tf[t];
  }

  double score = 0.0;
  for (const auto &qt : query_tokens) {
    auto df_it = df.find(qt);
    if (df_it == df.end() || df_it->second == 0) {
      continue;
    }

    double idf = std::log(1.0 + (static_cast<double>(total_docs) -
                                 static_cast<double>(df_it->second) + 0.5) /
                                    (static_cast<double>(df_it->second) + 0.5));

    int f_i = 0;
    auto tf_it = tf.find(qt);
    if (tf_it != tf.end()) {
      f_i = tf_it->second;
    }
    if (f_i == 0) {
      continue;
    }

    double numerator = static_cast<double>(f_i) * (K1 + 1.0);
    double denominator =
        static_cast<double>(f_i) + K1 * (1.0 - B + B * doc_len / avg_dl);
    score += idf * numerator / denominator;
  }

  return score;
}

} // namespace godot_autopilot
