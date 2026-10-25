#include "model/process_graph.hpp"
#include <cmath>
#include <algorithm>

namespace nerv {

bool GraphFilter::matches(const ProcessMetrics& m) const {
    if (!search_query.empty()) {
        std::string query_lower = search_query;
        std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        std::string name_lower = m.name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        std::string pid_str = std::to_string(m.identity.pid);

        if (name_lower.find(query_lower) == std::string::npos &&
            pid_str.find(query_lower) == std::string::npos) {
            return false;
        }
    }

    if (min_cpu_percent > 0.0f && m.cpu_percent < min_cpu_percent) {
        return false;
    }

    float ram_mb = static_cast<float>(m.resident_memory_bytes) / (1024.0f * 1024.0f);
    if (min_ram_mb > 0.0f && ram_mb < min_ram_mb) {
        return false;
    }

    if (hide_inaccessible && !m.is_accessible) {
        return false;
    }

    if (only_interactive && m.category != ProcessCategory::UserInteractive && m.category != ProcessCategory::Development) {
        return false;
    }

    return true;
}

ProcessGraph::ProcessGraph() {
    // Initialize central System Hub
    m_hub_node.identity = {0, 0};
    m_hub_node.pid = 0;
    m_hub_node.name = "NERV SYSTEM HUB";
    m_hub_node.position = {0.0f, 0.0f};
    m_hub_node.target_position = {0.0f, 0.0f};
    m_hub_node.radius = 34.0f;
    m_hub_node.target_radius = 34.0f;
    m_hub_node.glow_intensity = 0.85f;
    m_hub_node.base_color = colors::ElectricCyan;
    m_hub_node.alpha = 1.0f;
    m_hub_node.is_pinned = true;
}

GraphLOD ProcessGraph::compute_lod(float zoom) const {
    size_t count = get_active_node_count();
    if (zoom < 0.45f || count > 600) {
        return GraphLOD::LowDetail;
    }
    if (zoom <= 1.05f) {
        return GraphLOD::MediumDetail;
    }
    return GraphLOD::FullDetail;
}

void ProcessGraph::update_from_snapshot(const TelemetrySnapshot& snapshot) {
    if (!snapshot.is_valid) return;

    // Update central hub metrics
    m_hub_node.metrics.cpu_percent = snapshot.system.total_cpu_percent;
    m_hub_node.metrics.resident_memory_bytes = snapshot.system.used_ram_bytes;
    m_hub_node.target_glow = 0.45f + (snapshot.system.total_cpu_percent / 100.0f) * 1.5f;

    std::unordered_map<ProcessIdentity, bool> current_seen;

    // Build PID to node index for deterministic parent attachment
    std::unordered_map<uint32_t, ProcessIdentity> active_pid_map;
    for (const auto& [id, node] : m_nodes) {
        active_pid_map[node.pid] = id;
    }

    for (const auto& proc : snapshot.processes) {
        current_seen[proc.identity] = true;

        auto it = m_nodes.find(proc.identity);
        if (it == m_nodes.end()) {
            // New process node — Deterministic position initialization
            GraphNode node;
            node.identity = proc.identity;
            node.pid = proc.identity.pid;
            node.parent_pid = proc.parent_pid;
            node.name = proc.name;
            node.metrics = proc;
            node.alpha = 0.0f; // Smooth fade-in

            // Deterministic golden-ratio angle derived from PID
            float angle = (proc.identity.pid * 0.6180339887f) * 6.2831853f;

            // Anchor near parent if parent exists in graph
            auto parent_it = active_pid_map.find(proc.parent_pid);
            if (parent_it != active_pid_map.end() && parent_it->first != proc.identity.pid) {
                const auto& parent_node = m_nodes[parent_it->second];
                float offset_dist = parent_node.radius + 24.0f + (proc.identity.pid % 6) * 6.0f;
                node.position = parent_node.position + Vec2{std::cos(angle) * offset_dist, std::sin(angle) * offset_dist};
            } else {
                // Root process — Deterministic orbital ring around Hub
                float orbit_radius = 160.0f + ((proc.identity.pid % 19) * 22.0f);
                node.position = {std::cos(angle) * orbit_radius, std::sin(angle) * orbit_radius};
            }

            node.target_position = node.position;
            node.velocity = {0.0f, 0.0f};

            m_nodes[proc.identity] = node;
            it = m_nodes.find(proc.identity);
            m_layout_needs_recalc = true;
        }

        auto& node = it->second;
        node.metrics = proc;
        node.name = proc.name;
        node.parent_pid = proc.parent_pid;
        node.is_terminating = false;

        // Visual mapping: Bounded logarithmic RAM scaling
        float ram_mb = static_cast<float>(proc.resident_memory_bytes) / (1024.0f * 1024.0f);
        node.target_radius = std::clamp(8.0f + std::sqrt(std::max(0.0f, ram_mb)) * 1.35f, 8.0f, 44.0f);

        // Visual mapping: CPU Glow
        node.target_glow = 0.15f + (proc.cpu_percent / 100.0f) * 2.2f;

        // Visual mapping: Category / State Colors
        if (!proc.is_accessible) {
            node.base_color = Vec4{0.22f, 0.28f, 0.38f, 0.7f};
        } else if (proc.cpu_percent > 70.0f) {
            node.base_color = colors::CriticalRed;
        } else if (proc.cpu_percent > 30.0f) {
            node.base_color = colors::WarningAmber;
        } else {
            switch (proc.category) {
                case ProcessCategory::SystemKernel:
                    node.base_color = colors::NeuralViolet;
                    break;
                case ProcessCategory::SystemService:
                    node.base_color = colors::ElectricCyan;
                    break;
                case ProcessCategory::UserInteractive:
                    node.base_color = colors::ActiveGreen;
                    break;
                case ProcessCategory::Development:
                    node.base_color = colors::ElectricCyan;
                    break;
                case ProcessCategory::BackgroundWorker:
                    node.base_color = Vec4{0.45f, 0.65f, 0.90f, 0.85f};
                    break;
            }
        }

        node.is_filtered_out = !m_filter.matches(proc);
    }

    // Detect terminating processes
    for (auto& [id, node] : m_nodes) {
        if (current_seen.find(id) == current_seen.end()) {
            node.is_terminating = true;
        }
    }

    if (m_layout_needs_recalc && m_current_layout != GraphLayoutType::NeuralGraph) {
        trigger_layout_refresh();
    }

    rebuild_edges();
}

void ProcessGraph::rebuild_edges() {
    m_edges.clear();

    // Map PID to identity for fast lookup
    std::unordered_map<uint32_t, ProcessIdentity> pid_to_id;
    for (const auto& [id, node] : m_nodes) {
        if (!node.is_filtered_out) {
            pid_to_id[node.pid] = id;
        }
    }

    for (const auto& [id, node] : m_nodes) {
        if (node.is_filtered_out) continue;

        GraphEdge edge;
        edge.target_id = id;
        edge.target_pos = node.position;

        auto p_it = pid_to_id.find(node.parent_pid);
        if (p_it != pid_to_id.end() && p_it->second != id) {
            const auto& parent_node = m_nodes[p_it->second];

            // Verify temporal consistency: parent must start before or at the same time as child
            if (parent_node.identity.start_time_ns <= node.identity.start_time_ns) {
                edge.relationship_type = RelationshipType::VerifiedParentChild;
                edge.source_id = p_it->second;
                edge.source_pos = parent_node.position;
                edge.color = colors::NeuralViolet;
                edge.alpha = 0.45f;
                edge.pulse_speed = 1.0f + (node.metrics.cpu_percent / 20.0f);
            } else {
                // PID was reused by a new process born AFTER child: Inferred link
                edge.relationship_type = RelationshipType::InferredParentExited;
                edge.source_id = m_hub_node.identity;
                edge.source_pos = m_hub_node.position;
                edge.color = Vec4{0.25f, 0.35f, 0.50f, 0.30f};
                edge.alpha = 0.30f;
                edge.pulse_speed = 0.8f;
            }
        } else if (node.parent_pid != 0) {
            // Parent PID was reported by OS, but parent has already exited
            edge.relationship_type = RelationshipType::InferredParentExited;
            edge.source_id = m_hub_node.identity;
            edge.source_pos = m_hub_node.position;
            edge.color = Vec4{0.25f, 0.35f, 0.50f, 0.30f};
            edge.alpha = 0.30f;
            edge.pulse_speed = 0.8f;
        } else {
            // Root process connected to System Hub
            edge.relationship_type = RelationshipType::InferredSystemRoot;
            edge.source_id = m_hub_node.identity;
            edge.source_pos = m_hub_node.position;
            edge.color = colors::DeepNavy;
            edge.alpha = 0.25f;
            edge.pulse_speed = 0.8f;
        }

        m_edges.push_back(edge);
    }
}

void ProcessGraph::update_simulation(float delta_time) {
    m_sim_tick += delta_time;
    float dt = std::clamp(delta_time, 0.001f, 0.05f);

    // Smooth hub node properties
    m_hub_node.radius += (m_hub_node.target_radius - m_hub_node.radius) * std::min(1.0f, dt * 8.0f);
    m_hub_node.glow_intensity += (m_hub_node.target_glow - m_hub_node.glow_intensity) * std::min(1.0f, dt * 5.0f);

    // Update nodes
    for (auto it = m_nodes.begin(); it != m_nodes.end();) {
        auto& node = it->second;

        // Smooth fade-in
        if (!node.is_terminating) {
            node.alpha += (1.0f - node.alpha) * std::min(1.0f, dt * 6.0f);
        } else {
            // Smooth fade-out before removal
            node.termination_alpha -= dt * 2.5f;
            node.alpha = std::max(0.0f, node.termination_alpha);
            if (node.termination_alpha <= 0.0f) {
                if (m_selected_id == node.identity) {
                    m_selected_id = {0, 0};
                }
                it = m_nodes.erase(it);
                continue;
            }
        }

        // Smooth radius and glow interpolation
        node.radius += (node.target_radius - node.radius) * std::min(1.0f, dt * 6.0f);
        node.glow_intensity += (node.target_glow - node.glow_intensity) * std::min(1.0f, dt * 6.0f);

        // Position interpolation toward target if not Neural (or neural physics)
        if (m_current_layout != GraphLayoutType::NeuralGraph && !node.is_pinned) {
            Vec2 diff = node.target_position - node.position;
            node.position += diff * std::min(1.0f, dt * 6.0f);
        }

        ++it;
    }

    if (m_current_layout == GraphLayoutType::NeuralGraph) {
        apply_neural_layout_step(dt);
    }

    // Animate edge data pulses
    for (auto& edge : m_edges) {
        edge.pulse_progress += edge.pulse_speed * dt;
        if (edge.pulse_progress >= 1.0f) {
            edge.pulse_progress -= 1.0f;
        }

        // Update endpoint positions
        if (edge.source_id == m_hub_node.identity) {
            edge.source_pos = m_hub_node.position;
        } else {
            auto src_it = m_nodes.find(edge.source_id);
            if (src_it != m_nodes.end()) edge.source_pos = src_it->second.position;
        }

        auto tgt_it = m_nodes.find(edge.target_id);
        if (tgt_it != m_nodes.end()) edge.target_pos = tgt_it->second.position;
    }
}

void ProcessGraph::apply_neural_layout_step(float dt) {
    float damping = 0.88f;
    float repulsive_k = 6500.0f;
    float spring_k = 0.045f;
    float center_gravity = 0.08f;
    float max_vel = 180.0f; // Velocity clamp for rock-solid stability

    // Distance cutoff for repulsive forces (enables O(N) spatial scaling)
    float cutoff_dist_sq = 120000.0f; // ~346px

    // Node-Node Repulsion
    for (auto& [id_a, a] : m_nodes) {
        if (a.is_filtered_out || a.is_pinned) continue;

        Vec2 force{0.0f, 0.0f};

        // Repulsion from System Hub
        Vec2 hub_diff = a.position - m_hub_node.position;
        float hub_dist_sq = hub_diff.length_sq();
        if (hub_dist_sq > 1.0f && hub_dist_sq < 360000.0f) {
            float dist = std::sqrt(hub_dist_sq);
            force += (hub_diff / dist) * (repulsive_k * 2.0f / (dist + 30.0f));
        }

        // Repulsion from other nodes (with distance cutoff)
        for (const auto& [id_b, b] : m_nodes) {
            if (id_a == id_b || b.is_filtered_out) continue;

            Vec2 diff = a.position - b.position;
            float dist_sq = diff.length_sq();
            if (dist_sq > 0.1f && dist_sq < cutoff_dist_sq) {
                float dist = std::sqrt(dist_sq);
                float min_sep = a.radius + b.radius + 16.0f;
                float rep = repulsive_k / (dist + 12.0f);
                if (dist < min_sep) {
                    rep *= 2.8f;
                }
                force += (diff / dist) * rep;
            }
        }

        // Centering force
        force -= a.position * center_gravity;

        a.velocity = (a.velocity + force * dt) * damping;

        // Velocity clamping
        float v_len = a.velocity.length();
        if (v_len > max_vel) {
            a.velocity = (a.velocity / v_len) * max_vel;
        }

        a.position += a.velocity * dt;
    }

    // Spring Attraction along Edges
    for (const auto& edge : m_edges) {
        auto src_it = m_nodes.find(edge.source_id);
        auto tgt_it = m_nodes.find(edge.target_id);

        Vec2 src_pos = (src_it != m_nodes.end()) ? src_it->second.position : m_hub_node.position;
        if (tgt_it == m_nodes.end() || tgt_it->second.is_pinned) continue;

        Vec2 diff = src_pos - tgt_it->second.position;
        float dist = diff.length();
        float desired_dist = 65.0f + tgt_it->second.radius;
        float displacement = dist - desired_dist;

        if (dist > 0.001f) {
            Vec2 spring_force = (diff / dist) * (displacement * spring_k);
            tgt_it->second.velocity += spring_force * dt;
            if (src_it != m_nodes.end() && !src_it->second.is_pinned) {
                src_it->second.velocity -= spring_force * dt;
            }
        }
    }
}

void ProcessGraph::calculate_hierarchy_layout() {
    std::unordered_map<uint32_t, std::vector<ProcessIdentity>> children_map;
    std::vector<ProcessIdentity> roots;

    for (const auto& [id, node] : m_nodes) {
        if (node.parent_pid == 0 || node.parent_pid == node.pid) {
            roots.push_back(id);
        } else {
            children_map[node.parent_pid].push_back(id);
        }
    }

    float angle_step = 6.2831853f / std::max(1ULL, roots.size());
    for (size_t i = 0; i < roots.size(); ++i) {
        float angle = i * angle_step;
        float r = 160.0f;
        m_nodes[roots[i]].target_position = {std::cos(angle) * r, std::sin(angle) * r};

        auto c_it = children_map.find(roots[i].pid);
        if (c_it != children_map.end()) {
            float child_arc = angle_step * 0.85f;
            float child_step = child_arc / std::max(1ULL, c_it->second.size());
            float start_a = angle - child_arc * 0.5f;

            for (size_t c = 0; c < c_it->second.size(); ++c) {
                float ca = start_a + c * child_step;
                float cr = r + 130.0f;
                m_nodes[c_it->second[c]].target_position = {std::cos(ca) * cr, std::sin(ca) * cr};
            }
        }
    }
}

void ProcessGraph::calculate_cluster_layout() {
    std::vector<ProcessIdentity> cpu_group, ram_group, io_group, idle_group;

    for (const auto& [id, node] : m_nodes) {
        if (node.metrics.cpu_percent > 10.0f) {
            cpu_group.push_back(id);
        } else if (node.metrics.resident_memory_bytes > 200ULL * 1024 * 1024) {
            ram_group.push_back(id);
        } else if (node.metrics.total_disk_io_bytes_sec() > 1024 * 1024) {
            io_group.push_back(id);
        } else {
            idle_group.push_back(id);
        }
    }

    auto layout_cluster = [this](const std::vector<ProcessIdentity>& group, Vec2 center, float radius) {
        size_t n = group.size();
        for (size_t i = 0; i < n; ++i) {
            float a = (i * 6.2831853f) / std::max(1ULL, n);
            float r = (i == 0) ? 0.0f : radius * (0.3f + 0.7f * ((i % 5) / 5.0f));
            m_nodes[group[i]].target_position = center + Vec2{std::cos(a) * r, std::sin(a) * r};
        }
    };

    layout_cluster(cpu_group,  { 280.0f,  210.0f}, 170.0f);
    layout_cluster(ram_group,  {-280.0f,  210.0f}, 170.0f);
    layout_cluster(io_group,   { 280.0f, -210.0f}, 170.0f);
    layout_cluster(idle_group, {-280.0f, -210.0f}, 220.0f);
}

void ProcessGraph::calculate_timeline_layout() {
    std::vector<std::pair<uint64_t, ProcessIdentity>> sorted;
    for (const auto& [id, node] : m_nodes) {
        sorted.push_back({id.start_time_ns, id});
    }
    std::sort(sorted.begin(), sorted.end());

    float a = 0.0f;
    float r = 100.0f;
    for (const auto& [time, id] : sorted) {
        m_nodes[id].target_position = {std::cos(a) * r, std::sin(a) * r};
        a += 0.32f;
        r += 2.2f;
    }
}

void ProcessGraph::trigger_layout_refresh() {
    switch (m_current_layout) {
        case GraphLayoutType::HierarchyTree:
            calculate_hierarchy_layout();
            break;
        case GraphLayoutType::ResourceClusters:
            calculate_cluster_layout();
            break;
        case GraphLayoutType::TimelineLayout:
            calculate_timeline_layout();
            break;
        case GraphLayoutType::NeuralGraph:
            break;
    }
    m_layout_needs_recalc = false;
}

void ProcessGraph::set_layout(GraphLayoutType layout) {
    m_current_layout = layout;
    m_layout_needs_recalc = true;
    trigger_layout_refresh();
}

void ProcessGraph::select_node(const ProcessIdentity& id) {
    if (m_selected_id == id) return;

    if (auto* prev = get_node_by_identity(m_selected_id)) {
        prev->is_selected = false;
    }

    m_selected_id = id;
    if (auto* next = get_node_by_identity(m_selected_id)) {
        next->is_selected = true;
    }
}

void ProcessGraph::clear_selection() {
    if (auto* cur = get_node_by_identity(m_selected_id)) {
        cur->is_selected = false;
    }
    m_selected_id = {0, 0};
}

const GraphNode* ProcessGraph::get_selected_node() const {
    if (m_selected_id.pid == 0) return nullptr;
    auto it = m_nodes.find(m_selected_id);
    return (it != m_nodes.end()) ? &it->second : nullptr;
}

GraphNode* ProcessGraph::get_node_by_identity(const ProcessIdentity& id) {
    auto it = m_nodes.find(id);
    return (it != m_nodes.end()) ? &it->second : nullptr;
}

const GraphNode* ProcessGraph::find_node_by_pid(uint32_t pid) const {
    for (const auto& [id, node] : m_nodes) {
        if (node.pid == pid) return &node;
    }
    return nullptr;
}

const GraphNode* ProcessGraph::hit_test(const Vec2& world_pos) const {
    if (world_pos.distance(m_hub_node.position) <= m_hub_node.radius) {
        return &m_hub_node;
    }

    for (const auto& [id, node] : m_nodes) {
        if (node.is_filtered_out) continue;
        float hit_rad = std::max(node.radius, 14.0f);
        if (world_pos.distance(node.position) <= hit_rad) {
            return &node;
        }
    }
    return nullptr;
}

void ProcessGraph::set_filter(const GraphFilter& filter) {
    m_filter = filter;
    for (auto& [id, node] : m_nodes) {
        node.is_filtered_out = !m_filter.matches(node.metrics);
    }
    rebuild_edges();
}

size_t ProcessGraph::get_active_node_count() const {
    size_t count = 0;
    for (const auto& [id, node] : m_nodes) {
        if (!node.is_filtered_out) count++;
    }
    return count;
}

size_t ProcessGraph::get_verified_edge_count() const {
    size_t count = 0;
    for (const auto& e : m_edges) {
        if (e.is_verified()) count++;
    }
    return count;
}

size_t ProcessGraph::get_inferred_edge_count() const {
    size_t count = 0;
    for (const auto& e : m_edges) {
        if (!e.is_verified()) count++;
    }
    return count;
}

float ProcessGraph::compute_average_node_velocity() const {
    if (m_nodes.empty()) return 0.0f;
    float sum_vel = 0.0f;
    for (const auto& [id, node] : m_nodes) {
        sum_vel += node.velocity.length();
    }
    return sum_vel / m_nodes.size();
}

} // namespace nerv
