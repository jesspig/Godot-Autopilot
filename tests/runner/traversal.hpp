#pragma once
#include <mcp/JsonValue.hpp>
#include <string>
#include <vector>

#include "pipeline_executor.hpp"

namespace gda_test {

struct TraversalStats {
    size_t total = 0;
    size_t enumerated = 0;
    size_t excluded = 0;
    size_t candidates = 0;
    size_t passed = 0;
    size_t failed = 0;
    size_t exempt_errors = 0;
    std::vector<std::string> excluded_names;
    std::vector<std::string> warnings;
    bool baseline_matched = false;
    std::vector<std::string> baseline_diffs;
};

TraversalStats run_traversal(McpTestClient& client, const std::string& mode,
                             std::vector<StepResult>& out_steps);

} // namespace gda_test
