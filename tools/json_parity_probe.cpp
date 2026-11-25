#include <mcp/JsonValue.hpp>
#include <mcp/McpError.hpp>

#include <iostream>
#include <string>
#include <vector>

#include "util/json_value.hpp"

namespace {

const char* McpCodeName(mcp::McpErrorCode code) {
  if (code == mcp::McpErrorCode::ParseError) return "parse_error";
  if (code == mcp::McpErrorCode::DeserializeFailed) return "deserialization_failed";
  return "other";
}

const char* GdaCodeName(godot_autopilot::util::JsonErrorCode code) {
  if (code == godot_autopilot::util::JsonErrorCode::parse_error) return "parse_error";
  if (code == godot_autopilot::util::JsonErrorCode::deserialization_failed) {
    return "deserialization_failed";
  }
  return "other";
}

int CompareCase(const std::string& input) {
  bool mcp_ok = true;
  bool gda_ok = true;
  std::string mcp_dump;
  std::string gda_dump;
  std::string mcp_code;
  std::string gda_code;
  try {
    mcp_dump = mcp::JsonValue::Parse(input).Dump();
  } catch (const mcp::McpError& error) {
    mcp_ok = false;
    mcp_code = McpCodeName(error.Code());
  }
  try {
    gda_dump = godot_autopilot::util::JsonValue::Parse(input).Dump();
  } catch (const godot_autopilot::util::JsonError& error) {
    gda_ok = false;
    gda_code = GdaCodeName(error.Code());
  }
  if (mcp_ok != gda_ok) {
    std::cerr << "PARITY mismatch outcome: " << input
              << " mcp_ok=" << mcp_ok << " gda_ok=" << gda_ok << "\n";
    return 1;
  }
  if (mcp_ok && mcp_dump != gda_dump) {
    std::cerr << "PARITY mismatch dump: " << input
              << " mcp=" << mcp_dump << " gda=" << gda_dump << "\n";
    return 1;
  }
  if (!mcp_ok && mcp_code != gda_code) {
    std::cerr << "PARITY mismatch code: " << input
              << " mcp=" << mcp_code << " gda=" << gda_code << "\n";
    return 1;
  }
  return 0;
}

} // namespace

int main() {
  const std::vector<std::string> cases = {
      "null", "true", "false", "-17", "0", "-0", "42", "3.14", "0.1", "1.0", "1e2",
      "\"hello\"", R"("test \" \\ \/ \b \f \n \r \t")", R"("\uD83D\uDE00")",
      "[1,2,3]", "[ 1 , 2 , 3 ]", "[]", "{}", R"({"a":1,"b":"x"})",
      R"({"a":{"b":[1]}})", R"({"a":1,"a":2})", " \t\r\n42 \t\r\n",
      "-9223372036854775808", "9223372036854775807", "18446744073709551615",
      "18446744073709551616", "-9223372036854775809", "1e400", "1e-400", "5e-324",
      "01", "1.", ".5", "1e", "+1", "--1", "0x10", "NaN", "Infinity",
      "'a'", "[1,]", R"({"a":1,})", "", "   ", "1 2", "{}x", "{", "[1,",
      R"("abc)", "tru", "nu", "fal", "]", "}", "{a:1}", "[1 2]",
      std::string("\"\xFF\""), std::string("\"\xC0\x80\""),
      std::string("\"\xE2\x82\""), std::string("\"\xED\xA0\x80\""),
      std::string("\"\\uD800\""), std::string("\"\\uDC00\""),
  };

  int mismatches = 0;
  for (const std::string& input : cases) {
    mismatches += CompareCase(input);
  }
  std::cout << "json parity probe: " << cases.size() << " case(s), "
            << mismatches << " mismatch(es)\n";
  return mismatches == 0 ? 0 : 1;
}
