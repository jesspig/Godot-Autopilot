#include "core/control/invocation_service.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/control/control_types.hpp"
#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {

FixedClock::FixedClock(std::int64_t now_ms) : now_ms_(now_ms) {}

void FixedClock::set(std::int64_t now_ms) { now_ms_ = now_ms; }

std::int64_t FixedClock::now_ms() const { return now_ms_; }

void StaticExportProbe::set(bool exporting) { exporting_ = exporting; }

bool StaticExportProbe::exporting() const { return exporting_; }

void StaticRiskProbe::add(const std::string &tool) { tools_.push_back(tool); }

bool StaticRiskProbe::high_risk(const std::string &tool) const {
  for (const std::string &candidate : tools_) {
    if (candidate == tool) {
      return true;
    }
  }
  return false;
}

InvocationService::InvocationService(InvocationDeps deps) : deps_(deps) {}

void InvocationService::set_clock(Clock *clock) { deps_.clock = clock; }

void InvocationService::set_export_probe(ExportProbe *probe) {
  deps_.export_probe = probe;
}

void InvocationService::set_risk_probe(RiskProbe *probe) {
  deps_.risk_probe = probe;
}

void InvocationService::set_sink(InvocationSink *sink) { deps_.sink = sink; }

void InvocationService::set_waiter(DeferredWaiter *waiter) {
  deps_.waiter = waiter;
}

void InvocationService::set_desensitize(bool enabled) {
  deps_.desensitize = enabled;
}

std::int64_t InvocationService::now_ms() const {
  return deps_.clock == nullptr ? 0 : deps_.clock->now_ms();
}

std::string InvocationService::resolve_tool(const std::string &tool) const {
  if (deps_.catalog == nullptr) {
    return tool;
  }
  const std::string canonical = deps_.catalog->resolve_alias(tool);
  return canonical.empty() ? tool : canonical;
}

std::int64_t InvocationService::record_error(const std::string &request_id) {
  std::int64_t &counter = errors_[request_id];
  ++counter;
  return counter;
}

std::int64_t InvocationService::consume_new_errors(
    const std::string &request_id) {
  std::int64_t total = errors_[""];
  errors_[""] = 0;
  if (!request_id.empty()) {
    const std::map<std::string, std::int64_t>::iterator found =
        errors_.find(request_id);
    if (found != errors_.end()) {
      total += found->second;
      errors_.erase(found);
    }
  }
  return total;
}

void InvocationService::clear_errors() { errors_.clear(); }

InvocationResult InvocationService::reject(const std::string &request_id,
                                           gcp::ErrorCode code,
                                           const std::string &message) {
  InvocationResult result;
  result.status = gcp::Status::error;
  result.error = code;
  result.error_message = message;
  record_error(request_id);
  result.artifact = make_artifact(message, request_id);
  return result;
}

void InvocationService::finalize(InvocationResult &result,
                                 const std::string &request_id) {
  if (result.status == gcp::Status::error || result.error != gcp::ErrorCode::none) {
    record_error(request_id);
  }
  if (deps_.desensitize) {
    result.payload = sanitize_text(result.payload);
    result.error_message = sanitize_text(result.error_message);
  }
  result.artifact =
      make_artifact(result.payload.empty() ? result.error_message
                                           : result.payload,
                    result.task_id.empty() ? request_id : result.task_id);
}

InvocationResult InvocationService::dispatch_normalized(
    const InvocationRequest &request) {
  if (deps_.sink == nullptr) {
    return reject(request.request_id, gcp::ErrorCode::result_unknown,
                  "no dispatch sink");
  }
  InvocationResult result = deps_.sink->dispatch(request);
  if (request.deferred && result.status == gcp::Status::pending &&
      !result.task_id.empty() && deps_.jobs != nullptr) {
    const TaskKey key{request.instance_id, request.generation, result.task_id};
    if (!deps_.jobs->exists(key)) {
      deps_.jobs->create(request.instance_id, request.generation,
                         result.task_id, 0, now_ms());
    }
    if (deps_.waiter != nullptr) {
      InvocationResult waited;
      if (deps_.waiter->wait(result.task_id, request.deadline_ms, waited)) {
        result = waited;
      }
    }
  }
  finalize(result, request.request_id);
  return result;
}

InvocationResult InvocationService::invoke(const InvocationRequest &request) {
  InvocationRequest normalized = request;
  normalized.params = normalize_params(request.params);
  normalized.tool = resolve_tool(request.tool);

  if (deps_.export_probe != nullptr && deps_.export_probe->exporting()) {
    return reject(request.request_id, gcp::ErrorCode::export_in_progress,
                  "export in progress");
  }

  if (deps_.risk_probe != nullptr &&
      deps_.risk_probe->high_risk(normalized.tool) &&
      !param_bool(normalized.params, "confirm", false)) {
    InvocationResult result;
    result.status = gcp::Status::needs_confirmation;
    result.error = gcp::ErrorCode::confirmation_required;
    result.error_message = "confirmation required";
    if (deps_.confirmation != nullptr) {
      result.confirmation = deps_.confirmation->issue(
          request.request_id, normalized.params, now_ms());
    } else {
      result.confirmation =
          gcp::make_challenge(normalized.params, deps_.confirmation_ttl_ms,
                              now_ms(), "nonce-local");
    }
    result.artifact = make_artifact("needs_confirmation", request.request_id);
    return result;
  }

  return dispatch_normalized(normalized);
}

InvocationResult InvocationService::confirm(const std::string &request_id,
                                            const InvocationRequest &original,
                                            const gcp::Params &params) {
  if (deps_.confirmation == nullptr) {
    return reject(request_id, gcp::ErrorCode::confirmation_required,
                  "no confirmation service");
  }
  const gcp::Params normalized = normalize_params(params);
  const ConfirmResult decision =
      deps_.confirmation->accept(request_id, normalized, now_ms());
  if (decision.decision != ConfirmDecision::accepted) {
    switch (decision.decision) {
      case ConfirmDecision::expired:
        return reject(request_id, gcp::ErrorCode::confirmation_required,
                      "confirmation expired");
      case ConfirmDecision::digest_mismatch:
        return reject(request_id, gcp::ErrorCode::confirmation_required,
                      "confirmation parameters changed");
      default:
        return reject(request_id, gcp::ErrorCode::confirmation_required,
                      "no pending confirmation");
    }
  }
  InvocationRequest accepted = original;
  accepted.params = normalized;
  accepted.tool = resolve_tool(original.tool);
  return dispatch_normalized(accepted);
}

InvocationResult InvocationService::cancel(const std::string &instance_id,
                                           std::int64_t generation,
                                           const std::string &task_id) {
  if (deps_.jobs == nullptr) {
    return reject(task_id, gcp::ErrorCode::task_not_found, "no job service");
  }
  const TaskKey key{instance_id, generation, task_id};
  if (!deps_.jobs->request_cancel(key, now_ms())) {
    return reject(task_id, gcp::ErrorCode::task_not_found, "task not found");
  }
  InvocationResult result;
  result.status = gcp::Status::ok;
  result.task_id = task_id;
  result.payload = "cancelling";
  finalize(result, task_id);
  return result;
}

} // namespace control
} // namespace gda
