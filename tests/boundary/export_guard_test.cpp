#include "core/export_guard.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/observability/trace_recorder.hpp"

using godot_autopilot::ExportGuard;
using godot_autopilot::ExportState;
using godot_autopilot::TraceRecorder;

namespace {

constexpr int64_t kThirtyOneMinutesNs = 31LL * 60 * 1000000000LL;

template <typename ScopeT, typename = void>
struct HasDisarmInterface : std::false_type {};

template <typename ScopeT>
struct HasDisarmInterface<
    ScopeT, std::void_t<decltype(std::declval<ScopeT &>().release()),
                        decltype(std::declval<ScopeT &>().disarm()),
                        decltype(std::declval<ScopeT &>().reset())>>
    : std::true_type {};

} // namespace

TEST(ExportStateTest, ScopeTogglesExportingFlag) {
  ExportState state;
  EXPECT_FALSE(state.exporting());
  {
    ExportState::Scope scope(state);
    EXPECT_TRUE(state.exporting());
  }
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
}

TEST(ExportStateTest, OverlappingScopesRemainActiveUntilEveryOwnerReleases) {
  ExportState state;
  auto first = std::make_unique<ExportState::Scope>(state);
  auto second = std::make_unique<ExportState::Scope>(state);
  EXPECT_TRUE(state.exporting());

  second.reset();
  EXPECT_TRUE(state.exporting());
  first.reset();
  EXPECT_FALSE(state.exporting());
}

TEST(ExportStateTest, InstancesAreIsolated) {
  ExportState first;
  ExportState second;
  {
    ExportState::Scope scope(first);
    EXPECT_TRUE(first.exporting());
    EXPECT_FALSE(second.exporting());
  }
  EXPECT_FALSE(first.exporting());
  EXPECT_FALSE(second.exporting());
}

TEST(ExportStateTest, HeldMsNeverNegativeAndNeverBeforeStamp) {
  ExportState state;
  EXPECT_EQ(state.held_ms(-1), 0);
  EXPECT_EQ(state.held_ms(0), 0);
  {
    ExportState::Scope scope(state);
    const int64_t now = TraceRecorder::monotonic_now_ns();
    EXPECT_EQ(state.held_ms(0), 0);
    EXPECT_GE(state.held_ms(now), 0);
  }
  EXPECT_EQ(state.held_ms(TraceRecorder::monotonic_now_ns()), 0);
}

TEST(ExportGuardScanTest, ScanDoesNotResetLongRunningExport) {
  ExportState state;
  const int64_t before = TraceRecorder::monotonic_now_ns();
  {
    ExportState::Scope scope(state);
    const int64_t long_now = before + kThirtyOneMinutesNs;
    const std::vector<ExportState *> states = {nullptr, &state};

    const ExportGuard::ScanResult result =
        ExportGuard::scan_states(states, long_now);
    EXPECT_TRUE(result.exporting);
    EXPECT_EQ(result.active_states, 1u);
    EXPECT_GE(result.max_held_ms, 30LL * 60 * 1000 + 1000);
    EXPECT_TRUE(state.exporting());

    const ExportGuard::ScanResult again =
        ExportGuard::scan_states(states, long_now + kThirtyOneMinutesNs);
    EXPECT_TRUE(again.exporting);
    EXPECT_EQ(again.active_states, 1u);
    EXPECT_TRUE(state.exporting());
  }
  EXPECT_FALSE(state.exporting());
}

TEST(ExportGuardScanTest, ScanReportsEveryActiveInstanceWithoutMutation) {
  ExportState idle;
  ExportState first;
  ExportState second;
  const int64_t before = TraceRecorder::monotonic_now_ns();
  ExportState::Scope first_scope(first);
  ExportState::Scope second_scope(second);

  const ExportGuard::ScanResult result = ExportGuard::scan_states(
      {&idle, nullptr, &first, &second}, before + kThirtyOneMinutesNs);
  EXPECT_TRUE(result.exporting);
  EXPECT_EQ(result.active_states, 2u);
  EXPECT_GE(result.max_held_ms, 30LL * 60 * 1000 + 1000);
  EXPECT_TRUE(first.exporting());
  EXPECT_TRUE(second.exporting());
}

TEST(ExportStateRegistryTest, NestedScopesRemainActiveUntilEveryEnd) {
  ExportState state;
  EXPECT_FALSE(ExportGuard::is_exporting());
  auto first = std::make_unique<ExportState::Scope>(state);
  EXPECT_TRUE(ExportGuard::is_exporting());
  auto second = std::make_unique<ExportState::Scope>(state);
  EXPECT_TRUE(ExportGuard::is_exporting());
  second.reset();
  EXPECT_TRUE(ExportGuard::is_exporting());
  first.reset();
  EXPECT_FALSE(ExportGuard::is_exporting());
}
