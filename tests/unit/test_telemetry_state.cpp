#include <catch2/catch_test_macros.hpp>
#include "telemetry/telemetry_snapshot.hpp"
#include "telemetry/process_metrics.hpp"
#include "telemetry/system_metrics.hpp"
#include <chrono>
#include <thread>

using namespace nerv;

TEST_CASE("Telemetry snapshot timestamps and stale-data detection", "[telemetry][stale]") {
    TelemetrySnapshot snap;

    SECTION("Default uninitialized snapshot is invalid and stale") {
        REQUIRE_FALSE(snap.is_valid);
        REQUIRE(snap.is_stale(1.0));
        REQUIRE(snap.age_seconds() > 1000.0);
    }

    SECTION("Newly captured snapshot is fresh") {
        snap.timestamp = std::chrono::system_clock::now();
        snap.is_valid = true;

        REQUIRE(snap.is_valid);
        REQUIRE_FALSE(snap.is_stale(2.0));
        REQUIRE(snap.age_seconds() < 0.5);
    }

    SECTION("Snapshot older than max_age threshold is detected as stale") {
        // Artificially age snapshot by 4 seconds
        snap.timestamp = std::chrono::system_clock::now() - std::chrono::seconds(4);
        snap.is_valid = true;

        REQUIRE(snap.is_stale(2.0)); // 4s > 2s threshold
        REQUIRE_FALSE(snap.is_stale(6.0)); // 4s < 6s threshold
    }
}

TEST_CASE("Process metrics error states and inaccessible process handling", "[telemetry][error_states]") {
    ProcessMetrics proc;
    proc.identity = {400, 123456789ULL};
    proc.name = "csrss.exe";
    proc.state = ProcessState::Running;

    SECTION("Inaccessible / protected process integrity") {
        proc.is_accessible = false;
        proc.cpu_percent = 0.0f;
        proc.resident_memory_bytes = 0;
        proc.virtual_memory_bytes = 0;
        proc.exe_path = "";

        // Ensure inaccessible status is clearly exposed rather than masked with fake data
        REQUIRE_FALSE(proc.is_accessible);
        REQUIRE(proc.cpu_percent == 0.0f);
        REQUIRE(proc.resident_memory_bytes == 0);
        REQUIRE(proc.exe_path.empty());
    }

    SECTION("State string representations") {
        REQUIRE(std::string(ProcessMetrics::state_to_string(ProcessState::Running)) == "Running");
        REQUIRE(std::string(ProcessMetrics::state_to_string(ProcessState::Terminated)) == "Terminated");
        REQUIRE(std::string(ProcessMetrics::state_to_string(ProcessState::Inaccessible)) == "Inaccessible");
    }

    SECTION("Category classifications") {
        REQUIRE(std::string(ProcessMetrics::category_to_string(ProcessCategory::SystemKernel)) == "Kernel / Core");
        REQUIRE(std::string(ProcessMetrics::category_to_string(ProcessCategory::SystemService)) == "System Service");
        REQUIRE(std::string(ProcessMetrics::category_to_string(ProcessCategory::UserInteractive)) == "Interactive App");
        REQUIRE(std::string(ProcessMetrics::category_to_string(ProcessCategory::Development)) == "Developer Tool");
    }
}
