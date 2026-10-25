#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "model/process_graph.hpp"
#include <vector>

using namespace nerv;

TEST_CASE("ProcessGraph parent-child verification and relationship types", "[graph][relationships]") {
    ProcessGraph graph;

    TelemetrySnapshot snap;
    snap.is_valid = true;

    // Parent process (born at t=1000)
    ProcessMetrics parent;
    parent.identity = {100, 1000ULL};
    parent.parent_pid = 0;
    parent.name = "parent.exe";
    parent.is_accessible = true;
    snap.processes.push_back(parent);

    // Child process (born at t=2000, parent is PID 100)
    ProcessMetrics child_valid;
    child_valid.identity = {200, 2000ULL};
    child_valid.parent_pid = 100;
    child_valid.name = "child_valid.exe";
    child_valid.is_accessible = true;
    snap.processes.push_back(child_valid);

    // Orphan process (parent PID 999 does not exist)
    ProcessMetrics orphan;
    orphan.identity = {300, 2500ULL};
    orphan.parent_pid = 999;
    orphan.name = "orphan.exe";
    orphan.is_accessible = true;
    snap.processes.push_back(orphan);

    graph.update_from_snapshot(snap);

    SECTION("Verified parent-child vs Inferred parent exited") {
        REQUIRE(graph.get_active_node_count() == 3);
        REQUIRE(graph.get_verified_edge_count() >= 1);
        REQUIRE(graph.get_inferred_edge_count() >= 1);

        // Find child_valid edge
        bool found_verified = false;
        for (const auto& e : graph.get_edges()) {
            if (e.target_id == child_valid.identity) {
                REQUIRE(e.is_verified());
                REQUIRE(e.relationship_type == RelationshipType::VerifiedParentChild);
                REQUIRE(std::string(e.get_relationship_label()) == "Verified Parent-Child");
                found_verified = true;
            }
        }
        REQUIRE(found_verified);

        // Find orphan edge
        bool found_orphan_inferred = false;
        for (const auto& e : graph.get_edges()) {
            if (e.target_id == orphan.identity) {
                REQUIRE_FALSE(e.is_verified());
                REQUIRE(e.relationship_type == RelationshipType::InferredParentExited);
                REQUIRE(std::string(e.get_relationship_label()) == "Inferred (Parent Exited)");
                found_orphan_inferred = true;
            }
        }
        REQUIRE(found_orphan_inferred);
    }

    SECTION("PID reuse edge case: parent born AFTER child is flagged as Inferred") {
        TelemetrySnapshot reuse_snap;
        reuse_snap.is_valid = true;

        // Old child born at t=1000 claiming parent PID 500
        ProcessMetrics child;
        child.identity = {700, 1000ULL};
        child.parent_pid = 500;
        child.name = "old_child.exe";
        child.is_accessible = true;
        reuse_snap.processes.push_back(child);

        // New process using reused PID 500 but born AFTER the child at t=5000!
        ProcessMetrics reused_parent;
        reused_parent.identity = {500, 5000ULL};
        reused_parent.parent_pid = 0;
        reused_parent.name = "new_process_reused_pid.exe";
        reused_parent.is_accessible = true;
        reuse_snap.processes.push_back(reused_parent);

        ProcessGraph reuse_graph;
        reuse_graph.update_from_snapshot(reuse_snap);

        for (const auto& e : reuse_graph.get_edges()) {
            if (e.target_id == child.identity) {
                // Must be Inferred, NOT Verified!
                REQUIRE_FALSE(e.is_verified());
                REQUIRE(e.relationship_type == RelationshipType::InferredParentExited);
            }
        }
    }
}

TEST_CASE("ProcessGraph position stability across telemetry updates", "[graph][stability]") {
    ProcessGraph graph;
    graph.set_layout(GraphLayoutType::NeuralGraph);

    TelemetrySnapshot snap1;
    snap1.is_valid = true;
    for (uint32_t i = 1; i <= 25; ++i) {
        ProcessMetrics p;
        p.identity = {1000 + i, 1000ULL * i};
        p.parent_pid = (i > 5) ? (1000 + (i % 5)) : 0;
        p.name = "proc_" + std::to_string(i);
        p.cpu_percent = 5.0f;
        p.resident_memory_bytes = 50 * 1024 * 1024;
        snap1.processes.push_back(p);
    }

    graph.update_from_snapshot(snap1);

    // Run physics simulation to settle
    for (int step = 0; step < 60; ++step) {
        graph.update_simulation(0.016f);
    }

    // Record settled positions
    std::unordered_map<ProcessIdentity, Vec2> settled_positions;
    for (const auto& [id, node] : graph.get_nodes()) {
        settled_positions[id] = node.position;
    }

    // Send new snapshot with updated CPU metrics (no additions/removals)
    TelemetrySnapshot snap2 = snap1;
    for (auto& p : snap2.processes) {
        p.cpu_percent += 10.0f; // Metric updated
    }

    graph.update_from_snapshot(snap2);

    // Verify positions have NOT scrambled or jumped
    for (const auto& [id, node] : graph.get_nodes()) {
        auto it = settled_positions.find(id);
        REQUIRE(it != settled_positions.end());
        float drift = node.position.distance(it->second);
        REQUIRE(drift < 0.05f); // Sub-pixel stability!
    }
}

TEST_CASE("ProcessGraph large synthetic dataset and Level of Detail", "[graph][stress][lod]") {
    ProcessGraph graph;

    // Simulate 1,000 synthetic processes
    TelemetrySnapshot large_snap;
    large_snap.is_valid = true;
    for (uint32_t i = 1; i <= 1000; ++i) {
        ProcessMetrics p;
        p.identity = {10000 + i, 500ULL * i};
        p.parent_pid = (i > 10) ? (10000 + (i % 10)) : 0;
        p.name = "stress_worker_" + std::to_string(i);
        p.cpu_percent = (i % 20 == 0) ? 25.0f : 0.5f;
        p.resident_memory_bytes = (10ULL + (i % 300)) * 1024 * 1024;
        large_snap.processes.push_back(p);
    }

    graph.update_from_snapshot(large_snap);

    REQUIRE(graph.get_active_node_count() == 1000);
    REQUIRE(graph.get_edge_count() == 1000);

    SECTION("Level of Detail adaptation under high node count") {
        // High node count (> 600) automatically activates LowDetail
        REQUIRE(graph.compute_lod(0.3f) == GraphLOD::LowDetail);
        REQUIRE(graph.compute_lod(0.8f) == GraphLOD::LowDetail);
    }

    SECTION("Physics simulation step with 1,000 nodes remains bounded and stable") {
        graph.update_simulation(0.016f);

        // Verify no NaN or Inf in positions or velocities
        for (const auto& [id, node] : graph.get_nodes()) {
            REQUIRE_FALSE(std::isnan(node.position.x));
            REQUIRE_FALSE(std::isnan(node.position.y));
            REQUIRE_FALSE(std::isnan(node.velocity.x));
            REQUIRE_FALSE(std::isnan(node.velocity.y));
            REQUIRE(node.velocity.length() <= 181.0f); // Velocity clamped
        }
    }

    SECTION("Search and filtering over 1,000 nodes") {
        GraphFilter filter;
        filter.search_query = "stress_worker_42"; // Matches worker 42, 420-429
        graph.set_filter(filter);

        size_t matches = graph.get_active_node_count();
        REQUIRE(matches >= 1);
        REQUIRE(matches < 15);
    }
}
