#include "core/api_surface.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <version.hpp>

#include "core/api_auth.hpp"
#include "core/api_envelope.hpp"
#include "core/command_queue.hpp"
#include "core/http_parse.hpp"
#include "tools/dispatch.hpp"
#include "tools/register_all.hpp"
#include "tools/runtime_ops.hpp"
#include "tools/tool_base.hpp"
#include "tools/tool_registry.hpp"

namespace godot_autopilot {
namespace api_surface {

namespace {

constexpr const char *kApiPrefix = "/api";
constexpr const char *kToolsSegment = "/tools";
constexpr const char *kHealthSegment = "/health";
constexpr const char *kBatchSegment = "/batch";
constexpr const char *kOpenApiSegment = "/openapi.json";
constexpr const char *kCodeUnauthorized = "unauthorized";
constexpr const char *kCodeInvalidArgument = "invalid_argument";
constexpr const char *kCodeNotFound = "not_found";
constexpr const char *kCodeMethodNotAllowed = "method_not_allowed";

std::string to_lower(const std::string &text) {
  std::string out = text;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

std::string to_upper(const std::string &text) {
  std::string out = text;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return out;
}

std::string trim(const std::string &text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
    ++begin;
  }
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return text.substr(begin, end - begin);
}

bool contains_fold(const std::string &haystack, const std::string &needle) {
  if (needle.empty()) {
    return true;
  }
  return to_lower(haystack).find(to_lower(needle)) != std::string::npos;
}

bool equals_fold(const std::string &left, const std::string &right) {
  return to_lower(left) == to_lower(right);
}

std::vector<std::string> split_tags(const std::string &text) {
  std::vector<std::string> tags;
  size_t cursor = 0;
  while (cursor <= text.size()) {
    const size_t separator = text.find(',', cursor);
    const size_t end =
        separator == std::string::npos ? text.size() : separator;
    const std::string tag = trim(text.substr(cursor, end - cursor));
    if (!tag.empty()) {
      tags.push_back(tag);
    }
    if (separator == std::string::npos) {
      break;
    }
    cursor = separator + 1;
  }
  return tags;
}

bool parse_size(const std::string &text, size_t &out) {
  if (text.empty()) {
    return false;
  }
  size_t value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') {
      return false;
    }
    value = value * 10 + static_cast<size_t>(c - '0');
  }
  out = value;
  return true;
}

std::unordered_map<std::string, std::string>
query_params_from_headers(const std::unordered_map<std::string, std::string> &headers) {
  std::unordered_map<std::string, std::string> params;
  const std::string prefix = http_parse::kQueryHeaderPrefix;
  for (const auto &entry : headers) {
    if (entry.first.size() <= prefix.size() ||
        entry.first.compare(0, prefix.size(), prefix) != 0) {
      continue;
    }
    params[entry.first.substr(prefix.size())] = entry.second;
  }
  const auto raw = headers.find(http_parse::kQueryRawHeader);
  if (raw != headers.end() && !raw->second.empty()) {
    for (const auto &entry : http_parse::parse_query(raw->second)) {
      params[entry.first] = entry.second;
    }
  }
  return params;
}

bool entry_has_tag(const ToolEntry &entry, const std::string &tag) {
  for (const std::string &candidate : entry.tags) {
    if (equals_fold(candidate, tag)) {
      return true;
    }
  }
  return false;
}

bool entry_matches_query(const ToolEntry &entry, const std::string &needle) {
  if (contains_fold(entry.name, needle) ||
      contains_fold(entry.description, needle) ||
      contains_fold(entry.category, needle)) {
    return true;
  }
  for (const std::string &tag : entry.tags) {
    if (contains_fold(tag, needle)) {
      return true;
    }
  }
  return false;
}

mcp::JsonValue empty_object() { return mcp::JsonValue(mcp::JsonValue::object_tag); }

bool parse_body_object(const std::string &body, mcp::JsonValue &out,
                        std::string &error) {
  const std::string trimmed = trim(body);
  if (trimmed.empty()) {
    out = empty_object();
    return true;
  }
  mcp::JsonValue parsed;
  try {
    parsed = mcp::JsonValue::Parse(trimmed);
  } catch (const std::exception &) {
    error = "the request body is not valid JSON";
    return false;
  } catch (...) {
    error = "the request body is not valid JSON";
    return false;
  }
  if (!parsed.IsObject()) {
    error = "the request body must be a JSON object";
    return false;
  }
  out = std::move(parsed);
  return true;
}

std::vector<ToolEntry> default_list_tools() {
  std::vector<ToolEntry> entries;
  const std::shared_ptr<ToolRegistry> registry = get_active_registry();
  if (!registry) {
    return entries;
  }
  for (const std::shared_ptr<ToolBase> &tool : registry->all_any()) {
    if (!tool) {
      continue;
    }
    ToolEntry entry;
    entry.name = tool->meta().name;
    entry.description = tool->meta().description;
    entry.category = tool->meta().category;
    entry.tags = tool->meta().tags;
    entry.side_effect = side_effect_name(side_effect_of(*tool));
    entry.input_schema = tool->input_schema();
    const uint32_t flags = tool->tool_flags();
    entry.dynamic = (flags & tool_flags::kDynamic) != 0;
    entry.mutating = (flags & tool_flags::kMutating) != 0;
    entries.push_back(std::move(entry));
  }
  return entries;
}

mcp::JsonValue default_execute_tool(const std::string &name,
                                    const mcp::JsonValue &args) {
  if (runtime_ops::has_editor_queue() && get_editor_queue().is_main_thread()) {
    mcp::JsonValue result(mcp::JsonValue::object_tag);
    result["error"] = mcp::JsonValue(
        "the REST tool call must not run on the editor main thread");
    result["code"] = mcp::JsonValue("internal_error");
    return result;
  }
  return dispatch::call_handler(name, args);
}

bool default_verify_token(
    const std::unordered_map<std::string, std::string> &headers) {
  const ApiAuth auth;
  return auth.verify(headers);
}

RuntimeInfo default_runtime_info() {
  RuntimeInfo info;
  info.version = GDA_VERSION;
  return info;
}

ApiReply ok_reply(mcp::JsonValue body) {
  ApiReply reply;
  reply.status = 200;
  reply.status_text = status_text_for(200);
  reply.body = std::move(body);
  return reply;
}

ApiReply error_reply(int status, const std::string &code,
                     const std::string &message) {
  ApiReply reply;
  reply.status = status;
  reply.status_text = status_text_for(status);
  reply.body = build_error_body(code, message);
  return reply;
}

mcp::JsonValue openapi_operation(const std::string &summary,
                                 const std::string &tag) {
  mcp::JsonValue operation(mcp::JsonValue::object_tag);
  operation["summary"] = mcp::JsonValue(summary);
  operation["tags"] = mcp::JsonValue(tag);
  mcp::JsonValue ok_description(mcp::JsonValue::object_tag);
  ok_description["description"] = mcp::JsonValue("successful response");
  mcp::JsonValue unauthorized(mcp::JsonValue::object_tag);
  unauthorized["description"] =
      mcp::JsonValue("missing or invalid bearer token");
  mcp::JsonValue responses(mcp::JsonValue::object_tag);
  responses["200"] = std::move(ok_description);
  responses["401"] = std::move(unauthorized);
  operation["responses"] = std::move(responses);
  return operation;
}

mcp::JsonValue openapi_path(const std::string &summary, const std::string &tag) {
  mcp::JsonValue operation = openapi_operation(summary, tag);
  mcp::JsonValue path(mcp::JsonValue::object_tag);
  path["get"] = std::move(operation);
  return path;
}

mcp::JsonValue openapi_tool_path() {
  mcp::JsonValue path(mcp::JsonValue::object_tag);
  mcp::JsonValue listing = openapi_operation("list the registered tools", "tools");
  mcp::JsonValue parameters(mcp::JsonValue::array_tag);
  const char *names[] = {"query", "category", "tags", "limit", "offset"};
  for (const char *name : names) {
    mcp::JsonValue parameter(mcp::JsonValue::object_tag);
    parameter["name"] = mcp::JsonValue(name);
    parameter["in"] = mcp::JsonValue("query");
    parameter["required"] = mcp::JsonValue(false);
    parameters.PushBack(std::move(parameter));
  }
  listing["parameters"] = std::move(parameters);
  path["get"] = std::move(listing);
  mcp::JsonValue calling =
      openapi_operation("call a tool with a JSON argument object", "tools");
  mcp::JsonValue tool_parameter(mcp::JsonValue::object_tag);
  tool_parameter["name"] = mcp::JsonValue("tool");
  tool_parameter["in"] = mcp::JsonValue("path");
  tool_parameter["required"] = mcp::JsonValue(true);
  mcp::JsonValue tool_parameters(mcp::JsonValue::array_tag);
  tool_parameters.PushBack(std::move(tool_parameter));
  calling["parameters"] = std::move(tool_parameters);
  path["post"] = std::move(calling);
  return path;
}

mcp::JsonValue openapi_single_tool_path() {
  mcp::JsonValue path(mcp::JsonValue::object_tag);
  path["get"] =
      openapi_operation("read one tool description", "tools");
  path["post"] =
      openapi_operation("call one tool with a JSON argument object", "tools");
  return path;
}

} // namespace

ToolListerFn &tool_lister_slot() {
  static ToolListerFn provider;
  return provider;
}

void register_tool_lister(ToolListerFn provider) {
  tool_lister_slot() = std::move(provider);
}

ToolExecutorFn &tool_executor_slot() {
  static ToolExecutorFn provider;
  return provider;
}

void register_tool_executor(ToolExecutorFn provider) {
  tool_executor_slot() = std::move(provider);
}

TokenVerifierFn &token_verifier_slot() {
  static TokenVerifierFn verifier;
  return verifier;
}

void register_token_verifier(TokenVerifierFn verifier) {
  token_verifier_slot() = std::move(verifier);
}

RuntimeInfoProviderFn &runtime_info_slot() {
  static RuntimeInfoProviderFn provider;
  return provider;
}

void register_runtime_info_provider(RuntimeInfoProviderFn provider) {
  runtime_info_slot() = std::move(provider);
}

std::vector<ToolEntry> list_tools() {
  ToolListerFn &provider = tool_lister_slot();
  if (provider) {
    return provider();
  }
  return default_list_tools();
}

mcp::JsonValue execute_tool(const std::string &name, const mcp::JsonValue &args) {
  ToolExecutorFn &provider = tool_executor_slot();
  if (provider) {
    return provider(name, args);
  }
  return default_execute_tool(name, args);
}

bool verify_token(const std::unordered_map<std::string, std::string> &headers) {
  TokenVerifierFn &verifier = token_verifier_slot();
  if (verifier) {
    return verifier(headers);
  }
  return default_verify_token(headers);
}

RuntimeInfo runtime_info() {
  RuntimeInfoProviderFn &provider = runtime_info_slot();
  if (provider) {
    return provider();
  }
  return default_runtime_info();
}

bool is_valid_tool_name(const std::string &name) {
  if (name.empty() || name.size() > kMaxToolNameLength) {
    return false;
  }
  for (const char c : name) {
    const bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    const bool digit = c >= '0' && c <= '9';
    if (!alpha && !digit && c != '_' && c != '-' && c != '.') {
      return false;
    }
  }
  return true;
}

RouteMatch match_route(const std::string &method, const std::string &path) {
  RouteMatch match;
  const std::string prefix = kApiPrefix;
  if (path.size() < prefix.size() ||
      path.compare(0, prefix.size(), prefix) != 0) {
    match.kind = RouteKind::kNotFound;
    match.error = "unknown API route: " + path;
    return match;
  }
  std::string rest = path.substr(prefix.size());
  if (!rest.empty() && rest.front() != '/') {
    match.kind = RouteKind::kNotFound;
    match.error = "unknown API route: " + path;
    return match;
  }
  const std::string upper_method = to_upper(method);
  if (rest == "/") {
    rest.clear();
  }
  if (rest.empty()) {
    if (upper_method != "GET") {
      match.kind = RouteKind::kMethodNotAllowed;
      match.error = "the route " + path + " only accepts GET";
      return match;
    }
    match.kind = RouteKind::kIndex;
    return match;
  }
  if (rest == kHealthSegment) {
    if (upper_method != "GET") {
      match.kind = RouteKind::kMethodNotAllowed;
      match.error = "the route " + path + " only accepts GET";
      return match;
    }
    match.kind = RouteKind::kHealth;
    return match;
  }
  if (rest == kOpenApiSegment) {
    if (upper_method != "GET") {
      match.kind = RouteKind::kMethodNotAllowed;
      match.error = "the route " + path + " only accepts GET";
      return match;
    }
    match.kind = RouteKind::kOpenApi;
    return match;
  }
  if (rest == kToolsSegment) {
    if (upper_method != "GET") {
      match.kind = RouteKind::kMethodNotAllowed;
      match.error = "the route " + path + " only accepts GET";
      return match;
    }
    match.kind = RouteKind::kTools;
    return match;
  }
  const std::string tools_prefix = std::string(kToolsSegment) + "/";
  if (rest.compare(0, tools_prefix.size(), tools_prefix) == 0) {
    const std::string raw_name = rest.substr(tools_prefix.size());
    const std::string name = http_parse::percent_decode(raw_name);
    if (!is_valid_tool_name(name)) {
      match.kind = RouteKind::kNotFound;
      match.error = "unknown API route: " + path;
      return match;
    }
    if (upper_method != "GET" && upper_method != "POST") {
      match.kind = RouteKind::kMethodNotAllowed;
      match.error = "the route " + path + " only accepts GET and POST";
      return match;
    }
    match.tool_name = name;
    match.kind = upper_method == "GET" ? RouteKind::kToolDetail
                                       : RouteKind::kToolCall;
    return match;
  }
  if (rest == kBatchSegment) {
    if (upper_method != "POST") {
      match.kind = RouteKind::kMethodNotAllowed;
      match.error = "the route " + path + " only accepts POST";
      return match;
    }
    match.kind = RouteKind::kBatch;
    return match;
  }
  match.kind = RouteKind::kNotFound;
  match.error = "unknown API route: " + path;
  return match;
}

bool parse_list_query(const std::unordered_map<std::string, std::string> &params,
                       ListQuery &out, std::string &error) {
  out = ListQuery();
  const auto query = params.find("query");
  if (query != params.end()) {
    out.query = trim(query->second);
  }
  const auto category = params.find("category");
  if (category != params.end()) {
    out.category = trim(category->second);
  }
  const auto tags = params.find("tags");
  if (tags != params.end()) {
    out.tags = split_tags(tags->second);
  }
  const auto limit = params.find("limit");
  if (limit != params.end() && !limit->second.empty()) {
    size_t parsed = 0;
    if (!parse_size(trim(limit->second), parsed)) {
      error = "the limit parameter must be a non-negative integer";
      return false;
    }
    out.limit = parsed == 0 ? kDefaultToolListLimit : parsed;
    if (out.limit > kMaxToolListLimit) {
      out.limit = kMaxToolListLimit;
    }
  }
  const auto offset = params.find("offset");
  if (offset != params.end() && !offset->second.empty()) {
    size_t parsed = 0;
    if (!parse_size(trim(offset->second), parsed)) {
      error = "the offset parameter must be a non-negative integer";
      return false;
    }
    out.offset = parsed;
  }
  return true;
}

bool parse_batch_request(const mcp::JsonValue &body, BatchRequest &out,
                          std::string &error) {
  out = BatchRequest();
  if (!body.IsObject()) {
    error = "the request body must be a JSON object";
    return false;
  }
  const mcp::JsonValue *operations = body.Find("operations");
  if (operations == nullptr || !operations->IsArray()) {
    error = "missing required parameter: operations (array)";
    return false;
  }
  if (operations->GetArray().size() > kMaxBatchOperations) {
    error = "operations exceeds the maximum of " +
            std::to_string(kMaxBatchOperations) + " items";
    return false;
  }
  const mcp::JsonValue *stop = body.Find("stop_on_error");
  if (stop != nullptr) {
    if (!stop->IsBool()) {
      error = "stop_on_error must be a boolean";
      return false;
    }
    out.stop_on_error = stop->GetBool();
  }
  for (const mcp::JsonValue &item : operations->GetArray()) {
    if (!item.IsObject()) {
      error = "every operation must be a JSON object";
      return false;
    }
    const mcp::JsonValue *tool = item.Find("tool");
    if (tool == nullptr || !tool->IsString() || tool->GetString().empty()) {
      error = "every operation requires a non-empty tool name";
      return false;
    }
    BatchOperation operation;
    operation.tool = tool->GetString();
    const mcp::JsonValue *args = item.Find("args");
    if (args != nullptr && !args->IsNull()) {
      if (!args->IsObject()) {
        error = "the args of operation " + operation.tool +
                " must be a JSON object";
        return false;
      }
      operation.args = *args;
    } else {
      operation.args = empty_object();
    }
    out.operations.push_back(std::move(operation));
  }
  return true;
}

std::vector<ToolEntry> filter_tools(const std::vector<ToolEntry> &tools,
                                    const ListQuery &query) {
  std::vector<ToolEntry> filtered;
  filtered.reserve(tools.size());
  for (const ToolEntry &entry : tools) {
    if (!query.category.empty() && !equals_fold(entry.category, query.category)) {
      continue;
    }
    bool tags_ok = true;
    for (const std::string &tag : query.tags) {
      if (!entry_has_tag(entry, tag)) {
        tags_ok = false;
        break;
      }
    }
    if (!tags_ok) {
      continue;
    }
    if (!query.query.empty() && !entry_matches_query(entry, query.query)) {
      continue;
    }
    filtered.push_back(entry);
  }
  return filtered;
}

std::vector<ToolEntry> paginate_tools(const std::vector<ToolEntry> &tools,
                                      size_t offset, size_t limit) {
  std::vector<ToolEntry> page;
  if (offset >= tools.size()) {
    return page;
  }
  const size_t available = tools.size() - offset;
  const size_t count = limit == 0 ? available : std::min(limit, available);
  page.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    page.push_back(tools[offset + index]);
  }
  return page;
}

mcp::JsonValue tool_entry_to_json(const ToolEntry &entry) {
  mcp::JsonValue tags(mcp::JsonValue::array_tag);
  for (const std::string &tag : entry.tags) {
    tags.PushBack(mcp::JsonValue(tag));
  }
  mcp::JsonValue body(mcp::JsonValue::object_tag);
  body["name"] = mcp::JsonValue(entry.name);
  body["description"] = mcp::JsonValue(entry.description);
  body["category"] = mcp::JsonValue(entry.category);
  body["tags"] = std::move(tags);
  body["side_effect"] = mcp::JsonValue(entry.side_effect);
  body["input_schema"] = entry.input_schema;
  body["dynamic"] = mcp::JsonValue(entry.dynamic);
  body["mutating"] = mcp::JsonValue(entry.mutating);
  return body;
}

mcp::JsonValue build_tool_list_response(const std::vector<ToolEntry> &filtered,
                                         size_t total, const ListQuery &query) {
  mcp::JsonValue tools(mcp::JsonValue::array_tag);
  for (const ToolEntry &entry : filtered) {
    tools.PushBack(tool_entry_to_json(entry));
  }
  mcp::JsonValue body(mcp::JsonValue::object_tag);
  body["tools"] = std::move(tools);
  body["total"] = mcp::JsonValue(static_cast<int64_t>(total));
  body["count"] = mcp::JsonValue(static_cast<int64_t>(filtered.size()));
  body["offset"] = mcp::JsonValue(static_cast<int64_t>(query.offset));
  body["limit"] = mcp::JsonValue(static_cast<int64_t>(query.limit));
  return body;
}

mcp::JsonValue build_error_body(const std::string &code,
                                 const std::string &message) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] = mcp::JsonValue(message);
  result["code"] = mcp::JsonValue(code);
  const ApiEnvelope envelope;
  return envelope.build(result, std::string());
}

int status_for_code(const std::string &code) {
  if (code == kCodeMethodNotAllowed) {
    return 405;
  }
  if (code == "conflict") {
    return 409;
  }
  if (code == "payload_too_large") {
    return 413;
  }
  if (code == "too_many_requests") {
    return 429;
  }
  if (code == kCodeNotFound) {
    return 404;
  }
  if (code == "validation_error") {
    return 400;
  }
  const ApiEnvelope envelope;
  return envelope.status_for(code);
}

std::string status_text_for(int status) {
  switch (status) {
    case 200:
      return "OK";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 409:
      return "Conflict";
    case 413:
      return "Payload Too Large";
    case 422:
      return "Unprocessable Entity";
    case 429:
      return "Too Many Requests";
    case 500:
      return "Internal Server Error";
    case 503:
      return "Service Unavailable";
    case 504:
      return "Gateway Timeout";
    default:
      return "OK";
  }
}

mcp::JsonValue build_index_document() {
  const RuntimeInfo info = runtime_info();
  mcp::JsonValue endpoints(mcp::JsonValue::array_tag);
  const char *descriptions[][3] = {
      {"GET", "/api", "list the API endpoints"},
      {"GET", "/api/tools", "list the registered tools"},
      {"GET", "/api/tools/{tool}", "read one tool description"},
      {"POST", "/api/tools/{tool}", "call one tool"},
      {"POST", "/api/batch", "call several tools in order"},
      {"GET", "/api/health", "read the liveness probe"},
      {"GET", "/api/openapi.json", "read the OpenAPI document"},
  };
  for (const auto &entry : descriptions) {
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["method"] = mcp::JsonValue(entry[0]);
    item["path"] = mcp::JsonValue(entry[1]);
    item["description"] = mcp::JsonValue(entry[2]);
    endpoints.PushBack(std::move(item));
  }
  mcp::JsonValue body(mcp::JsonValue::object_tag);
  body["service"] = mcp::JsonValue("godot-autopilot");
  body["version"] = mcp::JsonValue(info.version);
  body["endpoints"] = std::move(endpoints);
  return body;
}

mcp::JsonValue build_health_document() {
  const RuntimeInfo info = runtime_info();
  mcp::JsonValue body(mcp::JsonValue::object_tag);
  body["status"] = mcp::JsonValue("ok");
  body["service"] = mcp::JsonValue("godot-autopilot");
  body["version"] = mcp::JsonValue(info.version);
  body["port"] = mcp::JsonValue(static_cast<int64_t>(info.port));
  body["pid"] = mcp::JsonValue(static_cast<int64_t>(info.pid));
  return body;
}

mcp::JsonValue build_openapi_document() {
  const RuntimeInfo info = runtime_info();
  mcp::JsonValue info_object(mcp::JsonValue::object_tag);
  info_object["title"] = mcp::JsonValue("Godot Autopilot REST API");
  info_object["version"] = mcp::JsonValue(info.version);
  mcp::JsonValue paths(mcp::JsonValue::object_tag);
  paths["/api"] = openapi_path("list the API endpoints", "meta");
  paths["/api/tools"] = openapi_tool_path();
  paths["/api/tools/{tool}"] = openapi_single_tool_path();
  mcp::JsonValue batch(mcp::JsonValue::object_tag);
  mcp::JsonValue batch_operation =
      openapi_operation("call several tools in order", "tools");
  batch_operation["summary"] = mcp::JsonValue("call several tools in order");
  batch["post"] = std::move(batch_operation);
  paths["/api/batch"] = std::move(batch);
  mcp::JsonValue health(mcp::JsonValue::object_tag);
  mcp::JsonValue health_operation =
      openapi_operation("read the liveness probe", "meta");
  health_operation["security"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  health["get"] = std::move(health_operation);
  paths["/api/health"] = std::move(health);
  paths["/api/openapi.json"] =
      openapi_path("read the OpenAPI document", "meta");
  mcp::JsonValue bearer(mcp::JsonValue::object_tag);
  bearer["type"] = mcp::JsonValue("http");
  bearer["scheme"] = mcp::JsonValue("bearer");
  mcp::JsonValue schemes(mcp::JsonValue::object_tag);
  schemes["bearerAuth"] = std::move(bearer);
  mcp::JsonValue components(mcp::JsonValue::object_tag);
  components["securitySchemes"] = std::move(schemes);
  mcp::JsonValue security(mcp::JsonValue::array_tag);
  mcp::JsonValue requirement(mcp::JsonValue::object_tag);
  requirement["bearerAuth"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  security.PushBack(std::move(requirement));
  mcp::JsonValue document(mcp::JsonValue::object_tag);
  document["openapi"] = mcp::JsonValue("3.0.3");
  document["info"] = std::move(info_object);
  document["paths"] = std::move(paths);
  document["components"] = std::move(components);
  document["security"] = std::move(security);
  return document;
}

ApiReply handle_request(const std::string &method, const std::string &path,
                        const std::unordered_map<std::string, std::string> &headers,
                        const std::string &body) {
  const RouteMatch route = match_route(method, path);
  if (route.kind == RouteKind::kNotFound) {
    return error_reply(404, kCodeNotFound, route.error);
  }
  if (route.kind == RouteKind::kMethodNotAllowed) {
    return error_reply(405, kCodeMethodNotAllowed, route.error);
  }
  if (route.kind != RouteKind::kHealth && !verify_token(headers)) {
    return error_reply(401, kCodeUnauthorized,
                       "a valid bearer token is required for this endpoint");
  }
  switch (route.kind) {
    case RouteKind::kIndex:
      return ok_reply(build_index_document());
    case RouteKind::kHealth:
      return ok_reply(build_health_document());
    case RouteKind::kOpenApi:
      return ok_reply(build_openapi_document());
    case RouteKind::kTools: {
      ListQuery query;
      std::string error;
      if (!parse_list_query(query_params_from_headers(headers), query, error)) {
        return error_reply(400, kCodeInvalidArgument, error);
      }
      const std::vector<ToolEntry> all = list_tools();
      const std::vector<ToolEntry> filtered = filter_tools(all, query);
      const std::vector<ToolEntry> page =
          paginate_tools(filtered, query.offset, query.limit);
      return ok_reply(build_tool_list_response(page, filtered.size(), query));
    }
    case RouteKind::kToolDetail: {
      for (const ToolEntry &entry : list_tools()) {
        if (entry.name == route.tool_name) {
          return ok_reply(tool_entry_to_json(entry));
        }
      }
      return error_reply(404, "tool_not_found",
                         "tool not found: " + route.tool_name);
    }
    case RouteKind::kToolCall: {
      mcp::JsonValue args;
      std::string error;
      if (!parse_body_object(body, args, error)) {
        return error_reply(400, kCodeInvalidArgument, error);
      }
      const mcp::JsonValue result = execute_tool(route.tool_name, args);
      const ApiEnvelope envelope;
      const mcp::JsonValue wrapped = envelope.build(result, route.tool_name);
      if (result.IsObject() && result.Find("error") != nullptr) {
        const std::string code = envelope.normalize_code(result);
        ApiReply reply;
        reply.status = status_for_code(code);
        reply.status_text = status_text_for(reply.status);
        reply.body = wrapped;
        return reply;
      }
      ApiReply reply;
      reply.status = 200;
      reply.status_text = status_text_for(200);
      reply.body = wrapped;
      return reply;
    }
    case RouteKind::kBatch: {
      mcp::JsonValue parsed;
      std::string error;
      if (!parse_body_object(body, parsed, error)) {
        return error_reply(400, kCodeInvalidArgument, error);
      }
      BatchRequest request;
      if (!parse_batch_request(parsed, request, error)) {
        return error_reply(400, kCodeInvalidArgument, error);
      }
      mcp::JsonValue results(mcp::JsonValue::array_tag);
      int64_t succeeded = 0;
      int64_t failed = 0;
      bool stopped = false;
      size_t executed = 0;
      for (const BatchOperation &operation : request.operations) {
        const mcp::JsonValue result = execute_tool(operation.tool, operation.args);
        const mcp::JsonValue *error_value = result.IsObject()
                                                ? result.Find("error")
                                                : nullptr;
        mcp::JsonValue item(mcp::JsonValue::object_tag);
        item["tool"] = mcp::JsonValue(operation.tool);
        if (error_value != nullptr) {
          item["status"] = mcp::JsonValue("error");
          item["error"] = *error_value;
          mcp::JsonValue error_body(mcp::JsonValue::object_tag);
          error_body["error"] = *error_value;
          item["result"] = std::move(error_body);
          ++failed;
        } else {
          item["status"] = mcp::JsonValue("ok");
          item["data"] = result;
          item["result"] = result;
          ++succeeded;
        }
        results.PushBack(std::move(item));
        ++executed;
        if (error_value != nullptr && request.stop_on_error) {
          stopped = true;
          break;
        }
      }
      mcp::JsonValue reply_body(mcp::JsonValue::object_tag);
      reply_body["results"] = std::move(results);
      reply_body["total"] = mcp::JsonValue(static_cast<int64_t>(executed));
      reply_body["succeeded"] = mcp::JsonValue(succeeded);
      reply_body["failed"] = mcp::JsonValue(failed);
      reply_body["skipped"] = mcp::JsonValue(
          static_cast<int64_t>(request.operations.size() - executed));
      if (stopped) {
        reply_body["note"] = mcp::JsonValue(
            "stopped at operation " + std::to_string(executed) + "/" +
            std::to_string(request.operations.size()) +
            " (stop_on_error=true); the remaining operations were not "
            "executed. Set stop_on_error=false to run all operations.");
      }
      return ok_reply(std::move(reply_body));
    }
    case RouteKind::kNotFound:
    case RouteKind::kMethodNotAllowed:
      break;
  }
  return error_reply(404, kCodeNotFound, "unknown API route: " + path);
}

} // namespace api_surface
} // namespace godot_autopilot
