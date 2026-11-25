#include "core/protocol/gcp_envelope.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gda {
namespace gcp {
namespace {

const std::vector<ErrorEntry> &error_entries() {
  static const std::vector<ErrorEntry> entries = {
      {ErrorCode::unsupported_version, 6, "unsupported_version"},
      {ErrorCode::auth_failed, 7, "auth_failed"},
      {ErrorCode::instance_mismatch, 8, "instance_mismatch"},
      {ErrorCode::result_unknown, 9, "result_unknown"},
      {ErrorCode::cursor_expired, 10, "cursor_expired"},
      {ErrorCode::export_in_progress, 11, "export_in_progress"},
      {ErrorCode::main_thread_timeout, 12, "main_thread_timeout"},
      {ErrorCode::confirmation_required, 13, "confirmation_required"},
      {ErrorCode::task_not_found, 14, "task_not_found"},
      {ErrorCode::backpressure_exceeded, 15, "backpressure_exceeded"},
  };
  return entries;
}

bool is_hex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

} // namespace

const char *to_string(Kind kind) {
  switch (kind) {
    case Kind::request:
      return "request";
    case Kind::response:
      return "response";
    case Kind::event:
      return "event";
  }
  return "unknown";
}

const char *to_string(Status status) {
  switch (status) {
    case Status::ok:
      return "ok";
    case Status::error:
      return "error";
    case Status::pending:
      return "pending";
    case Status::needs_confirmation:
      return "needs_confirmation";
  }
  return "unknown";
}

const char *to_string(ErrorCode code) {
  if (code == ErrorCode::none) {
    return "none";
  }
  for (const ErrorEntry &entry : error_entries()) {
    if (entry.code == code) {
      return entry.symbol;
    }
  }
  return "unknown";
}

const char *to_string(TaskState state) {
  switch (state) {
    case TaskState::pending:
      return "pending";
    case TaskState::running:
      return "running";
    case TaskState::blocked:
      return "blocked";
    case TaskState::cancelling:
      return "cancelling";
    case TaskState::cancelled:
      return "cancelled";
    case TaskState::succeeded:
      return "succeeded";
    case TaskState::failed:
      return "failed";
  }
  return "unknown";
}

const char *to_string(CancelStage stage) {
  switch (stage) {
    case CancelStage::requested:
      return "requested";
    case CancelStage::acknowledged:
      return "acknowledged";
    case CancelStage::draining:
      return "draining";
    case CancelStage::completed:
      return "completed";
  }
  return "unknown";
}

const char *to_string(Backpressure pressure) {
  switch (pressure) {
    case Backpressure::none:
      return "none";
    case Backpressure::soft:
      return "soft";
    case Backpressure::hard:
      return "hard";
  }
  return "unknown";
}

bool parse_kind(const std::string &text, Kind &out) {
  if (text == "request") {
    out = Kind::request;
    return true;
  }
  if (text == "response") {
    out = Kind::response;
    return true;
  }
  if (text == "event") {
    out = Kind::event;
    return true;
  }
  return false;
}

bool parse_status(const std::string &text, Status &out) {
  if (text == "ok") {
    out = Status::ok;
    return true;
  }
  if (text == "error") {
    out = Status::error;
    return true;
  }
  if (text == "pending") {
    out = Status::pending;
    return true;
  }
  if (text == "needs_confirmation") {
    out = Status::needs_confirmation;
    return true;
  }
  return false;
}

bool parse_error_symbol(const std::string &text, ErrorCode &out) {
  for (const ErrorEntry &entry : error_entries()) {
    if (text == entry.symbol) {
      out = entry.code;
      return true;
    }
  }
  return false;
}

int error_numeric(ErrorCode code) {
  for (const ErrorEntry &entry : error_entries()) {
    if (entry.code == code) {
      return entry.numeric;
    }
  }
  return 0;
}

ErrorCode error_from_numeric(int numeric) {
  for (const ErrorEntry &entry : error_entries()) {
    if (entry.numeric == numeric) {
      return entry.code;
    }
  }
  return ErrorCode::none;
}

const std::vector<ErrorEntry> &error_table() { return error_entries(); }

const std::vector<std::string> &all_methods() {
  static const std::vector<std::string> methods = {
      "hello",
      "capabilities.get",
      "catalog.list",
      "catalog.search",
      "catalog.describe",
      "call.start",
      "call.get",
      "call.confirm",
      "task.list",
      "task.get",
      "task.cancel",
      "events.subscribe",
      "events.resume",
      "trace.get",
  };
  return methods;
}

bool is_known_method(const std::string &method) {
  const std::vector<std::string> &methods = all_methods();
  return std::find(methods.begin(), methods.end(), method) != methods.end();
}

CapabilitySet default_capabilities() {
  CapabilitySet set;
  set.protocol_version = kEnvelopeVersion;
  set.methods = all_methods();
  set.features = {"catalog", "confirmation", "tasks", "events", "trace"};
  return set;
}

bool negotiate_capabilities(const CapabilitySet &offered,
                            const CapabilitySet &requested,
                            CapabilitySet &out) {
  out = CapabilitySet{};
  out.protocol_version = std::min(offered.protocol_version,
                                  requested.protocol_version);
  if (out.protocol_version < kEnvelopeVersion) {
    return false;
  }
  if (out.protocol_version > kEnvelopeVersion) {
    out.protocol_version = kEnvelopeVersion;
  }
  for (const std::string &method : requested.methods) {
    if (std::find(offered.methods.begin(), offered.methods.end(), method) !=
        offered.methods.end()) {
      out.methods.push_back(method);
    }
  }
  for (const std::string &feature : requested.features) {
    if (std::find(offered.features.begin(), offered.features.end(), feature) !=
        offered.features.end()) {
      out.features.push_back(feature);
    }
  }
  return true;
}

bool has_feature(const CapabilitySet &set, const std::string &feature) {
  return std::find(set.features.begin(), set.features.end(), feature) !=
         set.features.end();
}

const std::string *find_param(const Params &params, const std::string &key) {
  for (const Param &param : params) {
    if (param.key == key) {
      return &param.value;
    }
  }
  return nullptr;
}

std::string param_digest(const Params &params) {
  std::vector<const Param *> ordered;
  ordered.reserve(params.size());
  for (const Param &param : params) {
    ordered.push_back(&param);
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const Param *left, const Param *right) {
              if (left->key != right->key) {
                return left->key < right->key;
              }
              return left->value < right->value;
            });
  std::uint64_t hash = 14695981039346656037ULL;
  const std::uint64_t prime = 1099511628211ULL;
  for (const Param *param : ordered) {
    for (char c : param->key) {
      hash ^= static_cast<unsigned char>(c);
      hash *= prime;
    }
    hash ^= static_cast<unsigned char>('\n');
    hash *= prime;
    for (char c : param->value) {
      hash ^= static_cast<unsigned char>(c);
      hash *= prime;
    }
    hash ^= static_cast<unsigned char>('\n');
    hash *= prime;
  }
  static const char hex[] = "0123456789abcdef";
  std::string out;
  out.reserve(16);
  for (int shift = 60; shift >= 0; shift -= 4) {
    out.push_back(hex[(hash >> shift) & 0xFULL]);
  }
  return out;
}

Challenge make_challenge(const Params &params, std::int64_t ttl_ms,
                         std::int64_t issued_at_ms, const std::string &nonce) {
  Challenge challenge;
  challenge.ttl_ms = ttl_ms;
  challenge.issued_at_ms = issued_at_ms;
  challenge.param_digest = param_digest(params);
  challenge.nonce = nonce;
  return challenge;
}

bool challenge_fresh(const Challenge &challenge, std::int64_t now_ms) {
  if (challenge.ttl_ms <= 0 || now_ms < challenge.issued_at_ms) {
    return false;
  }
  return (now_ms - challenge.issued_at_ms) < challenge.ttl_ms;
}

bool challenge_matches(const Challenge &challenge, const Params &params) {
  return !challenge.param_digest.empty() &&
         challenge.param_digest == param_digest(params);
}

bool is_terminal_status(Status status) {
  return status == Status::ok || status == Status::error;
}

bool is_terminal_task(TaskState state) {
  return state == TaskState::cancelled || state == TaskState::succeeded ||
         state == TaskState::failed;
}

bool can_transition(TaskState from, TaskState to) {
  switch (from) {
    case TaskState::pending:
      return to == TaskState::running || to == TaskState::blocked ||
             to == TaskState::cancelling || to == TaskState::cancelled ||
             to == TaskState::failed;
    case TaskState::running:
      return to == TaskState::blocked || to == TaskState::succeeded ||
             to == TaskState::failed || to == TaskState::cancelling;
    case TaskState::blocked:
      return to == TaskState::running || to == TaskState::cancelling ||
             to == TaskState::cancelled || to == TaskState::failed;
    case TaskState::cancelling:
      return to == TaskState::cancelled || to == TaskState::failed;
    case TaskState::cancelled:
    case TaskState::succeeded:
    case TaskState::failed:
      return false;
  }
  return false;
}

bool can_advance(CancelStage from, CancelStage to) {
  if (to == from) {
    return true;
  }
  if (from == CancelStage::completed) {
    return false;
  }
  return static_cast<int>(to) == static_cast<int>(from) + 1;
}

Backpressure classify_backpressure(std::size_t pending, std::size_t soft_limit,
                                   std::size_t hard_limit) {
  if (hard_limit > 0 && pending >= hard_limit) {
    return Backpressure::hard;
  }
  if (soft_limit > 0 && pending >= soft_limit) {
    return Backpressure::soft;
  }
  return Backpressure::none;
}

bool valid_traceparent(const std::string &traceparent) {
  if (traceparent.size() != kTraceparentLength) {
    return false;
  }
  if (traceparent[0] != '0' || traceparent[1] != '0') {
    return false;
  }
  if (traceparent[2] != '-' || traceparent[35] != '-' ||
      traceparent[52] != '-') {
    return false;
  }
  for (std::size_t i = 3; i < 35; ++i) {
    if (!is_hex(traceparent[i])) {
      return false;
    }
  }
  for (std::size_t i = 36; i < 52; ++i) {
    if (!is_hex(traceparent[i])) {
      return false;
    }
  }
  for (std::size_t i = 53; i < 55; ++i) {
    if (!is_hex(traceparent[i])) {
      return false;
    }
  }
  return true;
}

std::vector<std::string> validate_envelope(const GcpEnvelope &envelope) {
  std::vector<std::string> violations;
  if (envelope.v != kEnvelopeVersion) {
    violations.push_back("v must equal 1");
  }
  if (envelope.id.empty() || envelope.id.size() > kMaxIdLength) {
    violations.push_back("id must be non-empty and bounded");
  }
  if (envelope.instance_id.empty()) {
    violations.push_back("instance_id must be non-empty");
  }
  if (envelope.params.size() > kMaxParamCount) {
    violations.push_back("params exceed limit");
  }
  for (const Param &param : envelope.params) {
    if (param.key.empty()) {
      violations.push_back("param key must be non-empty");
      break;
    }
  }
  if (!valid_traceparent(envelope.trace.traceparent)) {
    violations.push_back("trace.traceparent is malformed");
  }
  switch (envelope.kind) {
    case Kind::request:
      if (!is_known_method(envelope.method)) {
        violations.push_back("request method is not in the method set");
      }
      if (envelope.deadline_ms <= 0) {
        violations.push_back("request deadline_ms must be positive");
      }
      break;
    case Kind::response:
      if (!is_known_method(envelope.method)) {
        violations.push_back("response method is not in the method set");
      }
      if (!envelope.request_id.has_value() || envelope.request_id->empty()) {
        violations.push_back("response request_id must be non-empty");
      }
      if (envelope.status == Status::error &&
          (envelope.error == ErrorCode::none ||
           envelope.error_message.empty())) {
        violations.push_back("error response needs code and message");
      }
      if (envelope.status == Status::needs_confirmation &&
          (envelope.confirmation.ttl_ms <= 0 ||
           envelope.confirmation.param_digest.empty())) {
        violations.push_back("confirmation response needs challenge");
      }
      break;
    case Kind::event:
      if (envelope.seq == 0) {
        violations.push_back("event seq must be monotonic from 1");
      }
      if (envelope.event_type.empty()) {
        violations.push_back("event type must be non-empty");
      }
      if (envelope.task_id.has_value() && envelope.task_id->empty()) {
        violations.push_back("event task_id must be non-empty when present");
      }
      if (envelope.request_id.has_value() && envelope.request_id->empty()) {
        violations.push_back("event request_id must be non-empty when present");
      }
      break;
  }
  return violations;
}

} // namespace gcp
} // namespace gda
