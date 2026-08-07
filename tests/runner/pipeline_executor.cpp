#include "pipeline_executor.hpp"

#include "assert_engine.hpp"
#include "integration/mcp_test_client.hpp"
#include "traversal.hpp"

#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <mcp/JsonValue.hpp>
#include <string>
#include <utility>

namespace gsd_test {
namespace {

constexpr size_t CRASH_LOG_LIMIT = 2000;
constexpr char DEFAULT_TRAVERSE_MODE[] = "empty_args";

enum class StepOutcome { ok, failed, crashed };

struct StepBatch {
    StepOutcome outcome = StepOutcome::ok;
    std::vector<StepResult> steps;
};

bool expect_empty(const StepExpect& expect) {
    return expect.has_keys.empty() && expect.field_checks.empty();
}

std::string crash_log(GodotProcess* proc) {
    std::string logs = proc ? proc->capture_logs() : "";
    if (logs.size() > CRASH_LOG_LIMIT) {
        logs.resize(CRASH_LOG_LIMIT);
    }
    return "editor process not alive. Logs: " + logs;
}

mcp::JsonValue call_tool_envelope(const Step& step) {
    mcp::JsonValue envelope(mcp::JsonValue::object_tag);
    envelope["name"] = mcp::JsonValue(step.tool);
    if (step.args.IsObject()) {
        envelope["arguments"] = step.args;
    }
    return envelope;
}

std::string traverse_tool_name(const std::string& mode) {
    return "<traverse:" + mode + ">";
}

StepResult run_single_step(const Step& step, GodotProcess* proc,
                           McpTestClient& client, bool check_expect) {
    StepResult result;
    result.step_id = step.id.empty() ? step.tool : step.id;
    result.tool = step.tool;
    const auto step_start = std::chrono::steady_clock::now();
    try {
        const std::string response =
            client.call_tool("call_tool", call_tool_envelope(step).Dump());
        if (proc && !proc->alive()) {
            result.passed = false;
            result.detail = "editor process died during call";
        } else if (check_expect && !expect_empty(step.expect)) {
            try {
                mcp::JsonValue parsed = mcp::JsonValue::Parse(response);
                result.passed = check_expectations(parsed, step.expect, result.detail);
            } catch (const std::exception& e) {
                result.passed = false;
                result.detail = std::string("response not valid JSON: ") + e.what();
            }
            if (!result.passed && result.detail.empty()) {
                result.detail = "expectations not matched";
            }
        } else {
            result.passed = !client.last_call_was_error();
            result.detail = result.passed ? "ok" : "tool returned error";
        }
    } catch (const std::exception& e) {
        result.passed = false;
        result.detail = e.what();
    } catch (...) {
        result.passed = false;
        result.detail = "unknown exception";
    }
    result.duration_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - step_start).count();
    return result;
}

StepBatch exec_step(const Step& step, GodotProcess* proc, McpTestClient& client,
                    bool check_expect) {
    StepBatch batch;
    if (!step.tool.empty()) {
        batch.steps.push_back(run_single_step(step, proc, client, check_expect));
        if (proc && !proc->alive()) {
            batch.outcome = StepOutcome::crashed;
        } else if (!batch.steps.back().passed) {
            batch.outcome = StepOutcome::failed;
        }
        return batch;
    }

    const std::string mode = step.traverse_mode.empty() ? DEFAULT_TRAVERSE_MODE
                                                        : step.traverse_mode;
    try {
        auto stats = run_traversal(client, mode, batch.steps);
        (void)stats;
    } catch (const std::exception& e) {
        StepResult sr;
        sr.step_id = step.id.empty() ? traverse_tool_name(mode) : step.id;
        sr.tool = traverse_tool_name(mode);
        sr.detail = e.what();
        batch.steps.push_back(std::move(sr));
    }
    if (proc && !proc->alive()) {
        batch.outcome = StepOutcome::crashed;
        return batch;
    }
    for (const StepResult& sr : batch.steps) {
        if (!sr.passed) {
            batch.outcome = StepOutcome::failed;
            break;
        }
    }
    return batch;
}

void merge_batch(FileResult& result, StepBatch& batch) {
    for (StepResult& sr : batch.steps) {
        ++result.call_count;
        if (sr.passed) {
            ++result.call_success;
        }
        result.steps.push_back(std::move(sr));
    }
}

std::string before_all_error(const std::vector<StepResult>& steps) {
    for (auto it = steps.rbegin(); it != steps.rend(); ++it) {
        if (!it->passed) {
            return "before_all step '" + it->step_id + "' failed: " + it->detail;
        }
    }
    return "before_all step failed";
}

FileResult run_pipeline_inner(const TestCase& tc, GodotProcess* proc,
                              McpTestClient* external_client) {
    FileResult result;
    result.name = tc.name;

    std::unique_ptr<McpTestClient> owned_client;
    McpTestClient* client = external_client;
    if (client == nullptr) {
        if (proc == nullptr) {
            result.fatal_error = "no process and no external client";
            return result;
        }
        owned_client = std::make_unique<McpTestClient>(proc->port());
        if (!owned_client->connect()) {
            result.fatal_error = "MCP connect failed";
            return result;
        }
        client = owned_client.get();
    }

    bool all_steps_passed = true;
    const bool fail_fast = tc.on_failure != "continue";

    for (const Step& step : tc.before_all) {
        StepBatch batch = exec_step(step, proc, *client, false);
        merge_batch(result, batch);
        if (batch.outcome == StepOutcome::crashed) {
            result.fatal_error = crash_log(proc);
            all_steps_passed = false;
            break;
        }
        if (batch.outcome == StepOutcome::failed) {
            result.fatal_error = before_all_error(batch.steps);
            all_steps_passed = false;
            break;
        }
    }

    if (result.fatal_error.empty()) {
        bool abort_steps = false;
        for (const Step& step : tc.steps) {
            if (abort_steps) {
                break;
            }
            StepBatch batch = exec_step(step, proc, *client, true);
            merge_batch(result, batch);
            if (batch.outcome == StepOutcome::crashed) {
                result.fatal_error = crash_log(proc);
                all_steps_passed = false;
                break;
            }
            if (batch.outcome == StepOutcome::failed) {
                all_steps_passed = false;
                if (fail_fast) {
                    abort_steps = true;
                }
            }
        }
    }

    if (proc == nullptr || proc->alive()) {
        for (const Step& step : tc.after_all) {
            StepBatch batch = exec_step(step, proc, *client, false);
            merge_batch(result, batch);
            if (proc && !proc->alive()) {
                break;
            }
        }
    }
    if (proc && !proc->alive() && !tc.after_all.empty()) {
        const std::string note = "after_all skipped: " + crash_log(proc);
        if (result.fatal_error.empty()) {
            result.fatal_error = note;
        } else {
            result.fatal_error += "; " + note;
        }
    }

    result.passed = all_steps_passed && result.fatal_error.empty();
    return result;
}

} // namespace

FileResult run_pipeline(const TestCase& tc, GodotProcess* proc,
                        McpTestClient* external_client) {
    FileResult result;
    result.name = tc.name;
    const auto pipeline_start = std::chrono::steady_clock::now();

    if (tc.name.empty()) {
        result.fatal_error = "empty test case name";
        return result;
    }
    if (proc && !proc->alive()) {
        result.fatal_error = "editor process not alive";
        return result;
    }

    try {
        result = run_pipeline_inner(tc, proc, external_client);
    } catch (const std::exception& e) {
        result.fatal_error = e.what();
        result.passed = false;
    } catch (...) {
        result.fatal_error = "unknown exception";
        result.passed = false;
    }

    result.duration_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - pipeline_start).count();
    return result;
}

} // namespace gsd_test
