#ifndef GDA_TEST_SUPPORT_TOOL_CATALOG_BASELINE_HPP
#define GDA_TEST_SUPPORT_TOOL_CATALOG_BASELINE_HPP

#include <cstddef>

namespace gda_test {

struct ToolCatalogBaseline {
  std::size_t expected_catalog_total = 0;
  std::size_t expected_non_empty = 0;
  std::size_t expected_empty = 0;
  std::size_t expected_alias_count = 0;
};

const ToolCatalogBaseline &load_tool_catalog_baseline();

} // namespace gda_test

#endif
