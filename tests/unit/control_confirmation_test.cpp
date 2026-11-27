#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "core/control/confirmation_service.hpp"
#include "core/control/control_types.hpp"
#include "core/protocol/gcp_envelope.hpp"
#include "support/control_fixture.hpp"

namespace {

using namespace gda::control;
using namespace gda::control::fixture;
namespace gcp = gda::gcp;

TEST(ControlConfirmationTest, IssueCarriesDigestTtlAndNonce) {
  ConfirmationService service(60000);
  const gcp::Params params = fixture::sample_params();
  const gcp::Challenge challenge = service.issue("req-1", params, 1000);
  EXPECT_EQ(challenge.ttl_ms, 60000);
  EXPECT_EQ(challenge.issued_at_ms, 1000);
  EXPECT_EQ(challenge.nonce, "nonce-1");
  EXPECT_EQ(challenge.param_digest,
            gcp::param_digest(normalize_params(params)));
  EXPECT_EQ(service.pending_count(), 1u);
  EXPECT_TRUE(service.has_pending("req-1"));
  ASSERT_NE(service.find("req-1"), nullptr);
  EXPECT_EQ(service.find("req-1")->nonce, "nonce-1");
  EXPECT_EQ(service.issue("req-2", params, 1000, "custom").nonce, "custom");
  EXPECT_EQ(service.next_nonce(), "nonce-2");
}

TEST(ControlConfirmationTest, AcceptRequiresFreshMatchingParams) {
  ConfirmationService service(100);
  const gcp::Params params = fixture::sample_params();
  service.issue("r", params, 1000);
  const gcp::Params reordered{{"a", "1"}, {"b", "2"}};
  EXPECT_EQ(service.accept("r", reordered, 1050).decision,
            ConfirmDecision::accepted);
  EXPECT_EQ(service.pending_count(), 0u);
  EXPECT_EQ(service.accept("r", reordered, 1051).decision,
            ConfirmDecision::already_resolved);

  service.issue("r2", params, 1000);
  EXPECT_EQ(service.accept("r2", gcp::Params{{"a", "9"}}, 1050).decision,
            ConfirmDecision::digest_mismatch);
  EXPECT_TRUE(service.has_pending("r2"));
  EXPECT_EQ(service.accept("r2", reordered, 1100).decision,
            ConfirmDecision::expired);
  EXPECT_FALSE(service.has_pending("r2"));
}

TEST(ControlConfirmationTest, DeclineAndExpirePrunePending) {
  ConfirmationService service(50);
  service.issue("r1", fixture::sample_params(), 1000);
  EXPECT_EQ(service.decline("r1").decision, ConfirmDecision::declined);
  EXPECT_EQ(service.decline("r1").decision,
            ConfirmDecision::already_resolved);
  service.issue("r2", fixture::sample_params(), 1000);
  service.issue("r3", fixture::sample_params(), 2000);
  EXPECT_EQ(service.expire(2000), 1u);
  EXPECT_EQ(service.pending_count(), 1u);
  EXPECT_TRUE(service.has_pending("r3"));
  service.set_default_ttl_ms(10);
  EXPECT_EQ(service.default_ttl_ms(), 10);
  service.clear();
  EXPECT_EQ(service.pending_count(), 0u);
}

} // namespace
