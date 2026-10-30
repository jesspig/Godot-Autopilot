#ifndef GODOT_AUTOPILOT_API_SURFACE_HPP
#define GODOT_AUTOPILOT_API_SURFACE_HPP

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace api_surface {

constexpr size_t kDefaultToolListLimit = 500;
constexpr size_t kMaxToolListLimit = 2000;
constexpr size_t kMaxBatchOperations = 256;
constexpr size_t kMaxToolNameLength = 128;

struct ToolEntry {
  std::string name;
  std::string description;
  std::string category;
  std::vector<std::string> tags;
  std::string side_effect;
  mcp::JsonValue input_schema;
  bool dynamic{false};
  bool mutating{false};
};

struct RuntimeInfo {
  std::string version;
  int port{0};
  int pid{0};
};

struct ApiReply {
  int status{200};
  std::string status_text{"OK"};
  mcp::JsonValue body;
};

struct ListQuery {
  std::string query;
  std::string category;
  std::vector<std::string> tags;
  size_t limit{kDefaultToolListLimit};
  size_t offset{0};
};

struct BatchOperation {
  std::string tool;
  mcp::JsonValue args;
};

struct BatchRequest {
  std::vector<BatchOperation> operations;
  bool stop_on_error{true};
  bool rollback_on_error{false};
};

enum class RouteKind {
  kIndex,
  kTools,
  kToolDetail,
  kToolCall,
  kBatch,
  kHealth,
  kOpenApi,
  kEndpoints,
  kNotFound,
  kMethodNotAllowed,
};

struct RouteMatch {
  RouteKind kind{RouteKind::kNotFound};
  std::string tool_name;
  std::string error;
};

using ToolListerFn = std::function<std::vector<ToolEntry>()>;
using ToolExecutorFn =
    std::function<mcp::JsonValue(const std::string &, const mcp::JsonValue &)>;
using TokenVerifierFn =
    std::function<bool(const std::unordered_map<std::string, std::string> &)>;
using RuntimeInfoProviderFn = std::function<RuntimeInfo()>;

ToolListerFn &tool_lister_slot();
void register_tool_lister(ToolListerFn provider);
ToolExecutorFn &tool_executor_slot();
void register_tool_executor(ToolExecutorFn provider);
TokenVerifierFn &token_verifier_slot();
void register_token_verifier(TokenVerifierFn verifier);
RuntimeInfoProviderFn &runtime_info_slot();
void register_runtime_info_provider(RuntimeInfoProviderFn provider);

std::vector<ToolEntry> list_tools();
mcp::JsonValue execute_tool(const std::string &name, const mcp::JsonValue &args);
bool verify_token(const std::unordered_map<std::string, std::string> &headers);
RuntimeInfo runtime_info();

RouteMatch match_route(const std::string &method, const std::string &path);
bool is_valid_tool_name(const std::string &name);
bool parse_list_query(const std::unordered_map<std::string, std::string> &params,
                       ListQuery &out, std::string &error);
bool parse_batch_request(const mcp::JsonValue &body, BatchRequest &out,
                          std::string &error);
std::string sanitized_request_id(const std::string &raw);
std::vector<ToolEntry> filter_tools(const std::vector<ToolEntry> &tools,
                                    const ListQuery &query);
std::vector<ToolEntry> paginate_tools(const std::vector<ToolEntry> &tools,
                                      size_t offset, size_t limit);
mcp::JsonValue tool_entry_to_json(const ToolEntry &entry);
mcp::JsonValue build_tool_list_response(const std::vector<ToolEntry> &filtered,
                                         size_t total, const ListQuery &query);
mcp::JsonValue build_error_body(const std::string &code,
                                 const std::string &message,
                                 const std::string &request_id = {});
int status_for_code(const std::string &code);
std::string status_text_for(int status);
mcp::JsonValue build_index_document();
mcp::JsonValue build_health_document();
mcp::JsonValue build_openapi_document();
ApiReply handle_request(const std::string &method, const std::string &path,
                        const std::unordered_map<std::string, std::string> &headers,
                        const std::string &body);

} // namespace api_surface
} // namespace godot_autopilot

#endif
