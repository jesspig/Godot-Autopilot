#include "core/control/catalog_service.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gda {
namespace control {
namespace {

std::string catalog_lowered(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

bool catalog_contains(const std::string &haystack, const std::string &needle) {
  return !needle.empty() && haystack.find(needle) != std::string::npos;
}

double catalog_score_entry(const CatalogEntry &entry, const std::string &needle) {
  if (needle.empty()) {
    return 1.0;
  }
  const std::string name = catalog_lowered(entry.name);
  const std::string description = catalog_lowered(entry.description);
  const std::string category = catalog_lowered(entry.category);
  double score = 0.0;
  if (catalog_contains(name, needle)) {
    score += 4.0;
    if (name.compare(0, needle.size(), needle) == 0) {
      score += 2.0;
    }
  }
  if (catalog_contains(description, needle)) {
    score += 1.0;
  }
  if (catalog_contains(category, needle)) {
    score += 1.0;
  }
  for (const std::string &tag : entry.tags) {
    if (catalog_contains(catalog_lowered(tag), needle)) {
      score += 2.0;
    }
  }
  for (const std::string &alias : entry.aliases) {
    if (catalog_contains(catalog_lowered(alias), needle)) {
      score += 1.0;
    }
  }
  return score;
}

bool catalog_tags_match(const CatalogEntry &entry, const std::vector<std::string> &tags) {
  for (const std::string &wanted : tags) {
    bool found = false;
    for (const std::string &tag : entry.tags) {
      if (tag == wanted) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

} // namespace

void CatalogService::replace(std::vector<CatalogEntry> entries) {
  clear();
  for (const CatalogEntry &entry : entries) {
    add(entry);
  }
}

void CatalogService::add(const CatalogEntry &entry) {
  if (entry.name.empty()) {
    return;
  }
  entries_[entry.name] = entry;
  for (const std::string &alias : entry.aliases) {
    if (!alias.empty()) {
      aliases_[alias] = entry.name;
    }
  }
}

bool CatalogService::remove(const std::string &name) {
  const std::string canonical = resolve_alias(name);
  if (canonical.empty()) {
    return false;
  }
  const std::map<std::string, CatalogEntry>::iterator found =
      entries_.find(canonical);
  if (found == entries_.end()) {
    return false;
  }
  for (std::unordered_map<std::string, std::string>::iterator it =
           aliases_.begin();
       it != aliases_.end();) {
    if (it->second == canonical) {
      it = aliases_.erase(it);
      continue;
    }
    ++it;
  }
  entries_.erase(found);
  return true;
}

void CatalogService::clear() {
  entries_.clear();
  aliases_.clear();
}

std::size_t CatalogService::size() const { return entries_.size(); }

std::vector<CatalogEntry> CatalogService::list(const std::string &category,
                                               std::size_t limit,
                                               std::size_t offset) const {
  std::vector<CatalogEntry> out;
  std::size_t skipped = 0;
  for (const std::pair<const std::string, CatalogEntry> &item : entries_) {
    if (!category.empty() && item.second.category != category) {
      continue;
    }
    if (skipped < offset) {
      ++skipped;
      continue;
    }
    if (limit > 0 && out.size() >= limit) {
      break;
    }
    out.push_back(item.second);
  }
  return out;
}

std::vector<CatalogScore> CatalogService::search(
    const CatalogQuery &query) const {
  const std::string needle = catalog_lowered(query.text);
  std::vector<CatalogScore> scored;
  for (const std::pair<const std::string, CatalogEntry> &item : entries_) {
    if (!query.category.empty() && item.second.category != query.category) {
      continue;
    }
    if (!catalog_tags_match(item.second, query.tags)) {
      continue;
    }
    const double score = catalog_score_entry(item.second, needle);
    if (score <= 0.0) {
      continue;
    }
    CatalogScore result;
    result.entry = item.second;
    result.score = score;
    scored.push_back(result);
  }
  std::sort(scored.begin(), scored.end(),
            [](const CatalogScore &left, const CatalogScore &right) {
              if (left.score != right.score) {
                return left.score > right.score;
              }
              return left.entry.name < right.entry.name;
            });
  if (query.offset > 0) {
    if (query.offset >= scored.size()) {
      scored.clear();
    } else {
      scored.erase(scored.begin(),
                   scored.begin() + static_cast<std::ptrdiff_t>(query.offset));
    }
  }
  if (query.limit > 0 && scored.size() > query.limit) {
    scored.resize(query.limit);
  }
  return scored;
}

std::optional<CatalogEntry> CatalogService::describe(
    const std::string &name_or_alias) const {
  const std::string canonical = resolve_alias(name_or_alias);
  if (canonical.empty()) {
    return std::nullopt;
  }
  const std::map<std::string, CatalogEntry>::const_iterator found =
      entries_.find(canonical);
  if (found == entries_.end()) {
    return std::nullopt;
  }
  return found->second;
}

std::string CatalogService::resolve_alias(const std::string &name) const {
  const std::unordered_map<std::string, std::string>::const_iterator alias =
      aliases_.find(name);
  if (alias != aliases_.end()) {
    return alias->second;
  }
  return entries_.find(name) != entries_.end() ? name : std::string();
}

std::vector<std::string> CatalogService::categories() const {
  std::vector<std::string> out;
  for (const std::pair<const std::string, CatalogEntry> &item : entries_) {
    if (item.second.category.empty()) {
      continue;
    }
    if (std::find(out.begin(), out.end(), item.second.category) == out.end()) {
      out.push_back(item.second.category);
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

} // namespace control
} // namespace gda
