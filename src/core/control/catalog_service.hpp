#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gda {
namespace control {

struct CatalogEntry {
  std::string name;
  std::string description;
  std::string category;
  std::vector<std::string> tags;
  std::vector<std::string> aliases;
  bool mutating = false;
  bool high_risk = false;
};

struct CatalogScore {
  CatalogEntry entry;
  double score = 0.0;
};

struct CatalogQuery {
  std::string text;
  std::string category;
  std::vector<std::string> tags;
  std::size_t limit = 20;
  std::size_t offset = 0;
};

class CatalogService {
public:
  void replace(std::vector<CatalogEntry> entries);
  void add(const CatalogEntry &entry);
  bool remove(const std::string &name);
  void clear();

  std::size_t size() const;
  std::vector<CatalogEntry> list(const std::string &category,
                                 std::size_t limit, std::size_t offset) const;
  std::vector<CatalogScore> search(const CatalogQuery &query) const;
  std::optional<CatalogEntry> describe(const std::string &name_or_alias) const;
  std::string resolve_alias(const std::string &name) const;
  std::vector<std::string> categories() const;

private:
  std::map<std::string, CatalogEntry> entries_;
  std::unordered_map<std::string, std::string> aliases_;
};

} // namespace control
} // namespace gda
