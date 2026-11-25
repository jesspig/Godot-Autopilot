#include "core/control/gcp_dispatcher.hpp"

#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/control/control_types.hpp"
#include "core/control/trace_service.hpp"
#include "core/protocol/gcp_envelope.hpp"
#include "util/json_value.hpp"

namespace gda {
namespace control {
namespace {

using godot_autopilot::util::DuplicateKeyPolicy;
using godot_autopilot::util::JsonValue;

const char *const kParamResult = "result";

std::int64_t wall_clock_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string fresh_traceparent() {
  return TraceService::make_traceparent(TraceService::new_trace_id(),
                                        TraceService::new_span_id());
}

bool read_string(const JsonValue &doc, const char *key, std::string &out) {
  const JsonValue *value = doc.Find(key);
  if (value == nullptr || !value->IsString()) {
    return false;
  }
  out = value->GetString();
  return true;
}

bool read_int(const JsonValue &doc, const char *key, std::int64_t &out) {
  const JsonValue *value = doc.Find(key);
  if (value == nullptr || !value->IsInt()) {
    return false;
  }
  out = value->GetInt();
  return true;
}

std::string string_param(const gcp::Params &params, const std::string &key,
                         const std::string &fallback) {
  const std::string *value = gcp::find_param(params, key);
  return value == nullptr ? fallback : *value;
}

bool sensitive_key(const std::string &key) {
  std::string lowered;
  for (char character : key) {
    lowered.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }
  return lowered.find("token") != std::string::npos ||
         lowered.find("secret") != std::string::npos ||
         lowered.find("password") != std::string::npos ||
         lowered.find("credential") != std::string::npos ||
         lowered.find("api_key") != std::string::npos ||
         lowered.find("apikey") != std::string::npos;
}

gcp::Params without_secrets(const gcp::Params &params) {
  gcp::Params out;
  for (const gcp::Param &param : params) {
    if (!sensitive_key(param.key)) {
      out.push_back(param);
    }
  }
  return out;
}

std::vector<std::string> split_csv(const std::string &text) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t comma = text.find(',', start);
    if (comma == std::string::npos) {
      out.push_back(text.substr(start));
      break;
    }
    if (comma > start) {
      out.push_back(text.substr(start, comma - start));
    }
    start = comma + 1;
  }
  return out;
}

JsonValue string_array_json(const std::vector<std::string> &values) {
  JsonValue out(JsonValue::array_tag);
  for (const std::string &value : values) {
    out.PushBack(JsonValue(value));
  }
  return out;
}

JsonValue capabilities_json(const gcp::CapabilitySet &set) {
  JsonValue out(JsonValue::object_tag);
  out["protocol_version"] =
      JsonValue(static_cast<std::int64_t>(set.protocol_version));
  out["methods"] = string_array_json(set.methods);
  out["features"] = string_array_json(set.features);
  return out;
}

JsonValue catalog_entries_json(const std::vector<CatalogEntry> &entries) {
  JsonValue out(JsonValue::array_tag);
  for (const CatalogEntry &entry : entries) {
    JsonValue item(JsonValue::object_tag);
    item["name"] = JsonValue(entry.name);
    item["description"] = JsonValue(entry.description);
    item["category"] = JsonValue(entry.category);
    item["tags"] = string_array_json(entry.tags);
    item["aliases"] = string_array_json(entry.aliases);
    item["mutating"] = JsonValue(entry.mutating);
    item["high_risk"] = JsonValue(entry.high_risk);
    out.PushBack(std::move(item));
  }
  return out;
}

JsonValue catalog_scores_json(const std::vector<CatalogScore> &scores) {
  JsonValue out(JsonValue::array_tag);
  for (const CatalogScore &score : scores) {
    JsonValue item(JsonValue::object_tag);
    item["name"] = JsonValue(score.entry.name);
    item["description"] = JsonValue(score.entry.description);
    item["category"] = JsonValue(score.entry.category);
    item["score"] = JsonValue(score.score);
    out.PushBack(std::move(item));
  }
  return out;
}

JsonValue task_json(const TaskRecord &record) {
  JsonValue out(JsonValue::object_tag);
  out["task_id"] = JsonValue(record.key.task_id);
  out["instance_id"] = JsonValue(record.key.instance_id);
  out["generation"] = JsonValue(record.key.generation);
  out["state"] = JsonValue(gcp::to_string(record.state));
  out["progress"] = JsonValue(static_cast<std::int64_t>(record.progress));
  out["progress_message"] = JsonValue(record.progress_message);
  out["result"] = JsonValue(record.result);
  out["error"] = JsonValue(gcp::error_numeric(record.error));
  out["error_symbol"] = JsonValue(gcp::to_string(record.error));
  out["error_message"] = JsonValue(record.error_message);
  out["cancel_stage"] = JsonValue(gcp::to_string(record.cancel_stage));
  return out;
}

JsonValue invocation_json(const InvocationResult &result) {
  JsonValue out(JsonValue::object_tag);
  out["status"] = JsonValue(gcp::to_string(result.status));
  out["payload"] = JsonValue(result.payload);
  out["artifact_id"] = JsonValue(result.artifact.id);
  out["digest"] = JsonValue(result.artifact.digest);
  out["bytes"] = JsonValue(static_cast<std::int64_t>(result.artifact.bytes));
  out["task_id"] = JsonValue(result.task_id);
  return out;
}

JsonValue event_record_json(const EventRecord &record) {
  JsonValue out(JsonValue::object_tag);
  out["seq"] = JsonValue(static_cast<std::int64_t>(record.seq));
  out["instance_id"] = JsonValue(record.instance_id);
  out["event_type"] = JsonValue(record.event_type);
  out["payload"] = JsonValue(record.payload);
  out["traceparent"] = JsonValue(record.traceparent);
  if (record.task_id.has_value()) {
    out["task_id"] = JsonValue(*record.task_id);
  }
  if (record.request_id.has_value()) {
    out["request_id"] = JsonValue(*record.request_id);
  }
  return out;
}

gcp::GcpEnvelope event_envelope(const EventRecord &record) {
  gcp::GcpEnvelope event;
  event.v = gcp::kEnvelopeVersion;
  event.kind = gcp::Kind::event;
  event.id = "evt-" + std::to_string(record.seq);
  event.instance_id = record.instance_id;
  event.seq = record.seq;
  event.event_type = record.event_type;
  event.trace.traceparent = record.traceparent.empty()
                                ? fresh_traceparent()
                                : record.traceparent;
  event.task_id = record.task_id;
  event.request_id = record.request_id;
  return event;
}

} // namespace

GcpDispatcher::GcpDispatcher(ControlService &service,
                             const std::string &instance_id,
                             InvocationSink *sink)
    : service_(service), instance_id_(instance_id), sink_(sink) {
  if (sink_ != nullptr) {
    service_.invocation().set_sink(sink_);
  }
}

void GcpDispatcher::set_sink(InvocationSink *sink) {
  sink_ = sink;
  if (sink_ != nullptr) {
    service_.invocation().set_sink(sink_);
  }
}

void GcpDispatcher::set_instance_id(const std::string &instance_id) {
  instance_id_ = instance_id;
}

bool GcpDispatcher::looks_like_auth_frame(const std::string &text) {
  JsonValue doc;
  try {
    doc = JsonValue::Parse(text, DuplicateKeyPolicy::strict);
  } catch (...) {
    return false;
  }
  if (!doc.IsObject()) {
    return false;
  }
  const JsonValue *method = doc.Find("method");
  if (method != nullptr && method->IsString() &&
      method->GetString() == "initialize") {
    return true;
  }
  const JsonValue *jsonrpc = doc.Find("jsonrpc");
  if (jsonrpc != nullptr && jsonrpc->IsString()) {
    return true;
  }
  const JsonValue *params = doc.Find("params");
  if (params != nullptr && params->IsObject()) {
    if (params->Find("gda_token") != nullptr) {
      return true;
    }
    const JsonValue *meta = params->Find("_meta");
    if (meta != nullptr && meta->IsObject() &&
        meta->Find("gda_token") != nullptr) {
      return true;
    }
  }
  return false;
}

bool GcpDispatcher::looks_like_gcp(const std::string &text) {
  JsonValue doc;
  try {
    doc = JsonValue::Parse(text, DuplicateKeyPolicy::strict);
  } catch (...) {
    return false;
  }
  if (!doc.IsObject()) {
    return false;
  }
  const JsonValue *version = doc.Find("v");
  const JsonValue *kind = doc.Find("kind");
  return version != nullptr && version->IsInt() && kind != nullptr &&
         kind->IsString();
}

bool GcpDispatcher::encode(const gcp::GcpEnvelope &envelope, std::string &out) {
  JsonValue doc(JsonValue::object_tag);
  doc["v"] = JsonValue(envelope.v);
  doc["kind"] = JsonValue(gcp::to_string(envelope.kind));
  doc["id"] = JsonValue(envelope.id);
  doc["instance_id"] = JsonValue(envelope.instance_id);
  doc["method"] = JsonValue(envelope.method);
  JsonValue params(JsonValue::array_tag);
  for (const gcp::Param &param : envelope.params) {
    JsonValue item(JsonValue::object_tag);
    item["key"] = JsonValue(param.key);
    item["value"] = JsonValue(param.value);
    params.PushBack(std::move(item));
  }
  doc["params"] = std::move(params);
  JsonValue trace(JsonValue::object_tag);
  trace["traceparent"] = JsonValue(envelope.trace.traceparent);
  doc["trace"] = std::move(trace);
  doc["deadline_ms"] = JsonValue(envelope.deadline_ms);
  if (envelope.request_id.has_value()) {
    doc["request_id"] = JsonValue(*envelope.request_id);
  }
  doc["status"] = JsonValue(gcp::to_string(envelope.status));
  doc["error"] = JsonValue(gcp::error_numeric(envelope.error));
  doc["error_message"] = JsonValue(envelope.error_message);
  JsonValue confirmation(JsonValue::object_tag);
  confirmation["ttl_ms"] = JsonValue(envelope.confirmation.ttl_ms);
  confirmation["issued_at_ms"] = JsonValue(envelope.confirmation.issued_at_ms);
  confirmation["param_digest"] = JsonValue(envelope.confirmation.param_digest);
  confirmation["nonce"] = JsonValue(envelope.confirmation.nonce);
  doc["confirmation"] = std::move(confirmation);
  doc["seq"] = JsonValue(static_cast<std::int64_t>(envelope.seq));
  doc["event_type"] = JsonValue(envelope.event_type);
  if (envelope.task_id.has_value()) {
    doc["task_id"] = JsonValue(*envelope.task_id);
  }
  out = doc.Dump();
  return true;
}

bool GcpDispatcher::decode(const std::string &text, gcp::GcpEnvelope &out,
                           std::string &error) {
  out = gcp::GcpEnvelope{};
  JsonValue doc;
  try {
    doc = JsonValue::Parse(text, DuplicateKeyPolicy::strict);
  } catch (...) {
    error = "malformed JSON text";
    return false;
  }
  if (!doc.IsObject()) {
    error = "envelope must be a JSON object";
    return false;
  }
  std::int64_t version = 0;
  if (!read_int(doc, "v", version)) {
    error = "v must be an integer";
    return false;
  }
  out.v = static_cast<int>(version);
  std::string kind;
  if (!read_string(doc, "kind", kind) || !gcp::parse_kind(kind, out.kind)) {
    error = "kind is invalid";
    return false;
  }
  if (!read_string(doc, "id", out.id)) {
    error = "id must be a string";
    return false;
  }
  if (!read_string(doc, "instance_id", out.instance_id)) {
    error = "instance_id must be a string";
    return false;
  }
  read_string(doc, "method", out.method);
  const JsonValue *trace = doc.Find("trace");
  if (trace != nullptr && trace->IsObject()) {
    read_string(*trace, "traceparent", out.trace.traceparent);
  }
  const JsonValue *params = doc.Find("params");
  if (params != nullptr && params->IsArray()) {
    for (std::size_t index = 0; index < params->Size(); ++index) {
      const JsonValue &item = (*params)[index];
      if (!item.IsObject()) {
        continue;
      }
      std::string key;
      std::string value;
      if (read_string(item, "key", key) && read_string(item, "value", value)) {
        out.params.push_back(gcp::Param{key, value});
      }
    }
  }
  read_int(doc, "deadline_ms", out.deadline_ms);
  std::string request_id;
  if (read_string(doc, "request_id", request_id)) {
    out.request_id = request_id;
  }
  std::string status;
  if (read_string(doc, "status", status) &&
      !gcp::parse_status(status, out.status)) {
    error = "status is invalid";
    return false;
  }
  std::int64_t numeric = 0;
  if (read_int(doc, "error", numeric)) {
    out.error = gcp::error_from_numeric(static_cast<int>(numeric));
  }
  read_string(doc, "error_message", out.error_message);
  std::int64_t seq = 0;
  if (read_int(doc, "seq", seq)) {
    out.seq = static_cast<std::uint64_t>(seq);
  }
  read_string(doc, "event_type", out.event_type);
  std::string task_id;
  if (read_string(doc, "task_id", task_id)) {
    out.task_id = task_id;
  }
  const JsonValue *confirmation = doc.Find("confirmation");
  if (confirmation != nullptr && confirmation->IsObject()) {
    read_int(*confirmation, "ttl_ms", out.confirmation.ttl_ms);
    read_int(*confirmation, "issued_at_ms", out.confirmation.issued_at_ms);
    read_string(*confirmation, "param_digest", out.confirmation.param_digest);
    read_string(*confirmation, "nonce", out.confirmation.nonce);
  }
  return true;
}

gcp::GcpEnvelope
GcpDispatcher::response_envelope(const gcp::GcpEnvelope &request,
                                 gcp::Status status) const {
  gcp::GcpEnvelope out;
  out.v = gcp::kEnvelopeVersion;
  out.kind = gcp::Kind::response;
  const std::string id = "r-" + request.id;
  out.id = id.size() <= gcp::kMaxIdLength ? id : id.substr(0, gcp::kMaxIdLength);
  out.instance_id =
      request.instance_id.empty() ? instance_id_ : request.instance_id;
  out.method = request.method;
  out.trace = request.trace;
  if (!gcp::valid_traceparent(out.trace.traceparent)) {
    out.trace.traceparent = fresh_traceparent();
  }
  out.deadline_ms =
      request.deadline_ms > 0 ? request.deadline_ms : gcp::kDefaultDeadlineMs;
  out.request_id = request.id.empty() ? std::string("unknown") : request.id;
  out.status = status;
  return out;
}

DispatchResult
GcpDispatcher::complete(const gcp::GcpEnvelope &request, gcp::Status status,
                        const std::string &result_json, gcp::ErrorCode error,
                        const std::string &message,
                        const gcp::Challenge &confirmation,
                        const std::string &task_id) {
  gcp::GcpEnvelope out = response_envelope(request, status);
  if (!result_json.empty()) {
    out.params.push_back(gcp::Param{kParamResult, result_json});
  }
  out.error = error;
  out.error_message = message;
  out.confirmation = confirmation;
  if (!task_id.empty()) {
    out.task_id = task_id;
  }
  DispatchResult result;
  if (!encode(out, result.response_text)) {
    result.response_text.clear();
    result.state = DispatchState::decode_failed;
    result.error = gcp::ErrorCode::result_unknown;
    result.error_message = "response encoding failed";
    return result;
  }
  state_ = DispatchState::terminal;
  result.state = DispatchState::terminal;
  result.status = status;
  result.error = error;
  result.error_message = message;
  result.request_id = request.id;
  result.task_id = task_id;
  return result;
}

DispatchResult GcpDispatcher::fail(const gcp::GcpEnvelope &request,
                                   gcp::ErrorCode code,
                                   const std::string &message,
                                   DispatchState state) {
  DispatchResult result =
      complete(request, gcp::Status::error, std::string(), code, message);
  result.state = state;
  state_ = state;
  return result;
}

DispatchResult GcpDispatcher::dispatch_request(std::uint64_t connection_id,
                                               const gcp::GcpEnvelope &request) {
  if (completed_.find(request.id) != completed_.end()) {
    return fail(request, gcp::ErrorCode::result_unknown,
                "request already written; result unknown",
                DispatchState::duplicate_rejected);
  }
  if (request.v != gcp::kEnvelopeVersion) {
    return fail(request, gcp::ErrorCode::unsupported_version,
                "unsupported envelope version",
                DispatchState::validation_failed);
  }
  if (request.instance_id != instance_id_) {
    return fail(request, gcp::ErrorCode::instance_mismatch,
                "instance id mismatch", DispatchState::validation_failed);
  }
  if (request.kind != gcp::Kind::request) {
    return fail(request, gcp::ErrorCode::result_unknown,
                "only request envelopes are dispatched",
                DispatchState::validation_failed);
  }
  const std::vector<std::string> violations = gcp::validate_envelope(request);
  if (!violations.empty()) {
    return fail(request, gcp::ErrorCode::result_unknown, violations.front(),
                DispatchState::validation_failed);
  }
  state_ = DispatchState::dispatched;
  const std::string &method = request.method;

  if (method == "hello") {
    JsonValue result(JsonValue::object_tag);
    result["instance_id"] = JsonValue(instance_id_);
    result["capabilities"] = capabilities_json(gcp::default_capabilities());
    return complete(request, gcp::Status::ok, result.Dump());
  }

  if (method == "capabilities.get") {
    gcp::CapabilitySet requested;
    std::int64_t version = gcp::kEnvelopeVersion;
    const std::string *version_text =
        gcp::find_param(request.params, "protocol_version");
    if (version_text != nullptr) {
      try {
        version = std::stoll(*version_text);
      } catch (...) {
      }
    }
    requested.protocol_version = static_cast<int>(version);
    const std::string *methods = gcp::find_param(request.params, "methods");
    if (methods != nullptr) {
      requested.methods = split_csv(*methods);
    }
    const std::string *features = gcp::find_param(request.params, "features");
    if (features != nullptr) {
      requested.features = split_csv(*features);
    }
    gcp::CapabilitySet negotiated;
    if (!gcp::negotiate_capabilities(gcp::default_capabilities(), requested,
                                     negotiated)) {
      return fail(request, gcp::ErrorCode::unsupported_version,
                  "protocol version not supported",
                  DispatchState::validation_failed);
    }
    return complete(request, gcp::Status::ok,
                    capabilities_json(negotiated).Dump());
  }

  if (method == "catalog.list") {
    const std::string category =
        string_param(request.params, "category", std::string());
    const std::size_t limit = static_cast<std::size_t>(
        param_int(request.params, "limit", 20));
    const std::size_t offset = static_cast<std::size_t>(
        param_int(request.params, "offset", 0));
    return complete(request, gcp::Status::ok,
                    catalog_entries_json(
                        service_.catalog().list(category, limit, offset))
                        .Dump());
  }

  if (method == "catalog.search") {
    CatalogQuery query;
    query.text = string_param(request.params, "text", std::string());
    query.category = string_param(request.params, "category", std::string());
    const std::string *tags = gcp::find_param(request.params, "tags");
    if (tags != nullptr) {
      query.tags = split_csv(*tags);
    }
    query.limit =
        static_cast<std::size_t>(param_int(request.params, "limit", 20));
    query.offset =
        static_cast<std::size_t>(param_int(request.params, "offset", 0));
    return complete(request, gcp::Status::ok,
                    catalog_scores_json(service_.catalog().search(query))
                        .Dump());
  }

  if (method == "catalog.describe") {
    const std::optional<CatalogEntry> found = service_.catalog().describe(
        string_param(request.params, "name", std::string()));
    JsonValue result(JsonValue::object_tag);
    result["found"] = JsonValue(found.has_value());
    if (found.has_value()) {
      result["name"] = JsonValue(found->name);
      result["description"] = JsonValue(found->description);
      result["category"] = JsonValue(found->category);
      result["tags"] = string_array_json(found->tags);
      result["aliases"] = string_array_json(found->aliases);
      result["mutating"] = JsonValue(found->mutating);
      result["high_risk"] = JsonValue(found->high_risk);
    }
    return complete(request, gcp::Status::ok, result.Dump());
  }

  if (method == "call.start") {
    InvocationRequest invocation;
    invocation.request_id = request.id;
    invocation.instance_id = request.instance_id;
    invocation.generation = param_int(request.params, "generation", 0);
    invocation.tool = string_param(request.params, "tool", std::string());
    invocation.params = without_secrets(request.params);
    invocation.traceparent = request.trace.traceparent;
    invocation.deadline_ms = request.deadline_ms;
    invocation.deferred = false;
    if (invocation.tool.empty()) {
      return fail(request, gcp::ErrorCode::result_unknown, "tool is required",
                  DispatchState::validation_failed);
    }
    const InvocationResult outcome = service_.invocation().invoke(invocation);
    if (outcome.status == gcp::Status::needs_confirmation) {
      originals_[request.id] = invocation;
      return complete(request, gcp::Status::needs_confirmation, std::string(),
                      gcp::ErrorCode::confirmation_required,
                      outcome.error_message, outcome.confirmation);
    }
    if (outcome.status == gcp::Status::error) {
      return complete(request, gcp::Status::error, std::string(), outcome.error,
                      outcome.error_message);
    }
    if (outcome.status == gcp::Status::pending && !outcome.task_id.empty()) {
      const TaskKey key{invocation.instance_id, invocation.generation,
                        outcome.task_id};
      const std::int64_t now = wall_clock_ms();
      if (!service_.jobs().exists(key)) {
        service_.jobs().create(invocation.instance_id, invocation.generation,
                               outcome.task_id, 0, now);
      }
      service_.jobs().advance(key, gcp::TaskState::running, now);
      JsonValue pending(JsonValue::object_tag);
      pending["task_id"] = JsonValue(outcome.task_id);
      pending["state"] = JsonValue(gcp::to_string(gcp::TaskState::running));
      DispatchResult result =
          complete(request, gcp::Status::pending, pending.Dump(),
                   gcp::ErrorCode::none, std::string(), gcp::Challenge{},
                   outcome.task_id);
      result.state = DispatchState::awaiting;
      state_ = DispatchState::awaiting;
      return result;
    }
    return complete(request, gcp::Status::ok, invocation_json(outcome).Dump());
  }

  if (method == "call.get" || method == "task.get") {
    const std::string task_id =
        string_param(request.params, "task_id", std::string());
    if (task_id.empty()) {
      return fail(request, gcp::ErrorCode::task_not_found,
                  "task_id is required", DispatchState::validation_failed);
    }
    const TaskKey key{request.instance_id,
                      param_int(request.params, "generation", 0), task_id};
    const std::optional<TaskRecord> record = service_.jobs().get(key);
    if (!record.has_value()) {
      return fail(request, gcp::ErrorCode::task_not_found, "task not found",
                  DispatchState::validation_failed);
    }
    return complete(request, gcp::Status::ok, task_json(*record).Dump());
  }

  if (method == "call.confirm") {
    const std::string request_id =
        string_param(request.params, "request_id", std::string());
    std::map<std::string, InvocationRequest>::iterator found =
        originals_.find(request_id);
    if (request_id.empty() || found == originals_.end()) {
      return fail(request, gcp::ErrorCode::confirmation_required,
                  "no pending confirmation", DispatchState::validation_failed);
    }
    const InvocationRequest original = found->second;
    originals_.erase(found);
    const InvocationResult outcome =
        service_.invocation().confirm(request_id, original, original.params);
    if (outcome.status == gcp::Status::error) {
      return complete(request, gcp::Status::error, std::string(), outcome.error,
                      outcome.error_message);
    }
    return complete(request, outcome.status, invocation_json(outcome).Dump(),
                    outcome.error, outcome.error_message, outcome.confirmation,
                    outcome.task_id);
  }

  if (method == "task.list") {
    const std::vector<TaskRecord> records = service_.jobs().list(
        request.instance_id, param_int(request.params, "generation", 0));
    JsonValue out(JsonValue::array_tag);
    for (const TaskRecord &record : records) {
      out.PushBack(task_json(record));
    }
    return complete(request, gcp::Status::ok, out.Dump());
  }

  if (method == "task.cancel") {
    const InvocationResult outcome = service_.invocation().cancel(
        request.instance_id, param_int(request.params, "generation", 0),
        string_param(request.params, "task_id", std::string()));
    if (outcome.status == gcp::Status::error) {
      return complete(request, gcp::Status::error, std::string(), outcome.error,
                      outcome.error_message);
    }
    return complete(request, gcp::Status::ok, invocation_json(outcome).Dump());
  }

  if (method == "events.subscribe") {
    if (event_pressure() == gcp::Backpressure::hard) {
      return fail(request, gcp::ErrorCode::backpressure_exceeded,
                  "event outbox hard limit reached",
                  DispatchState::validation_failed);
    }
    const std::uint64_t cursor = static_cast<std::uint64_t>(
        param_int(request.params, "cursor", 0));
    if (!service_.events().cursor_valid(request.instance_id, cursor)) {
      return fail(request, gcp::ErrorCode::cursor_expired, "cursor expired",
                  DispatchState::validation_failed);
    }
    const std::string type =
        string_param(request.params, "event_type", std::string("all"));
    const std::string id =
        service_.events().subscribe(request.instance_id, type, cursor);
    JsonValue result(JsonValue::object_tag);
    result["subscription_id"] = JsonValue(id);
    return complete(request, gcp::Status::ok, result.Dump());
  }

  if (method == "events.resume") {
    const std::string id =
        string_param(request.params, "subscription_id", std::string());
    if (id.empty()) {
      return fail(request, gcp::ErrorCode::result_unknown,
                  "subscription_id is required",
                  DispatchState::validation_failed);
    }
    gcp::Backpressure pressure = gcp::Backpressure::none;
    const std::size_t limit = static_cast<std::size_t>(
        param_int(request.params, "limit", 20));
    const std::vector<EventRecord> records =
        service_.events().poll(id, limit, &pressure);
    if (pressure == gcp::Backpressure::hard) {
      return fail(request, gcp::ErrorCode::backpressure_exceeded,
                  "event outbox hard limit reached",
                  DispatchState::validation_failed);
    }
    JsonValue delivered(JsonValue::array_tag);
    for (const EventRecord &record : records) {
      delivered.PushBack(event_record_json(record));
      enqueue_event(event_envelope(record), connection_id);
    }
    JsonValue result(JsonValue::object_tag);
    result["delivered"] = JsonValue(static_cast<std::int64_t>(records.size()));
    result["events"] = std::move(delivered);
    return complete(request, gcp::Status::ok, result.Dump());
  }

  if (method == "trace.get") {
    const std::string traceparent =
        string_param(request.params, "traceparent", request.trace.traceparent);
    SpanContext context = service_.trace().parse(traceparent);
    if (context.traceparent.empty()) {
      context = service_.trace().current();
    }
    JsonValue result(JsonValue::object_tag);
    result["traceparent"] = JsonValue(context.traceparent);
    result["trace_id"] = JsonValue(context.trace_id);
    result["span_id"] = JsonValue(context.span_id);
    result["parent_span"] = JsonValue(context.parent_span);
    result["flags"] = JsonValue(context.flags);
    return complete(request, gcp::Status::ok, result.Dump());
  }

  return fail(request, gcp::ErrorCode::result_unknown, "unsupported method",
              DispatchState::validation_failed);
}

DispatchResult GcpDispatcher::handle_frame(std::uint64_t connection_id,
                                           const std::string &text) {
  state_ = DispatchState::idle;
  if (looks_like_auth_frame(text)) {
    state_ = DispatchState::auth_ignored;
    DispatchResult result;
    result.state = state_;
    result.connection_id = connection_id;
    return result;
  }
  gcp::GcpEnvelope request;
  std::string error;
  if (!decode(text, request, error)) {
    DispatchResult result = fail(
        request, gcp::ErrorCode::result_unknown, error,
        DispatchState::decode_failed);
    result.connection_id = connection_id;
    completed_.insert(request.id.empty() ? std::string("decode-error")
                                         : request.id);
    return result;
  }
  DispatchResult result = dispatch_request(connection_id, request);
  result.connection_id = connection_id;
  if (!result.response_text.empty()) {
    completed_.insert(request.id);
  }
  return result;
}

bool GcpDispatcher::publish_event(const std::string &event_type,
                                  const std::string &payload,
                                  std::uint64_t connection_id,
                                  const std::string &traceparent,
                                  const std::string &task_id,
                                  const std::string &request_id) {
  const EventRecord record = service_.events().publish(
      instance_id_, event_type, payload, traceparent,
      task_id.empty() ? std::nullopt : std::optional<std::string>(task_id),
      request_id.empty() ? std::nullopt
                         : std::optional<std::string>(request_id));
  return enqueue_event(event_envelope(record), connection_id);
}

bool GcpDispatcher::enqueue_event(const gcp::GcpEnvelope &event,
                                  std::uint64_t connection_id) {
  if (outbox_.size() >= kEventOutboxHardLimit) {
    return false;
  }
  std::string encoded;
  if (!encode(event, encoded)) {
    return false;
  }
  outbox_.push_back(OutboxEntry{connection_id, std::move(encoded)});
  return true;
}

std::size_t GcpDispatcher::pending_events() const { return outbox_.size(); }

gcp::Backpressure GcpDispatcher::event_pressure() const {
  return gcp::classify_backpressure(outbox_.size(), kEventOutboxSoftLimit,
                                    kEventOutboxHardLimit);
}

std::vector<OutboxEntry> GcpDispatcher::drain_events() {
  std::vector<OutboxEntry> drained;
  drained.swap(outbox_);
  return drained;
}

void GcpDispatcher::mark_written(const std::string &request_id) {
  if (!request_id.empty()) {
    completed_.insert(request_id);
  }
}

bool GcpDispatcher::was_written(const std::string &request_id) const {
  return completed_.find(request_id) != completed_.end();
}

std::size_t GcpDispatcher::completed_count() const { return completed_.size(); }

DispatchState GcpDispatcher::state() const { return state_; }

void GcpDispatcher::reset() {
  originals_.clear();
  completed_.clear();
  outbox_.clear();
  state_ = DispatchState::idle;
}

ControlService &GcpDispatcher::service() { return service_; }

const ControlService &GcpDispatcher::service() const { return service_; }

} // namespace control
} // namespace gda
