#include "util/bm25_index.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace godot_self_driving {

std::vector<std::string> Bm25Index::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::string current;
    for (unsigned char c : text) {
        if (std::isalnum(c)) {
            current.push_back(static_cast<char>(std::tolower(c)));
        } else {
            if (!current.empty()) {
                tokens.push_back(std::move(current));
                current.clear();
            }
        }
    }
    if (!current.empty()) {
        tokens.push_back(std::move(current));
    }
    return tokens;
}

void Bm25Index::add_entry(const std::string& name, const std::string& description,
                           const std::string& category, const std::vector<std::string>& tags) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string combined = name + " " + description + " " + category;
    for (const auto& tag : tags) {
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

std::vector<Bm25Result> Bm25Index::search(const SearchQuery& query) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (docs_.empty()) {
        return {};
    }

    if (query.text.empty()) {
        if (!query.category) {
            return {};
        }
        std::vector<Bm25Result> results;
        for (const auto& doc : docs_) {
            if (doc.category != *query.category) {
                continue;
            }
            results.push_back({doc.name, 0.0});
        }
        return results;
    }

    auto query_tokens = tokenize(query.text);
    if (query_tokens.empty()) {
        return {};
    }

    std::vector<size_t> candidates;
    for (size_t i = 0; i < docs_.size(); ++i) {
        const auto& doc = docs_[i];
        if (query.category && doc.category != *query.category) {
            continue;
        }
        if (query.tags) {
            bool all_found = true;
            for (const auto& tag : *query.tags) {
                if (std::find(doc.tags.begin(), doc.tags.end(), tag) == doc.tags.end()) {
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
    for (const auto& qt : query_tokens) {
        size_t count = 0;
        for (size_t idx : candidates) {
            const auto& tokens = docs_[idx].tokens;
            if (std::find(tokens.begin(), tokens.end(), qt) != tokens.end()) {
                ++count;
            }
        }
        df[qt] = count;
    }

    size_t N = candidates.size();

    std::vector<Bm25Result> results;
    results.reserve(candidates.size());
    for (size_t idx : candidates) {
        double score = compute_bm25(query_tokens, docs_[idx], N, df);
        if (score > 0.0) {
            results.push_back({docs_[idx].name, score});
        }
    }

    std::sort(results.begin(), results.end(),
              [](const Bm25Result& a, const Bm25Result& b) { return a.score > b.score; });

    if (static_cast<int>(results.size()) > query.max_results) {
        results.resize(static_cast<size_t>(query.max_results));
    }

    return results;
}

std::vector<Bm25Result> Bm25Index::search(const std::string& query, int max_results) const {
    SearchQuery q;
    q.text = query;
    q.max_results = max_results;
    return search(q);
}

void Bm25Index::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    docs_.clear();
}

size_t Bm25Index::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return docs_.size();
}

double Bm25Index::compute_bm25(const std::vector<std::string>& query_tokens, const Document& doc,
                                size_t total_docs,
                                const std::unordered_map<std::string, size_t>& df) const {
    if (total_docs == 0) {
        return 0.0;
    }

    double avg_dl = 0.0;
    for (const auto& d : docs_) {
        avg_dl += static_cast<double>(d.tokens.size());
    }
    avg_dl = docs_.empty() ? 1.0 : avg_dl / static_cast<double>(docs_.size());
    if (avg_dl == 0.0) {
        avg_dl = 1.0;
    }

    double doc_len = static_cast<double>(doc.tokens.size());

    std::unordered_map<std::string, int> tf;
    for (const auto& t : doc.tokens) {
        ++tf[t];
    }

    double score = 0.0;
    for (const auto& qt : query_tokens) {
        auto df_it = df.find(qt);
        if (df_it == df.end() || df_it->second == 0) {
            continue;
        }

        double idf = std::log(1.0 + (static_cast<double>(total_docs) - static_cast<double>(df_it->second) + 0.5)
                                   / (static_cast<double>(df_it->second) + 0.5));

        int f_i = 0;
        auto tf_it = tf.find(qt);
        if (tf_it != tf.end()) {
            f_i = tf_it->second;
        }
        if (f_i == 0) {
            continue;
        }

        double numerator = static_cast<double>(f_i) * (K1 + 1.0);
        double denominator = static_cast<double>(f_i)
                           + K1 * (1.0 - B + B * doc_len / avg_dl);
        score += idf * numerator / denominator;
    }

    return score;
}

} // namespace godot_self_driving
