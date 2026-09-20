#include "core/perf_sampler.hpp"

#include <gtest/gtest.h>

using godot_autopilot::perf_sampler::FrameAccumulator;
using godot_autopilot::perf_sampler::RateWindow;
using godot_autopilot::perf_sampler::Rates;

TEST(PerfSamplerTest, AccumulateFrameClampsNegativeDeltaAndTracksMax) {
    FrameAccumulator acc;
    godot_autopilot::perf_sampler::accumulate_frame(acc, -5.0);
    EXPECT_EQ(acc.frames, 1u);
    EXPECT_DOUBLE_EQ(acc.total_ms, 0.0);
    EXPECT_DOUBLE_EQ(acc.max_ms, 0.0);

    godot_autopilot::perf_sampler::accumulate_frame(acc, 10.5);
    EXPECT_EQ(acc.frames, 2u);
    EXPECT_DOUBLE_EQ(acc.total_ms, 10.5);
    EXPECT_DOUBLE_EQ(acc.max_ms, 10.5);

    godot_autopilot::perf_sampler::accumulate_frame(acc, 4.0);
    EXPECT_EQ(acc.frames, 3u);
    EXPECT_DOUBLE_EQ(acc.total_ms, 14.5);
    EXPECT_DOUBLE_EQ(acc.max_ms, 10.5);
}

TEST(PerfSamplerTest, AverageFrameMsReturnsZeroWhenEmpty) {
    FrameAccumulator empty;
    EXPECT_DOUBLE_EQ(godot_autopilot::perf_sampler::average_frame_ms(empty), 0.0);

    FrameAccumulator acc;
    godot_autopilot::perf_sampler::accumulate_frame(acc, 10.5);
    godot_autopilot::perf_sampler::accumulate_frame(acc, 4.0);
    godot_autopilot::perf_sampler::accumulate_frame(acc, -1.0);
    EXPECT_DOUBLE_EQ(godot_autopilot::perf_sampler::average_frame_ms(acc), 14.5 / 3.0);
}

TEST(PerfSamplerTest, ComputeRatesReturnsZerosForNonPositiveSeconds) {
    RateWindow window;
    window.requests = 10;
    window.responses = 8;
    window.errors = 2;
    window.latency_ms_sum = 40;
    window.latency_samples = 4;

    const Rates zero = godot_autopilot::perf_sampler::compute_rates(window, 0.0);
    EXPECT_DOUBLE_EQ(zero.requests_per_second, 0.0);
    EXPECT_DOUBLE_EQ(zero.errors_per_second, 0.0);
    EXPECT_DOUBLE_EQ(zero.error_rate, 0.0);
    EXPECT_DOUBLE_EQ(zero.average_latency_ms, 0.0);

    const Rates negative =
        godot_autopilot::perf_sampler::compute_rates(window, -2.0);
    EXPECT_DOUBLE_EQ(negative.requests_per_second, 0.0);
    EXPECT_DOUBLE_EQ(negative.errors_per_second, 0.0);
    EXPECT_DOUBLE_EQ(negative.error_rate, 0.0);
    EXPECT_DOUBLE_EQ(negative.average_latency_ms, 0.0);
}

TEST(PerfSamplerTest, ComputeRatesMatchesHandCalculatedValues) {
    RateWindow window;
    window.requests = 10;
    window.responses = 8;
    window.errors = 2;
    window.inflight = 3;
    window.latency_ms_sum = 40;
    window.latency_samples = 4;

    const Rates rates = godot_autopilot::perf_sampler::compute_rates(window, 2.0);
    EXPECT_DOUBLE_EQ(rates.requests_per_second, 5.0);
    EXPECT_DOUBLE_EQ(rates.errors_per_second, 1.0);
    EXPECT_DOUBLE_EQ(rates.error_rate, 0.25);
    EXPECT_DOUBLE_EQ(rates.average_latency_ms, 10.0);
}

TEST(PerfSamplerTest, ComputeRatesLeavesRatiosZeroWithoutSamples) {
    RateWindow window;
    window.requests = 4;
    window.responses = 0;
    window.errors = 0;
    window.latency_samples = 0;

    const Rates rates = godot_autopilot::perf_sampler::compute_rates(window, 2.0);
    EXPECT_DOUBLE_EQ(rates.requests_per_second, 2.0);
    EXPECT_DOUBLE_EQ(rates.errors_per_second, 0.0);
    EXPECT_DOUBLE_EQ(rates.error_rate, 0.0);
    EXPECT_DOUBLE_EQ(rates.average_latency_ms, 0.0);
}

TEST(PerfSamplerTest, MillisecondsToNanosecondsScales) {
    EXPECT_EQ(godot_autopilot::perf_sampler::milliseconds_to_nanoseconds(0), 0);
    EXPECT_EQ(godot_autopilot::perf_sampler::milliseconds_to_nanoseconds(1), 1000000);
    EXPECT_EQ(godot_autopilot::perf_sampler::milliseconds_to_nanoseconds(1000),
              1000000000);
    EXPECT_EQ(godot_autopilot::perf_sampler::milliseconds_to_nanoseconds(-1), -1000000);
}
