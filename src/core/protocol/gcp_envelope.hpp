#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gda {
namespace gcp {

constexpr int kEnvelopeVersion = 1;
constexpr std::int64_t kDefaultDeadlineMs = 30000;
constexpr std::size_t kMaxParamCount = 64;
constexpr std::size_t kMaxIdLength = 128;
constexpr std::size_t kTraceparentLength = 55;

enum class Kind { request, response, event };

enum class Status { ok, error, pending, needs_confirmation };

enum class ErrorCode {
  none = 0,
  unsupported_version = 6,
  auth_failed = 7,
  instance_mismatch = 8,
  result_unknown = 9,
  cursor_expired = 10,
  export_in_progress = 11,
  main_thread_timeout = 12,
  confirmation_required = 13,
  task_not_found = 14,
  backpressure_exceeded = 15,
};

enum class TaskState {
  pending,
  running,
  blocked,
  cancelling,
  cancelled,
  succeeded,
  failed,
};

enum class CancelStage { requested, acknowledged, draining, completed };

enum class Backpressure { none, soft, hard };

struct Param {
  std::string key;
  std::string value;
};

using Params = std::vector<Param>;

struct TraceContext {
  std::string traceparent;
};

struct Challenge {
  std::int64_t ttl_ms = 0;
  std::int64_t issued_at_ms = 0;
  std::string param_digest;
  std::string nonce;
};

struct GcpEnvelope {
  int v = kEnvelopeVersion;
  Kind kind = Kind::request;
  std::string id;
  std::string instance_id;
  std::string method;
  Params params;
  TraceContext trace;
  std::int64_t deadline_ms = kDefaultDeadlineMs;
  std::optional<std::string> request_id;
  Status status = Status::pending;
  ErrorCode error = ErrorCode::none;
  std::string error_message;
  Challenge confirmation;
  std::uint64_t seq = 0;
  std::string event_type;
  std::optional<std::string> task_id;
};

struct CapabilitySet {
  int protocol_version = kEnvelopeVersion;
  std::vector<std::string> methods;
  std::vector<std::string> features;
};

struct ErrorEntry {
  ErrorCode code;
  int numeric;
  const char *symbol;
};

struct EventCursor {
  std::string instance_id;
  std::uint64_t seq = 0;
};

const char *to_string(Kind kind);
const char *to_string(Status status);
const char *to_string(ErrorCode code);
const char *to_string(TaskState state);
const char *to_string(CancelStage stage);
const char *to_string(Backpressure pressure);

bool parse_kind(const std::string &text, Kind &out);
bool parse_status(const std::string &text, Status &out);
bool parse_error_symbol(const std::string &text, ErrorCode &out);
int error_numeric(ErrorCode code);
ErrorCode error_from_numeric(int numeric);
const std::vector<ErrorEntry> &error_table();

bool is_known_method(const std::string &method);
const std::vector<std::string> &all_methods();
CapabilitySet default_capabilities();
bool negotiate_capabilities(const CapabilitySet &offered,
                            const CapabilitySet &requested,
                            CapabilitySet &out);
bool has_feature(const CapabilitySet &set, const std::string &feature);

const std::string *find_param(const Params &params, const std::string &key);
std::string param_digest(const Params &params);
Challenge make_challenge(const Params &params, std::int64_t ttl_ms,
                         std::int64_t issued_at_ms, const std::string &nonce);
bool challenge_fresh(const Challenge &challenge, std::int64_t now_ms);
bool challenge_matches(const Challenge &challenge, const Params &params);

bool is_terminal_status(Status status);
bool is_terminal_task(TaskState state);
bool can_transition(TaskState from, TaskState to);
bool can_advance(CancelStage from, CancelStage to);

Backpressure classify_backpressure(std::size_t pending, std::size_t soft_limit,
                                   std::size_t hard_limit);

bool valid_traceparent(const std::string &traceparent);
std::vector<std::string> validate_envelope(const GcpEnvelope &envelope);

} // namespace gcp
} // namespace gda
