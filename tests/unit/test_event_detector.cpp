#include <catch2/catch_test_macros.hpp>
#include "events/event_detector.hpp"

using namespace nerv;

TEST_CASE("EventDetector thresholds, cooldowns, and lifecycle", "[events]") {
    EventDetectorConfig cfg;
    cfg.cpu_threshold_percent = 70.0f;
    cfg.memory_threshold_percent = 80.0f;
    cfg.cooldown_seconds = 2.0;
    cfg.enable_spawn_events = true;
    cfg.enable_exit_events = true;

    EventDetector detector(cfg);

    SECTION("Normal metrics do not trigger events") {
        TelemetrySnapshot snap;
        snap.is_valid = true;
        snap.system.is_cpu_available = true;
        snap.system.total_cpu_percent = 35.0f;
        snap.system.is_ram_available = true;
        snap.system.ram_usage_percent = 50.0f;

        auto events = detector.update(snap);
        REQUIRE(events.empty());
    }

    SECTION("CPU spike triggers event and cooldown prevents flooding") {
        TelemetrySnapshot spike;
        spike.is_valid = true;
        spike.system.is_cpu_available = true;
        spike.system.total_cpu_percent = 88.0f; // Crosses 70.0%

        auto events1 = detector.update(spike);
        REQUIRE(events1.size() == 1);
        REQUIRE(events1[0].type == EventType::CpuSpike);
        REQUIRE(events1[0].severity == EventSeverity::Warning);

        // Immediate subsequent update within cooldown period should NOT trigger duplicate event
        auto events2 = detector.update(spike);
        REQUIRE(events2.empty());
    }

    SECTION("Process spawn and exit detection") {
        TelemetrySnapshot initial;
        initial.is_valid = true;
        ProcessMetrics p1;
        p1.identity = {100, 1000ULL};
        p1.name = "agent.exe";
        initial.processes.push_back(p1);

        detector.update(initial); // Baseline registration

        // New snapshot introduces p2
        TelemetrySnapshot spawn_snap = initial;
        ProcessMetrics p2;
        p2.identity = {200, 2000ULL};
        p2.name = "worker.exe";
        spawn_snap.processes.push_back(p2);

        auto events_spawn = detector.update(spawn_snap);
        REQUIRE(events_spawn.size() == 1);
        REQUIRE(events_spawn[0].type == EventType::ProcessSpawned);
        REQUIRE(events_spawn[0].related_process == p2.identity);

        // Third snapshot removes p1
        TelemetrySnapshot exit_snap;
        exit_snap.is_valid = true;
        exit_snap.processes.push_back(p2); // p1 exited

        auto events_exit = detector.update(exit_snap);
        REQUIRE(events_exit.size() == 1);
        REQUIRE(events_exit[0].type == EventType::ProcessTerminated);
        REQUIRE(events_exit[0].related_process == p1.identity);
    }
}
