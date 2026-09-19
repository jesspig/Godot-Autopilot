#pragma once
#include "pipeline_executor.hpp"
#include <string>
#include <vector>

namespace gda_test {

void print_console_report(const std::vector<FileResult>& results);

std::string save_json_report(const std::vector<FileResult>& results, const std::string& dir);

}
