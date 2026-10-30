#pragma once
#include "godot_process.hpp"
#include "config_loader.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace gda_test {

class McpTestClient;

struct StepResult {
    std::string step_id;
    std::string tool;
    bool passed = false;
    std::string detail;
    double duration_ms = 0.0;
    bool soft_failed = false;
    std::string soft_reason;
};

struct TraversalSummary {
    std::string mode;
    size_t enumerated = 0;
    size_t excluded = 0;
    size_t candidates = 0;
    size_t warnings = 0;
    size_t total_calls = 0;
    size_t failed_steps = 0;
    size_t exempt_errors = 0;
    bool baseline_matched = false;
    std::vector<std::string> baseline_diffs;
};

struct FileResult {
    std::string name;
    std::vector<StepResult> steps;
    bool passed = false;
    double duration_ms = 0.0;
    std::string fatal_error;
    size_t call_count = 0;
    size_t call_success = 0;
    bool baseline_matched = true;
    std::vector<TraversalSummary> traversals;
};

FileResult run_pipeline(const TestCase& tc, GodotProcess* proc,
                        McpTestClient* external_client = nullptr);

}
