#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <cstdint>

// Function simulating CPU calculation across sampling interval
inline float compute_cpu_utilization(uint64_t proc_delta_100ns, double wall_delta_sec, uint32_t num_cores) {
    if (wall_delta_sec <= 0.0001 || num_cores == 0) return 0.0f;
    double proc_sec = static_cast<double>(proc_delta_100ns) * 1e-7;
    double cpu_pct = (proc_sec / (wall_delta_sec * num_cores)) * 100.0;
    return std::clamp(static_cast<float>(cpu_pct), 0.0f, 100.0f);
}

TEST_CASE("CPU utilization calculations and normalization", "[telemetry][cpu]") {
    uint32_t num_cores = 8;
    double interval_sec = 1.0;

    SECTION("Process consumes exactly 1 full core on an 8-core system") {
        // 1 core * 1 second = 10,000,000 * 100ns ticks
        uint64_t proc_ticks = 10000000ULL;
        float cpu = compute_cpu_utilization(proc_ticks, interval_sec, num_cores);

        // 1 core out of 8 = 12.5%
        REQUIRE(cpu == Catch::Approx(12.5f).margin(0.01f));
    }

    SECTION("Process consumes all 8 cores (100% total system CPU)") {
        uint64_t proc_ticks = 8ULL * 10000000ULL;
        float cpu = compute_cpu_utilization(proc_ticks, interval_sec, num_cores);
        REQUIRE(cpu == Catch::Approx(100.0f).margin(0.01f));
    }

    SECTION("Zero CPU activity") {
        float cpu = compute_cpu_utilization(0, interval_sec, num_cores);
        REQUIRE(cpu == 0.0f);
    }

    SECTION("Short-interval measurement invariance") {
        // Half second interval with 1 core activity
        double short_interval = 0.5;
        uint64_t proc_ticks = 5000000ULL;
        float cpu = compute_cpu_utilization(proc_ticks, short_interval, num_cores);
        REQUIRE(cpu == Catch::Approx(12.5f).margin(0.01f));
    }
}
