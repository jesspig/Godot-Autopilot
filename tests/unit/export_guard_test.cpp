#include "core/export_guard.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/trace_recorder.hpp"

using godot_autopilot::ExportGuard;
using godot_autopilot::ExportState;
using godot_autopilot::TraceRecorder;

namespace {

template <typename ScopeT, typename = void>
struct HasDisarmInterface : std::false_type {};

template <typename ScopeT>
struct HasDisarmInterface<
    ScopeT, std::void_t<decltype(std::declval<ScopeT &>().release()),
                        decltype(std::declval<ScopeT &>().disarm()),
                        decltype(std::declval<ScopeT &>().reset())>>
    : std::true_type {};

} // namespace

TEST(ExportStateTest, BeginAndEndToggleExportingFlag) {
  ExportState state;
  EXPECT_FALSE(state.exporting());
  state.begin();
  EXPECT_TRUE(state.exporting());
  state.end();
  EXPECT_FALSE(state.exporting());
}

TEST(ExportStateTest, ScopeResetsFlagWhenLeavingAbnormally) {
  ExportState state;
  try {
    ExportState::Scope scope(state);
    EXPECT_TRUE(state.exporting());
    throw std::runtime_error("export begin failed");
  } catch (const std::exception &) {
  }
  EXPECT_FALSE(state.exporting());
}

TEST(ExportStateTest, ScopeDestructionAlwaysClearsExportingFlag) {
  static_assert(!HasDisarmInterface<ExportState::Scope>::value,
                "Scope must not expose any interface that disarms its RAII "
                "reset, otherwise the export flag can outlive its owner again");
  ExportState state;
  {
    ExportState::Scope scope(state);
    EXPECT_TRUE(state.exporting());
  }
  EXPECT_FALSE(state.exporting());
  EXPECT_FALSE(state.claim_stale_reset(TraceRecorder::monotonic_now_ns()));
}

TEST(ExportStateTest, InstancesAreIsolated) {
  ExportState first;
  ExportState second;
  first.begin();
  EXPECT_TRUE(first.exporting());
  EXPECT_FALSE(second.exporting());
  first.end();
  EXPECT_FALSE(first.exporting());
  EXPECT_FALSE(second.exporting());
}

TEST(ExportStateTest, ClaimStaleResetRejectsWithinBudget) {
  ExportState state;
  const int64_t before = TraceRecorder::monotonic_now_ns();
  state.begin();
  const int64_t after = TraceRecorder::monotonic_now_ns();

  EXPECT_FALSE(state.claim_stale_reset(after));
  EXPECT_TRUE(state.exporting());
  EXPECT_FALSE(state.claim_stale_reset(before + ExportState::kHoldBudgetNs));
  EXPECT_TRUE(state.exporting());
}

TEST(ExportStateTest, ClaimStaleResetSucceedsExactlyOnceBeyondBudget) {
  ExportState state;
  state.begin();
  const int64_t after = TraceRecorder::monotonic_now_ns();
  const int64_t stale_now = after + ExportState::kHoldBudgetNs + 1;

  EXPECT_TRUE(state.claim_stale_reset(stale_now));
  EXPECT_FALSE(state.exporting());
  EXPECT_FALSE(state.claim_stale_reset(stale_now + 1));
  EXPECT_FALSE(state.exporting());
}

TEST(ExportStateTest, HeldMsNeverNegativeAndNeverBeforeStamp) {
  ExportState state;
  EXPECT_EQ(0, state.held_ms(-1));
  EXPECT_EQ(0, state.held_ms(0));
  EXPECT_EQ(ExportState::kHoldBudgetMs,
            state.held_ms(ExportState::kHoldBudgetNs));
  EXPECT_EQ(ExportState::kHoldBudgetMs - 1,
            state.held_ms(ExportState::kHoldBudgetNs - 1000000LL));

  const int64_t before = TraceRecorder::monotonic_now_ns();
  state.begin();
  const int64_t after = TraceRecorder::monotonic_now_ns();
  EXPECT_EQ(0, state.held_ms(before));
  EXPECT_GE(state.held_ms(after), state.held_ms(before));
}

TEST(ExportGuardScanTest, ScanResetsEveryStaleInstanceExactlyOnce) {
  ExportState idle;
  ExportState first;
  first.begin();
  ExportState second;
  second.begin();
  const int64_t after = TraceRecorder::monotonic_now_ns();
  const int64_t stale_now = after + ExportState::kHoldBudgetNs + 1;
  const std::vector<ExportState *> states = {&idle, nullptr, &first, &second};

  const ExportGuard::ScanResult result =
      ExportGuard::scan_states(states, stale_now);
  EXPECT_EQ(2, result.stale_resets);
  EXPECT_FALSE(result.exporting);
  EXPECT_GE(result.max_held_ms, ExportState::kHoldBudgetMs);
  EXPECT_FALSE(idle.exporting());
  EXPECT_FALSE(first.exporting());
  EXPECT_FALSE(second.exporting());

  const ExportGuard::ScanResult again =
      ExportGuard::scan_states(states, stale_now);
  EXPECT_EQ(0, again.stale_resets);
  EXPECT_FALSE(again.exporting);
  EXPECT_EQ(0, again.max_held_ms);
}

TEST(ExportGuardScanTest, ScanResetsStaleInstanceAndKeepsFreshOneExporting) {
  ExportState stale;
  stale.begin();
  const int64_t origin = TraceRecorder::monotonic_now_ns();
  int64_t boundary = TraceRecorder::monotonic_now_ns();
  while (boundary == origin) {
    boundary = TraceRecorder::monotonic_now_ns();
  }
  ExportState fresh;
  fresh.begin();

  const std::vector<ExportState *> states = {&stale, &fresh};
  const ExportGuard::ScanResult result = ExportGuard::scan_states(
      states, boundary + ExportState::kHoldBudgetNs);
  EXPECT_EQ(1, result.stale_resets);
  EXPECT_TRUE(result.exporting);
  EXPECT_GE(result.max_held_ms, ExportState::kHoldBudgetMs);
  EXPECT_FALSE(stale.exporting());
  EXPECT_TRUE(fresh.exporting());
}
