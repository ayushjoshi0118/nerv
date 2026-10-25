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

    SECTION("Multi-threaded process consuming 4 cores on an 8-core system") {
        uint64_t proc_ticks = 4ULL * 10000000ULL;
        float cpu = compute_cpu_utilization(proc_ticks, interval_sec, num_cores);
        REQUIRE(cpu == Catch::Approx(50.0f).margin(0.01f));
    }

    SECTION("Zero CPU activity") {
        float cpu = compute_cpu_utilization(0, interval_sec, num_cores);
        REQUIRE(cpu == 0.0f);
    }

    SECTION("Measurement invariance across varying intervals (250ms, 500ms, 2000ms)") {
        // Equivalent to 2 full cores sustained workload
        // Over 0.25s: 2 cores * 0.25s = 0.5s = 5,000,000 ticks
        REQUIRE(compute_cpu_utilization(5000000ULL, 0.25, num_cores) == Catch::Approx(25.0f).margin(0.01f));

        // Over 0.5s: 2 cores * 0.5s = 1.0s = 10,000,000 ticks
        REQUIRE(compute_cpu_utilization(10000000ULL, 0.50, num_cores) == Catch::Approx(25.0f).margin(0.01f));

        // Over 2.0s: 2 cores * 2.0s = 4.0s = 40,000,000 ticks
        REQUIRE(compute_cpu_utilization(40000000ULL, 2.00, num_cores) == Catch::Approx(25.0f).margin(0.01f));
    }

    SECTION("Division by zero and invalid input protection") {
        // Zero wall delta time
        REQUIRE(compute_cpu_utilization(1000000ULL, 0.0, num_cores) == 0.0f);
        // Negative wall delta time
        REQUIRE(compute_cpu_utilization(1000000ULL, -1.0, num_cores) == 0.0f);
        // Zero cores
        REQUIRE(compute_cpu_utilization(1000000ULL, 1.0, 0) == 0.0f);
    }

    SECTION("Various core topologies (1, 16, 64 cores)") {
        // 1 core system: 1 core saturated = 100%
        REQUIRE(compute_cpu_utilization(10000000ULL, 1.0, 1) == Catch::Approx(100.0f).margin(0.01f));

        // 16 core system: 1 core saturated = 6.25%
        REQUIRE(compute_cpu_utilization(10000000ULL, 1.0, 16) == Catch::Approx(6.25f).margin(0.01f));

        // 64 core system: 1 core saturated = 1.5625%
        REQUIRE(compute_cpu_utilization(10000000ULL, 1.0, 64) == Catch::Approx(1.5625f).margin(0.01f));
    }

    SECTION("Out-of-bounds safety clamping") {
        // Ticks claiming more CPU than physically possible (e.g. clock drift) clamp to 100.0f
        uint64_t extreme_ticks = 100ULL * 10000000ULL;
        REQUIRE(compute_cpu_utilization(extreme_ticks, 1.0, num_cores) == 100.0f);
    }
}
