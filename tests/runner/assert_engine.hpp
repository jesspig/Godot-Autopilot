#pragma once
#include "config_loader.hpp"
#include <mcp/JsonValue.hpp>
#include <string>

namespace gda_test {

const mcp::JsonValue* json_lookup(const mcp::JsonValue& root, const std::string& dotted_path);

bool check_expectations(const mcp::JsonValue& response, const StepExpect& expect,
                        std::string& out_detail);

}
