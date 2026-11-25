#include "core/control/confirmation_service.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include "core/control/control_types.hpp"
#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {

ConfirmationService::ConfirmationService(std::int64_t default_ttl_ms)
    : default_ttl_ms_(default_ttl_ms) {}

std::string ConfirmationService::next_nonce() {
  ++nonce_counter_;
  return "nonce-" + std::to_string(nonce_counter_);
}

gcp::Challenge ConfirmationService::issue(const std::string &request_id,
                                          const gcp::Params &params,
                                          std::int64_t issued_at_ms,
                                          const std::string &nonce) {
  const gcp::Params normalized = normalize_params(params);
  const std::string resolved_nonce = nonce.empty() ? next_nonce() : nonce;
  const gcp::Challenge challenge =
      gcp::make_challenge(normalized, default_ttl_ms_, issued_at_ms,
                          resolved_nonce);
  pending_[request_id] = challenge;
  resolved_.erase(request_id);
  return challenge;
}

std::size_t ConfirmationService::pending_count() const {
  return pending_.size();
}

bool ConfirmationService::has_pending(const std::string &request_id) const {
  return pending_.find(request_id) != pending_.end();
}

const gcp::Challenge *ConfirmationService::find(
    const std::string &request_id) const {
  const std::map<std::string, gcp::Challenge>::const_iterator found =
      pending_.find(request_id);
  return found == pending_.end() ? nullptr : &found->second;
}

ConfirmResult ConfirmationService::accept(const std::string &request_id,
                                          const gcp::Params &params,
                                          std::int64_t now_ms) {
  ConfirmResult result;
  const std::map<std::string, gcp::Challenge>::iterator found =
      pending_.find(request_id);
  if (found == pending_.end()) {
    if (resolved_.find(request_id) != resolved_.end()) {
      result.decision = ConfirmDecision::already_resolved;
    }
    return result;
  }
  result.challenge = found->second;
  if (!gcp::challenge_fresh(found->second, now_ms)) {
    pending_.erase(found);
    resolved_[request_id] = ConfirmDecision::expired;
    result.decision = ConfirmDecision::expired;
    return result;
  }
  if (!gcp::challenge_matches(found->second, normalize_params(params))) {
    result.decision = ConfirmDecision::digest_mismatch;
    return result;
  }
  pending_.erase(found);
  resolved_[request_id] = ConfirmDecision::accepted;
  result.decision = ConfirmDecision::accepted;
  return result;
}

ConfirmResult ConfirmationService::decline(const std::string &request_id) {
  ConfirmResult result;
  const std::map<std::string, gcp::Challenge>::iterator found =
      pending_.find(request_id);
  if (found == pending_.end()) {
    if (resolved_.find(request_id) != resolved_.end()) {
      result.decision = ConfirmDecision::already_resolved;
    }
    return result;
  }
  result.challenge = found->second;
  pending_.erase(found);
  resolved_[request_id] = ConfirmDecision::declined;
  result.decision = ConfirmDecision::declined;
  return result;
}

std::size_t ConfirmationService::expire(std::int64_t now_ms) {
  std::size_t removed = 0;
  for (std::map<std::string, gcp::Challenge>::iterator it = pending_.begin();
       it != pending_.end();) {
    if (!gcp::challenge_fresh(it->second, now_ms)) {
      resolved_[it->first] = ConfirmDecision::expired;
      it = pending_.erase(it);
      ++removed;
      continue;
    }
    ++it;
  }
  return removed;
}

void ConfirmationService::clear() {
  pending_.clear();
  resolved_.clear();
}

void ConfirmationService::set_default_ttl_ms(std::int64_t ttl_ms) {
  default_ttl_ms_ = ttl_ms;
}

std::int64_t ConfirmationService::default_ttl_ms() const {
  return default_ttl_ms_;
}

} // namespace control
} // namespace gda
