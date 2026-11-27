#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {

enum class ConfirmDecision {
  accepted,
  declined,
  expired,
  digest_mismatch,
  not_found,
  already_resolved,
};

struct ConfirmResult {
  ConfirmDecision decision = ConfirmDecision::not_found;
  gcp::Challenge challenge;
};

class ConfirmationService {
public:
  explicit ConfirmationService(std::int64_t default_ttl_ms = 60000);

  gcp::Challenge issue(const std::string &request_id, const gcp::Params &params,
                       std::int64_t issued_at_ms,
                       const std::string &nonce = std::string());

  std::size_t pending_count() const;
  bool has_pending(const std::string &request_id) const;
  const gcp::Challenge *find(const std::string &request_id) const;

  ConfirmResult accept(const std::string &request_id,
                       const gcp::Params &params, std::int64_t now_ms);
  ConfirmResult decline(const std::string &request_id);

  std::size_t expire(std::int64_t now_ms);
  void clear();

  void set_default_ttl_ms(std::int64_t ttl_ms);
  std::int64_t default_ttl_ms() const;
  std::string next_nonce();

private:
  std::int64_t default_ttl_ms_ = 60000;
  std::size_t nonce_counter_ = 0;
  std::map<std::string, gcp::Challenge> pending_;
  std::map<std::string, ConfirmDecision> resolved_;
};

} // namespace control
} // namespace gda
