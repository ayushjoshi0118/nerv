#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "model/process_graph.hpp"
#include "history/history_store.hpp"
#include "events/event_detector.hpp"
#include "telemetry/telemetry_snapshot.hpp"
#include <sstream>

using namespace nerv;

TEST_CASE("Process Inspector - Selection, Process Disappearance, and Stale Telemetry", "[inspector]") {
    ProcessGraph graph;
    HistoryStore history;

    // Build initial snapshot with 3 processes
    TelemetrySnapshot snap1;
    snap1.is_valid = true;
    snap1.timestamp = std::chrono::system_clock::now();
    snap1.system.is_cpu_available = true;
    snap1.system.total_cpu_percent = 25.0f;
    snap1.system.is_ram_available = true;
    snap1.system.ram_usage_percent = 65.0f;

    ProcessMetrics p1;
    p1.identity = {1001, 10000000ULL};
    p1.name = "editor.exe";
    p1.parent_pid = 4;
    p1.cpu_percent = 14.5f;
    p1.resident_memory_bytes = 420 * 1024 * 1024;
    p1.virtual_memory_bytes = 850 * 1024 * 1024;
    p1.thread_count = 18;
    p1.exe_path = "C:\\Tools\\editor.exe";
    p1.is_accessible = true;
    p1.category = ProcessCategory::UserInteractive;

    ProcessMetrics p2;
    p2.identity = {2002, 20000000ULL};
    p2.name = "service.exe";
    p2.parent_pid = 1;
    p2.cpu_percent = 1.2f;
    p2.resident_memory_bytes = 64 * 1024 * 1024;
    p2.virtual_memory_bytes = 120 * 1024 * 1024;
    p2.thread_count = 6;
    p2.exe_path = ""; // Inaccessible/elevated path
    p2.is_accessible = false;
    p2.category = ProcessCategory::SystemService;

    snap1.processes.push_back(p1);
    snap1.processes.push_back(p2);

    graph.update_from_snapshot(snap1);
    history.record_snapshot(snap1);

    SECTION("Node selection exposes all genuine process fields and metadata") {
        graph.select_node(p1.identity);
        const auto* selected = graph.get_selected_node();

        REQUIRE(selected != nullptr);
        REQUIRE(selected->identity == p1.identity);
        REQUIRE(selected->pid == 1001);
        REQUIRE(selected->parent_pid == 4);
        REQUIRE(selected->name == "editor.exe");
        REQUIRE(selected->metrics.cpu_percent == Catch::Approx(14.5f));
        REQUIRE(selected->metrics.resident_memory_bytes == 420 * 1024 * 1024);
        REQUIRE(selected->metrics.thread_count == 18);
        REQUIRE(selected->metrics.exe_path == "C:\\Tools\\editor.exe");
        REQUIRE(selected->metrics.is_accessible == true);
        REQUIRE(selected->is_terminating == false);
    }

    SECTION("Inaccessible process clearly labels protected metadata") {
        graph.select_node(p2.identity);
        const auto* selected = graph.get_selected_node();

        REQUIRE(selected != nullptr);
        REQUIRE(selected->metrics.is_accessible == false);
        REQUIRE(selected->metrics.exe_path.empty());
        // Path should be labeled protected rather than invented
        REQUIRE(selected->name == "service.exe");
    }

    SECTION("Process disappearing while selected enters preserved termination state") {
        // User selects editor.exe
        graph.select_node(p1.identity);
        REQUIRE(graph.get_selected_node() != nullptr);

        // Snapshot 2: editor.exe terminates (missing from current snapshot)
        TelemetrySnapshot snap2;
        snap2.is_valid = true;
        snap2.timestamp = std::chrono::system_clock::now();
        snap2.processes.push_back(p2); // only p2 remains

        graph.update_from_snapshot(snap2);

        // Node enters is_terminating state
        const auto* selected_during_term = graph.get_selected_node();
        REQUIRE(selected_during_term != nullptr);
        REQUIRE(selected_during_term->is_terminating == true);
        REQUIRE(selected_during_term->name == "editor.exe");
        REQUIRE(selected_during_term->pid == 1001);
        // Final telemetry remains fully intact
        REQUIRE(selected_during_term->metrics.resident_memory_bytes == 420 * 1024 * 1024);

        // Simulation update: termination alpha fades down to 0
        for (int i = 0; i < 20; ++i) {
            graph.update_simulation(0.05f);
        }

        // Selected node MUST NOT be erased while the user is inspecting it!
        const auto* selected_post_mortem = graph.get_selected_node();
        REQUIRE(selected_post_mortem != nullptr);
        REQUIRE(selected_post_mortem->is_terminating == true);
        REQUIRE(selected_post_mortem->alpha == Catch::Approx(0.45f));

        // When the user deselects, the terminated node is reaped
        graph.clear_selection();
        REQUIRE(graph.get_selected_node() == nullptr);
        REQUIRE(graph.get_node_by_identity(p1.identity) == nullptr);
    }

    SECTION("Stale telemetry detection handles aging snapshots gracefully") {
        TelemetrySnapshot fresh_snap;
        fresh_snap.is_valid = true;
        fresh_snap.timestamp = std::chrono::system_clock::now();
        REQUIRE_FALSE(fresh_snap.is_stale(3.0));

        TelemetrySnapshot stale_snap;
        stale_snap.is_valid = true;
        // Snapshot from 5 seconds ago
        stale_snap.timestamp = std::chrono::system_clock::now() - std::chrono::seconds(5);
        REQUIRE(stale_snap.is_stale(3.0));
        REQUIRE(stale_snap.age_seconds() >= 4.9);
    }

    SECTION("Bounded ring buffer history computes accurate statistics (Min, Max, Avg)") {
        HistoryStore test_history;

        // Record 10 successive samples for p1
        for (int i = 1; i <= 10; ++i) {
            TelemetrySnapshot s;
            s.is_valid = true;
            s.timestamp = std::chrono::system_clock::now();
            ProcessMetrics p = p1;
            p.cpu_percent = static_cast<float>(i * 5); // 5%, 10%, ... 50%
            p.resident_memory_bytes = static_cast<uint64_t>(100 + i * 10) * 1024 * 1024;
            s.processes.push_back(p);
            test_history.record_snapshot(s);
        }

        const auto* cpu_hist = test_history.get_process_cpu_history(p1.identity);
        REQUIRE(cpu_hist != nullptr);
        REQUIRE(cpu_hist->size() == 10);

        auto cpu_vals = cpu_hist->to_vector();
        float min_c = cpu_vals[0], max_c = cpu_vals[0], sum_c = 0.0f;
        for (float v : cpu_vals) {
            min_c = std::min(min_c, v);
            max_c = std::max(max_c, v);
            sum_c += v;
        }
        float avg_c = sum_c / cpu_vals.size();

        REQUIRE(min_c == Catch::Approx(5.0f));
        REQUIRE(max_c == Catch::Approx(50.0f));
        REQUIRE(avg_c == Catch::Approx(27.5f));
        REQUIRE(cpu_hist->back() == Catch::Approx(50.0f)); // Current value
    }

    SECTION("Pinning and Unpinning process node maintains position stability") {
        graph.select_node(p1.identity);
        auto* node = graph.get_node_by_identity(p1.identity);
        REQUIRE(node != nullptr);
        REQUIRE_FALSE(node->is_pinned);

        // Pin the node
        node->is_pinned = true;
        node->position = {250.0f, -150.0f};

        // Advance simulation
        graph.update_simulation(0.05f);

        // Position stays pinned
        REQUIRE(node->is_pinned == true);
        REQUIRE(node->position.x == Catch::Approx(250.0f));
        REQUIRE(node->position.y == Catch::Approx(-150.0f));

        // Unpin node
        node->is_pinned = false;
        REQUIRE_FALSE(node->is_pinned);
    }
}
