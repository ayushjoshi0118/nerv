#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "events/event_detector.hpp"
#include "rendering/shockwave_system.hpp"
#include <thread>
#include <chrono>

using namespace nerv;

TEST_CASE("EventDetector rising-edge hysteresis and sustained utilization", "[events]") {
    EventDetectorConfig cfg;
    cfg.cpu_threshold_percent = 75.0f;
    cfg.cpu_hysteresis_percent = 5.0f; // Re-arms below 70.0%
    cfg.memory_threshold_percent = 85.0f;
    cfg.memory_hysteresis_percent = 4.0f; // Re-arms below 81.0%
    cfg.disk_spike_mb_sec = 35.0f;
    cfg.disk_hysteresis_mb_sec = 10.0f; // Re-arms below 25.0 MB/s
    cfg.net_spike_mb_sec = 15.0f;
    cfg.net_hysteresis_mb_sec = 4.0f; // Re-arms below 11.0 MB/s
    cfg.process_cpu_spike_percent = 50.0f;
    cfg.process_cpu_hysteresis_percent = 10.0f; // Re-arms below 40.0%
    cfg.cooldown_seconds = 0.05; // Short cooldown for deterministic unit testing
    cfg.process_cooldown_seconds = 0.05;
    cfg.enable_spawn_events = true;
    cfg.enable_exit_events = true;
    cfg.enable_process_cpu_spikes = true;

    EventDetector detector(cfg);

    SECTION("Normal metrics do not trigger events") {
        TelemetrySnapshot snap;
        snap.is_valid = true;
        snap.system.is_cpu_available = true;
        snap.system.total_cpu_percent = 42.0f;
        snap.system.is_ram_available = true;
        snap.system.ram_usage_percent = 60.0f;

        auto events = detector.update(snap);
        REQUIRE(events.empty());
        REQUIRE_FALSE(detector.is_cpu_in_high_state());
        REQUIRE_FALSE(detector.is_memory_in_high_state());
    }

    SECTION("CPU spike fires on rising edge and prevents duplicate events during sustained high load") {
        TelemetrySnapshot spike;
        spike.is_valid = true;
        spike.system.is_cpu_available = true;
        spike.system.total_cpu_percent = 82.5f;

        // 1. First crossing: Rising edge trigger!
        auto events1 = detector.update(spike);
        REQUIRE(events1.size() == 1);
        REQUIRE(events1[0].type == EventType::CpuSpike);
        REQUIRE(events1[0].severity == EventSeverity::Warning);
        REQUIRE(events1[0].measured_value == Catch::Approx(82.5f));
        REQUIRE(events1[0].unit == "%");
        REQUIRE(events1[0].badge_text == "CPU: 82.5%");
        REQUIRE(detector.is_cpu_in_high_state());

        // 2. Sustained high load across multiple ticks (even with cooldown elapsed)
        std::this_thread::sleep_for(std::chrono::milliseconds(60));

        spike.system.total_cpu_percent = 88.0f;
        auto events2 = detector.update(spike);
        REQUIRE(events2.empty()); // MUST NOT RE-TRIGGER during sustained high utilization!

        spike.system.total_cpu_percent = 92.0f;
        auto events3 = detector.update(spike);
        REQUIRE(events3.empty()); // Still in sustained high state

        // 3. Drop into hysteresis buffer (73% is below 75% threshold, but above 70% reset threshold)
        spike.system.total_cpu_percent = 73.0f;
        auto events_hysteresis = detector.update(spike);
        REQUIRE(events_hysteresis.empty());
        REQUIRE(detector.is_cpu_in_high_state()); // Still in high state until dropping below 70%!

        // 4. Drop below reset threshold (68% < 70%) -> Re-arms!
        spike.system.total_cpu_percent = 68.0f;
        detector.update(spike);
        REQUIRE_FALSE(detector.is_cpu_in_high_state()); // Successfully re-armed!

        // 5. Next rising edge fires a new event
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        spike.system.total_cpu_percent = 91.5f; // Critical spike!
        auto events4 = detector.update(spike);
        REQUIRE(events4.size() == 1);
        REQUIRE(events4[0].type == EventType::CpuSpike);
        REQUIRE(events4[0].severity == EventSeverity::Critical);
        REQUIRE(events4[0].measured_value == Catch::Approx(91.5f));
        REQUIRE(events4[0].badge_text == "CPU: 91.5%");
    }

    SECTION("RAM threshold crossing with actual measurement values") {
        TelemetrySnapshot ram_snap;
        ram_snap.is_valid = true;
        ram_snap.system.is_ram_available = true;
        ram_snap.system.ram_usage_percent = 89.2f;
        ram_snap.system.used_ram_bytes = static_cast<uint64_t>(7.1 * 1024 * 1024 * 1024);
        ram_snap.system.total_ram_bytes = static_cast<uint64_t>(8.0 * 1024 * 1024 * 1024);

        auto events = detector.update(ram_snap);
        REQUIRE(events.size() == 1);
        REQUIRE(events[0].type == EventType::MemoryHigh);
        REQUIRE(events[0].measured_value == Catch::Approx(89.2f));
        REQUIRE(events[0].badge_text == "RAM: 89.2%");
        REQUIRE(detector.is_memory_in_high_state());

        // Sustained RAM usage does not duplicate
        auto events_subsequent = detector.update(ram_snap);
        REQUIRE(events_subsequent.empty());

        // Drop below 81% resets
        ram_snap.system.ram_usage_percent = 79.0f;
        detector.update(ram_snap);
        REQUIRE_FALSE(detector.is_memory_in_high_state());
    }

    SECTION("Disk activity changes and throughput surges") {
        TelemetrySnapshot disk_snap;
        disk_snap.is_valid = true;
        // 52 MB/s combined disk throughput
        disk_snap.system.disk_read_bytes_sec = static_cast<uint64_t>(30.0 * 1024 * 1024);
        disk_snap.system.disk_write_bytes_sec = static_cast<uint64_t>(22.0 * 1024 * 1024);

        auto events = detector.update(disk_snap);
        REQUIRE(events.size() == 1);
        REQUIRE(events[0].type == EventType::DiskSpike);
        REQUIRE(events[0].measured_value == Catch::Approx(52.0f).margin(0.1f));
        REQUIRE(events[0].unit == "MB/s");
        REQUIRE(events[0].badge_text == "DISK: 52.0 MB/s");
        REQUIRE(detector.is_disk_in_high_state());

        // Sustained disk activity does not duplicate
        auto events_subsequent = detector.update(disk_snap);
        REQUIRE(events_subsequent.empty());

        // Drop below 25 MB/s resets
        disk_snap.system.disk_read_bytes_sec = static_cast<uint64_t>(10.0 * 1024 * 1024);
        disk_snap.system.disk_write_bytes_sec = static_cast<uint64_t>(5.0 * 1024 * 1024);
        detector.update(disk_snap);
        REQUIRE_FALSE(detector.is_disk_in_high_state());
    }

    SECTION("Network throughput changes and bandwidth bursts") {
        TelemetrySnapshot net_snap;
        net_snap.is_valid = true;
        // 22.5 MB/s combined network throughput
        net_snap.system.network_rx_bytes_sec = static_cast<uint64_t>(15.5 * 1024 * 1024);
        net_snap.system.network_tx_bytes_sec = static_cast<uint64_t>(7.0 * 1024 * 1024);

        auto events = detector.update(net_snap);
        REQUIRE(events.size() == 1);
        REQUIRE(events[0].type == EventType::NetworkSpike);
        REQUIRE(events[0].measured_value == Catch::Approx(22.5f).margin(0.1f));
        REQUIRE(events[0].unit == "MB/s");
        REQUIRE(events[0].badge_text == "NET: 22.5 MB/s");
        REQUIRE(detector.is_net_in_high_state());

        // Drop below 11 MB/s resets
        net_snap.system.network_rx_bytes_sec = static_cast<uint64_t>(4.0 * 1024 * 1024);
        net_snap.system.network_tx_bytes_sec = static_cast<uint64_t>(3.0 * 1024 * 1024);
        detector.update(net_snap);
        REQUIRE_FALSE(detector.is_net_in_high_state());
    }

    SECTION("Per-process CPU spikes with hysteresis") {
        TelemetrySnapshot base;
        base.is_valid = true;
        ProcessMetrics proc;
        proc.identity = {3344, 5000ULL};
        proc.name = "compiler.exe";
        proc.cpu_percent = 64.0f; // Crosses 50.0%
        proc.is_accessible = true;
        base.processes.push_back(proc);

        auto events = detector.update(base);
        REQUIRE(events.size() == 1);
        REQUIRE(events[0].type == EventType::ProcessCpuSpike);
        REQUIRE(events[0].related_process == proc.identity);
        REQUIRE(events[0].measured_value == Catch::Approx(64.0f));
        REQUIRE(events[0].badge_text == "compiler.exe: 64.0%");

        // Sustained per-process high CPU does not duplicate
        auto events_sustained = detector.update(base);
        REQUIRE(events_sustained.empty());
    }

    SECTION("Process lifecycle events include PID measurement") {
        TelemetrySnapshot initial;
        initial.is_valid = true;
        ProcessMetrics p1;
        p1.identity = {100, 1000ULL};
        p1.name = "agent.exe";
        initial.processes.push_back(p1);

        detector.update(initial); // Baseline

        // Spawn p2
        TelemetrySnapshot spawn_snap = initial;
        ProcessMetrics p2;
        p2.identity = {200, 2000ULL};
        p2.name = "worker.exe";
        spawn_snap.processes.push_back(p2);

        auto events_spawn = detector.update(spawn_snap);
        REQUIRE(events_spawn.size() == 1);
        REQUIRE(events_spawn[0].type == EventType::ProcessSpawned);
        REQUIRE(events_spawn[0].related_process == p2.identity);
        REQUIRE(events_spawn[0].measured_value == 200.0f);
        REQUIRE(events_spawn[0].badge_text == "+ worker.exe [200]");

        // Exit p1
        TelemetrySnapshot exit_snap;
        exit_snap.is_valid = true;
        exit_snap.processes.push_back(p2);

        auto events_exit = detector.update(exit_snap);
        REQUIRE(events_exit.size() == 1);
        REQUIRE(events_exit[0].type == EventType::ProcessTerminated);
        REQUIRE(events_exit[0].related_process == p1.identity);
        REQUIRE(events_exit[0].measured_value == 100.0f);
        REQUIRE(events_exit[0].badge_text == "- agent.exe [100]");
    }

    SECTION("Disabling visual animation operates independently of event detection") {
        cfg.enable_visual_effects = false; // User disabled visual shockwaves
        EventDetector headless_detector(cfg);

        TelemetrySnapshot spike;
        spike.is_valid = true;
        spike.system.is_cpu_available = true;
        spike.system.total_cpu_percent = 94.0f;

        auto events = headless_detector.update(spike);
        // Events are still fully detected, timestamped, and stored!
        REQUIRE(events.size() == 1);
        REQUIRE(headless_detector.get_event_log().size() == 1);
        REQUIRE(headless_detector.get_event_log().front().type == EventType::CpuSpike);
        REQUIRE(headless_detector.get_event_log().front().measured_value == Catch::Approx(94.0f));
    }
}

TEST_CASE("ShockwaveSystem carries genuine measurement labels", "[rendering]") {
    ShockwaveSystem sw_system;

    sw_system.trigger(Vec2{100.0f, 200.0f}, colors::CriticalRed, 350.0f, 1.5f,
                      "CPU: 94.2%", "Threshold: 75.0%", EventSeverity::Critical,
                      EventType::CpuSpike, 94.2f, "%", 42);

    REQUIRE(sw_system.get_shockwaves().size() == 1);
    const auto& sw = sw_system.get_shockwaves()[0];
    REQUIRE(sw.badge_text == "CPU: 94.2%");
    REQUIRE(sw.secondary_text == "Threshold: 75.0%");
    REQUIRE(sw.severity == EventSeverity::Critical);
    REQUIRE(sw.measured_value == Catch::Approx(94.2f));
    REQUIRE(sw.unit == "%");
    REQUIRE(sw.event_id == 42);
    REQUIRE(sw.center.x == Catch::Approx(100.0f));
    REQUIRE(sw.center.y == Catch::Approx(200.0f));
    REQUIRE(sw.get_progress() == Catch::Approx(0.0f));

    // Update simulation
    sw_system.update(0.75f);
    REQUIRE(sw_system.get_shockwaves().size() == 1);
    REQUIRE(sw_system.get_shockwaves()[0].get_progress() == Catch::Approx(0.5f));
    REQUIRE(sw_system.get_shockwaves()[0].current_radius > 0.0f);

    // Complete simulation
    sw_system.update(0.8f);
    REQUIRE(sw_system.get_shockwaves().empty());
}
