#include <catch2/catch_test_macros.hpp>
#include "telemetry/mock_system_monitor.hpp"
#include "telemetry/telemetry_collector.hpp"

#ifdef _WIN32
#include "platform/windows/windows_system_monitor.hpp"
#endif

using namespace nerv;

TEST_CASE("MockSystemMonitor telemetry generation", "[integration][mock]") {
    MockSystemMonitor mock(25);
    REQUIRE(mock.initialize());

    auto sys = mock.sample_system_metrics();
    REQUIRE(sys.is_cpu_available);
    REQUIRE(sys.is_ram_available);
    REQUIRE(sys.total_cpu_percent >= 0.0f);
    REQUIRE(sys.total_cpu_percent <= 100.0f);
    REQUIRE(sys.total_ram_bytes > 0);
    REQUIRE(sys.used_ram_bytes > 0);
    REQUIRE(sys.available_ram_bytes > 0);

    auto procs = mock.sample_process_metrics();
    REQUIRE(procs.size() >= 25);
    REQUIRE(procs[0].identity.pid == 4); // Kernel/System process
    REQUIRE(procs[0].name == "System");

    mock.shutdown();
}

#ifdef _WIN32
TEST_CASE("WindowsSystemMonitor live host telemetry", "[integration][windows]") {
    WindowsSystemMonitor win_mon;
    REQUIRE(win_mon.initialize());

    auto sys = win_mon.sample_system_metrics();
    REQUIRE(sys.is_cpu_available);
    REQUIRE(sys.is_ram_available);
    REQUIRE(sys.total_cpu_percent >= 0.0f);
    REQUIRE(sys.total_cpu_percent <= 100.0f);
    REQUIRE(sys.total_ram_bytes > 1024ULL * 1024ULL * 1024ULL); // Host has > 1GB RAM
    REQUIRE(sys.uptime_seconds > 0.0);

    auto procs = win_mon.sample_process_metrics();
    REQUIRE_FALSE(procs.empty()); // Real host has active processes running

    // Verify at least one real process has valid identity and working set
    bool found_valid_proc = false;
    for (const auto& p : procs) {
        if (p.is_accessible && p.resident_memory_bytes > 0) {
            found_valid_proc = true;
            break;
        }
    }
    REQUIRE(found_valid_proc);

    win_mon.shutdown();
}
#endif

TEST_CASE("TelemetryCollector worker thread and clean shutdown", "[integration][collector]") {
    auto monitor = std::make_unique<MockSystemMonitor>(20);
    TelemetryCollector collector(std::move(monitor));

    collector.set_sampling_interval_ms(200); // 200ms sampling for fast test
    collector.start();

    // Sleep briefly to allow worker to take at least one snapshot
    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    auto snap = collector.get_latest_snapshot();
    REQUIRE(snap != nullptr);
    REQUIRE(snap->is_valid);
    REQUIRE(snap->sequence_number >= 1);
    REQUIRE(snap->processes.size() >= 20);

    // Verify clean, deterministic shutdown
    collector.stop();
    REQUIRE_FALSE(snap->is_stale(100.0)); // Clean shutdown doesn't hang
}
