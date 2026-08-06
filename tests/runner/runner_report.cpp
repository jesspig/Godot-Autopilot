#include "runner_report.hpp"

#include <mcp/JsonValue.hpp>

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace gsd_test {

namespace {

// 状态: fatal_error 非空 → ERROR; passed → PASS; 否则 FAIL
const char* status_text(const FileResult& result) {
    if (!result.fatal_error.empty()) {
        return "ERROR";
    }
    return result.passed ? "PASS" : "FAIL";
}

size_t passed_step_count(const FileResult& result) {
    size_t count = 0;
    for (const auto& step : result.steps) {
        if (step.passed) {
            ++count;
        }
    }
    return count;
}

// 耗时 < 10 秒显示毫秒, 否则显示秒(1 位小数)
std::string duration_text(long long duration_ms) {
    if (duration_ms >= 10000) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f s", duration_ms / 1000.0);
        return std::string(buf);
    }
    return std::to_string(duration_ms) + " ms";
}

// 本地时间戳 YYYYmmdd_HHMMSS
std::string local_timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm broken_down{};
#ifdef _WIN32
    localtime_s(&broken_down, &now);
#else
    localtime_r(&now, &broken_down);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &broken_down);
    return std::string(buf);
}

}  // namespace

void print_console_report(const std::vector<FileResult>& results) {
    size_t name_width = 14;
    for (const auto& result : results) {
        if (result.name.size() + 2 > name_width) {
            name_width = result.name.size() + 2;
        }
    }

    std::cout << std::left << std::setw(static_cast<int>(name_width)) << "文件名称"
              << "| " << std::right << std::setw(7) << "步骤"
              << " | " << std::setw(8) << "耗时"
              << " | " << std::setw(5) << "状态" << '\n';
    std::cout << std::string(name_width + 1, '-') << "+---------+----------+-------\n";

    long long total_ms = 0;
    size_t passed_files = 0;
    constexpr size_t kMaxFailedDetailLines = 20;

    for (const auto& result : results) {
        total_ms += result.duration_ms;
        if (result.passed) {
            ++passed_files;
        }

        const size_t total_steps = result.steps.size();
        const size_t passed_steps = passed_step_count(result);
        std::cout << std::left << std::setw(static_cast<int>(name_width)) << result.name
                  << "| " << std::right << std::setw(6)
                  << (std::to_string(passed_steps) + "/" + std::to_string(total_steps))
                  << " | " << std::setw(8) << duration_text(result.duration_ms)
                  << " | " << std::setw(5) << status_text(result) << '\n';

        size_t failed_count = 0;
        for (const auto& step : result.steps) {
            if (step.passed) {
                continue;
            }
            ++failed_count;
            if (failed_count > kMaxFailedDetailLines) {
                continue;
            }
            std::cout << "    - 步骤 " << step.step_id << " [" << step.tool << "] "
                      << step.detail << '\n';
        }
        if (failed_count > kMaxFailedDetailLines) {
            std::cout << "    … 共 " << failed_count << " 条失败步骤, 仅显示前 "
                      << kMaxFailedDetailLines << " 条\n";
        }
    }

    const size_t failed_files = results.size() - passed_files;
    std::cout << "总计: " << results.size() << " 文件, 通过 " << passed_files << ", 失败 "
              << failed_files << ", 总耗时 " << duration_text(total_ms) << '\n';
}

std::string save_json_report(const std::vector<FileResult>& results, const std::string& dir) {
    const std::string timestamp = local_timestamp();
    const std::string file_path = dir + "/report-" + timestamp + ".json";

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        throw std::runtime_error("创建报告目录失败(位置: " + dir + "): " + ec.message()
                                 + " (期望: 目录可创建)");
    }

    size_t passed_files = 0;
    for (const auto& result : results) {
        if (result.passed) {
            ++passed_files;
        }
    }

    mcp::JsonValue root(mcp::JsonValue::object_tag);
    root["generated_at"] = mcp::JsonValue(timestamp);
    root["total_files"] = mcp::JsonValue(static_cast<int64_t>(results.size()));
    root["passed_files"] = mcp::JsonValue(static_cast<int64_t>(passed_files));

    mcp::JsonValue files_array(mcp::JsonValue::array_tag);
    for (const auto& result : results) {
        mcp::JsonValue file_obj(mcp::JsonValue::object_tag);
        file_obj["name"] = mcp::JsonValue(result.name);
        file_obj["passed"] = mcp::JsonValue(result.passed);
        file_obj["duration_ms"] = mcp::JsonValue(result.duration_ms);
        file_obj["fatal_error"] = mcp::JsonValue(result.fatal_error);

        mcp::JsonValue steps_array(mcp::JsonValue::array_tag);
        for (const auto& step : result.steps) {
            mcp::JsonValue step_obj(mcp::JsonValue::object_tag);
            step_obj["step_id"] = mcp::JsonValue(step.step_id);
            step_obj["tool"] = mcp::JsonValue(step.tool);
            step_obj["passed"] = mcp::JsonValue(step.passed);
            step_obj["detail"] = mcp::JsonValue(step.detail);
            steps_array.PushBack(std::move(step_obj));
        }
        file_obj["steps"] = std::move(steps_array);
        files_array.PushBack(std::move(file_obj));
    }
    root["files"] = std::move(files_array);

    std::ofstream out(file_path);
    if (!out) {
        throw std::runtime_error("无法打开报告文件(位置: " + file_path + "): (期望: 文件可写)");
    }
    out << root.Dump(2) << '\n';
    if (!out) {
        throw std::runtime_error("写入报告文件失败(位置: " + file_path + "): (期望: 文件可写)");
    }
    return file_path;
}

}  // namespace gsd_test
