#pragma once
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace gsd_test {

struct ToolSummary {
  std::string name;
  std::string description;
};

class McpTestClient {
public:
  explicit McpTestClient(int port);
  ~McpTestClient();

  bool connect(std::chrono::seconds timeout = std::chrono::seconds(30));

  std::vector<ToolSummary> list_tools();

  std::string call_tool(const std::string &name, const std::string &args_json);

  std::string call_tool(const std::string &name);

  bool last_call_was_error() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace gsd_test
