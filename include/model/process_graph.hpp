#pragma once

#include "telemetry/process_metrics.hpp"
#include "telemetry/telemetry_snapshot.hpp"
#include "rendering/math_types.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace nerv {

enum class GraphLayoutType : uint8_t {
    NeuralGraph,      // Organic force-directed spring network
    HierarchyTree,    // Radial parent-child tree
    ResourceClusters, // Clustered by resource consumption
    TimelineLayout    // Arranged chronologically by start time
};

struct GraphNode {
    ProcessIdentity identity;
    uint32_t pid{0};
    uint32_t parent_pid{0};
    std::string name;

    Vec2 position{0.0f, 0.0f};
    Vec2 velocity{0.0f, 0.0f};
    Vec2 target_position{0.0f, 0.0f};

    float radius{10.0f};
    float target_radius{10.0f};

    float glow_intensity{0.2f};
    float target_glow{0.2f};

    Vec4 base_color{colors::ElectricCyan};
    float alpha{0.0f}; // Fade in from 0.0 on spawn

    bool is_pinned{false};
    bool is_selected{false};
    bool is_hovered{false};
    bool is_filtered_out{false};

    bool is_terminating{false};
    float termination_alpha{1.0f};

    ProcessMetrics metrics;
};

struct GraphEdge {
    ProcessIdentity source_id;
    ProcessIdentity target_id;
    Vec2 source_pos{0.0f, 0.0f};
    Vec2 target_pos{0.0f, 0.0f};
    float pulse_progress{0.0f}; // 0.0 to 1.0 moving packet
    float pulse_speed{1.0f};
    Vec4 color{colors::DeepNavy};
    float alpha{0.4f};
};

struct GraphFilter {
    std::string search_query;
    float min_cpu_percent{0.0f};
    float min_ram_mb{0.0f};
    bool hide_inaccessible{false};
    bool only_interactive{false};

    bool matches(const ProcessMetrics& m) const;
};

class ProcessGraph {
public:
    ProcessGraph();

    void update_from_snapshot(const TelemetrySnapshot& snapshot);
    void update_simulation(float delta_time);

    void set_layout(GraphLayoutType layout);
    GraphLayoutType get_layout() const { return m_current_layout; }

    // Node selection & query
    void select_node(const ProcessIdentity& id);
    void clear_selection();
    const GraphNode* get_selected_node() const;
    GraphNode* get_node_by_identity(const ProcessIdentity& id);
    const GraphNode* find_node_by_pid(uint32_t pid) const;

    // Hit testing (screen coordinates converted to world)
    const GraphNode* hit_test(const Vec2& world_pos) const;

    // Filters
    void set_filter(const GraphFilter& filter);
    const GraphFilter& get_filter() const { return m_filter; }

    const std::unordered_map<ProcessIdentity, GraphNode>& get_nodes() const { return m_nodes; }
    const std::vector<GraphEdge>& get_edges() const { return m_edges; }
    const GraphNode& get_hub_node() const { return m_hub_node; }

    size_t get_active_node_count() const;
    size_t get_edge_count() const { return m_edges.size(); }

    void trigger_layout_refresh();

private:
    void apply_neural_layout_step(float dt);
    void calculate_hierarchy_layout();
    void calculate_cluster_layout();
    void calculate_timeline_layout();
    void rebuild_edges();

    std::unordered_map<ProcessIdentity, GraphNode> m_nodes;
    std::vector<GraphEdge> m_edges;
    GraphNode m_hub_node; // Central System OS Hub

    GraphLayoutType m_current_layout{GraphLayoutType::NeuralGraph};
    GraphFilter m_filter;
    ProcessIdentity m_selected_id{0, 0};

    float m_sim_tick{0.0f};
    bool m_layout_needs_recalc{true};
};

} // namespace nerv
