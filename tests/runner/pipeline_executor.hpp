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
};

struct FileResult {
    std::string name;
    std::vector<StepResult> steps;
    bool passed = false;
    double duration_ms = 0.0;
    std::string fatal_error;
    size_t call_count = 0;
    size_t call_success = 0;
};

FileResult run_pipeline(const TestCase& tc, GodotProcess* proc,
                        McpTestClient* external_client = nullptr);

}
