#include "ui/ui_manager.hpp"
#include "ui/ui_theme.hpp"
#include <imgui.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>

#ifdef APIENTRY
#undef APIENTRY
#endif

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace nerv {

UIManager::UIManager() = default;

void UIManager::initialize() {
    UITheme::apply_nerv_theme();
}

std::string UIManager::format_bytes(uint64_t bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double d_bytes = static_cast<double>(bytes);
    while (d_bytes >= 1024.0 && i < 4) {
        d_bytes /= 1024.0;
        i++;
    }
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << d_bytes << " " << units[i];
    return ss.str();
}

std::string UIManager::format_throughput(uint64_t bytes_sec) {
    return format_bytes(bytes_sec) + "/s";
}

void UIManager::open_containing_folder(const std::string& path) {
    if (path.empty()) return;
#ifdef _WIN32
    try {
        std::filesystem::path p(path);
        if (std::filesystem::exists(p)) {
            std::string parent = p.parent_path().string();
            ShellExecuteA(nullptr, "open", parent.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
        }
    } catch (...) {}
#endif
}

void UIManager::render(ProcessGraph& graph, Camera& camera, HistoryStore& history,
                       EventDetector& event_detector, TelemetryCollector& collector,
                       Renderer& renderer, float delta_time) {
    (void)delta_time;
    m_mouse_over_ui = ImGui::GetIO().WantCaptureMouse;

    auto snapshot_ptr = collector.get_latest_snapshot();
    const TelemetrySnapshot& snapshot = *snapshot_ptr;

    // Follow process camera lock
    if (m_follow_selected) {
        const auto* sel = graph.get_selected_node();
        if (sel) {
            camera.focus_on(sel->position, camera.get_zoom());
        } else {
            m_follow_selected = false;
        }
    }

    render_top_bar(graph, camera, collector, snapshot);
    if (m_show_inspector) {
        render_inspector(graph, camera, history);
    }
    if (m_show_bottom_panel) {
        render_bottom_panel(graph, camera, history, event_detector);
    }
    if (m_show_settings) {
        render_settings_modal(collector, renderer, event_detector, history);
    }

    render_node_labels(graph, camera);
}

void UIManager::render_top_bar(ProcessGraph& graph, Camera& camera, TelemetryCollector& collector,
                               const TelemetrySnapshot& snapshot) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, 52.0f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.020f, 0.027f, 0.051f, 0.95f));

    if (ImGui::Begin("##TopBar", nullptr, flags)) {
        // App Identity Brand
        ImGui::TextColored(ImVec4(0.365f, 0.922f, 1.0f, 1.0f), "NERV");
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.604f, 0.482f, 1.0f, 1.0f), "SYSTEM NERVOUS SYSTEM");
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        // System Telemetry Badges
        // CPU
        float cpu = snapshot.is_valid ? snapshot.system.total_cpu_percent : 0.0f;
        ImVec4 cpu_col = (cpu > 75.0f) ? ImVec4(1.0f, 0.396f, 0.478f, 1.0f) :
                         (cpu > 35.0f) ? ImVec4(1.0f, 0.749f, 0.412f, 1.0f) :
                                         ImVec4(0.388f, 0.961f, 0.690f, 1.0f);
        ImGui::Text("CPU:");
        ImGui::SameLine();
        ImGui::TextColored(cpu_col, "%.1f%%", cpu);
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        // RAM
        float ram_pct = snapshot.is_valid ? snapshot.system.ram_usage_percent : 0.0f;
        ImGui::Text("RAM:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.365f, 0.922f, 1.0f, 1.0f), "%.1f%% (%s / %s)",
                           ram_pct,
                           format_bytes(snapshot.system.used_ram_bytes).c_str(),
                           format_bytes(snapshot.system.total_ram_bytes).c_str());
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        // Disk
        ImGui::Text("DISK:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.604f, 0.482f, 1.0f, 1.0f), "%s",
                           format_throughput(snapshot.system.total_disk_bytes_sec()).c_str());
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        // Network
        ImGui::Text("NET:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.388f, 0.961f, 0.690f, 1.0f), "%s",
                           format_throughput(snapshot.system.total_network_bytes_sec()).c_str());
        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        // Process count
        ImGui::TextDisabled("Procs:");
        ImGui::SameLine();
        ImGui::Text("%zu", graph.get_active_node_count());
        ImGui::SameLine();

        // Right side controls
        float right_width = 460.0f;
        ImGui::SetCursorPosX(vp->Size.x - right_width);

        // Search Input
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::InputTextWithHint("##Search", "Search PID/Name", m_search_buffer, sizeof(m_search_buffer))) {
            m_filter.search_query = m_search_buffer;
            graph.set_filter(m_filter);
        }
        ImGui::SameLine();

        // Layout Dropdown
        const char* layouts[] = {"Neural", "Hierarchy", "Clusters", "Timeline"};
        int cur_layout = static_cast<int>(graph.get_layout());
        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::Combo("##Layout", &cur_layout, layouts, IM_ARRAYSIZE(layouts))) {
            graph.set_layout(static_cast<GraphLayoutType>(cur_layout));
        }
        ImGui::SameLine();

        // Pause/Resume button
        bool paused = collector.is_paused();
        if (ImGui::Button(paused ? "Resume" : "Pause")) {
            collector.set_paused(!paused);
        }
        ImGui::SameLine();

        // Reset View button
        if (ImGui::Button("Reset View")) {
            camera.reset_view();
            m_follow_selected = false;
        }
        ImGui::SameLine();

        // Settings Modal button
        if (ImGui::Button("Settings")) {
            m_show_settings = true;
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void UIManager::render_inspector(ProcessGraph& graph, Camera& camera, const HistoryStore& history) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float panel_w = 340.0f;
    float top_h = 52.0f;
    float bottom_h = m_show_bottom_panel ? 200.0f : 0.0f;
    float panel_h = vp->Size.y - top_h - bottom_h;

    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - panel_w, vp->Pos.y + top_h));
    ImGui::SetNextWindowSize(ImVec2(panel_w, panel_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("PROCESS INSPECTOR", &m_show_inspector, flags)) {
        const GraphNode* selected = graph.get_selected_node();

        if (selected) {
            // Header: Name & PID
            ImGui::TextColored(ImVec4(0.365f, 0.922f, 1.0f, 1.0f), "%s", selected->name.c_str());
            ImGui::TextDisabled("PID: %u | PPID: %u", selected->pid, selected->parent_pid);

            // State Badge
            ImGui::SameLine();
            if (selected->is_terminating) {
                ImGui::TextColored(ImVec4(1.0f, 0.396f, 0.478f, 1.0f), "[TERMINATED]");
            } else if (!selected->metrics.is_accessible) {
                ImGui::TextColored(ImVec4(1.0f, 0.749f, 0.412f, 1.0f), "[PROTECTED / INACCESSIBLE]");
            } else {
                ImGui::TextColored(ImVec4(0.388f, 0.961f, 0.690f, 1.0f), "[ACTIVE]");
            }

            ImGui::Separator();

            // CPU Metric & Sparkline
            ImGui::Text("CPU Usage:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.365f, 0.922f, 1.0f, 1.0f), "%.2f%%", selected->metrics.cpu_percent);

            const auto* cpu_hist = history.get_process_cpu_history(selected->identity);
            if (cpu_hist && !cpu_hist->empty()) {
                auto vals = cpu_hist->to_vector();
                ImGui::PlotLines("##ProcCPU", vals.data(), static_cast<int>(vals.size()), 0,
                                 nullptr, 0.0f, 100.0f, ImVec2(panel_w - 30.0f, 45.0f));
            }

            // RAM Metric & Sparkline
            float ram_mb = static_cast<float>(selected->metrics.resident_memory_bytes) / (1024.0f * 1024.0f);
            float virt_mb = static_cast<float>(selected->metrics.virtual_memory_bytes) / (1024.0f * 1024.0f);
            ImGui::Text("Resident RAM (Working Set):");
            ImGui::TextColored(ImVec4(0.388f, 0.961f, 0.690f, 1.0f), "%.1f MB (%s)",
                               ram_mb, format_bytes(selected->metrics.resident_memory_bytes).c_str());
            ImGui::TextDisabled("Commit / Virtual: %.1f MB", virt_mb);

            const auto* ram_hist = history.get_process_ram_history(selected->identity);
            if (ram_hist && !ram_hist->empty()) {
                auto vals = ram_hist->to_vector();
                ImGui::PlotLines("##ProcRAM", vals.data(), static_cast<int>(vals.size()), 0,
                                 nullptr, 0.0f, ram_mb * 1.5f + 10.0f, ImVec2(panel_w - 30.0f, 45.0f));
            }

            ImGui::Separator();

            // I/O & Threads
            ImGui::Text("Threads: %u", selected->metrics.thread_count);
            ImGui::Text("Disk Read: %s", format_throughput(selected->metrics.disk_read_bytes_sec).c_str());
            ImGui::Text("Disk Write: %s", format_throughput(selected->metrics.disk_write_bytes_sec).c_str());
            ImGui::TextDisabled("Category: %s", ProcessMetrics::category_to_string(selected->metrics.category));

            ImGui::Separator();

            // Executable Path
            ImGui::TextDisabled("Executable Path:");
            if (!selected->metrics.exe_path.empty()) {
                ImGui::TextWrapped("%s", selected->metrics.exe_path.c_str());
            } else {
                ImGui::TextDisabled("(Access denied or system process)");
            }

            ImGui::Spacing();

            // Actions
            if (ImGui::Button("Focus Node")) {
                camera.focus_on(selected->position, 1.8f);
            }
            ImGui::SameLine();
            if (ImGui::Button(m_follow_selected ? "Unfollow" : "Follow")) {
                m_follow_selected = !m_follow_selected;
            }
            ImGui::SameLine();
            if (ImGui::Button(selected->is_pinned ? "Unpin" : "Pin")) {
                auto* mut_node = graph.get_node_by_identity(selected->identity);
                if (mut_node) mut_node->is_pinned = !mut_node->is_pinned;
            }

            if (!selected->metrics.exe_path.empty()) {
                if (ImGui::Button("Open Containing Folder")) {
                    open_containing_folder(selected->metrics.exe_path);
                }
                ImGui::SameLine();
                if (ImGui::Button("Copy Path")) {
                    ImGui::SetClipboardText(selected->metrics.exe_path.c_str());
                }
            }

            if (ImGui::Button("Clear Selection")) {
                graph.clear_selection();
                m_follow_selected = false;
            }
        } else {
            // No process selected: Show Central System Hub Info
            const auto& hub = graph.get_hub_node();
            ImGui::TextColored(ImVec4(0.365f, 0.922f, 1.0f, 1.0f), "SYSTEM CORE TELEMETRY");
            ImGui::TextDisabled("Central OS Kernel & Aggregate Hub");
            ImGui::Separator();

            ImGui::Text("Total Processes: %zu", graph.get_active_node_count());
            ImGui::Text("Active Edges: %zu", graph.get_edge_count());
            ImGui::Text("Total CPU: %.1f%%", hub.metrics.cpu_percent);
            ImGui::Text("Total RAM Used: %s", format_bytes(hub.metrics.resident_memory_bytes).c_str());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextWrapped("Click any node in the nervous system graph to inspect process details, thread counts, I/O rates, and live memory charts.");
        }
    }
    ImGui::End();
}

void UIManager::render_bottom_panel(ProcessGraph& graph, Camera& camera, const HistoryStore& history,
                                    EventDetector& event_detector) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float panel_h = 200.0f;
    float panel_w = m_show_inspector ? (vp->Size.x - 340.0f) : vp->Size.x;

    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - panel_h));
    ImGui::SetNextWindowSize(ImVec2(panel_w, panel_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("SYSTEM TELEMETRY & EVENT TIMELINE", &m_show_bottom_panel, flags)) {
        if (ImGui::BeginTabBar("##BottomTabs")) {
            // Tab 1: Live System Charts
            if (ImGui::BeginTabItem("Activity Charts")) {
                float chart_w = (panel_w - 60.0f) / 4.0f;
                float chart_h = 100.0f;

                // CPU History
                const auto& cpu_hist = history.get_cpu_history();
                if (!cpu_hist.empty()) {
                    auto vals = cpu_hist.to_vector();
                    ImGui::BeginGroup();
                    ImGui::TextColored(ImVec4(0.365f, 0.922f, 1.0f, 1.0f), "CPU History (%%)");
                    ImGui::PlotLines("##SysCPU", vals.data(), static_cast<int>(vals.size()), 0,
                                     nullptr, 0.0f, 100.0f, ImVec2(chart_w, chart_h));
                    ImGui::EndGroup();
                    ImGui::SameLine();
                }

                // RAM History
                const auto& ram_hist = history.get_ram_pct_history();
                if (!ram_hist.empty()) {
                    auto vals = ram_hist.to_vector();
                    ImGui::BeginGroup();
                    ImGui::TextColored(ImVec4(0.604f, 0.482f, 1.0f, 1.0f), "RAM Load (%%)");
                    ImGui::PlotLines("##SysRAM", vals.data(), static_cast<int>(vals.size()), 0,
                                     nullptr, 0.0f, 100.0f, ImVec2(chart_w, chart_h));
                    ImGui::EndGroup();
                    ImGui::SameLine();
                }

                // Disk Throughput History
                const auto& disk_hist = history.get_disk_read_history();
                if (!disk_hist.empty()) {
                    auto vals = disk_hist.to_vector();
                    ImGui::BeginGroup();
                    ImGui::TextColored(ImVec4(0.388f, 0.961f, 0.690f, 1.0f), "Disk Read (MB/s)");
                    ImGui::PlotLines("##DiskRead", vals.data(), static_cast<int>(vals.size()), 0,
                                     nullptr, 0.0f, 50.0f, ImVec2(chart_w, chart_h));
                    ImGui::EndGroup();
                    ImGui::SameLine();
                }

                // Network RX History
                const auto& net_hist = history.get_net_rx_history();
                if (!net_hist.empty()) {
                    auto vals = net_hist.to_vector();
                    ImGui::BeginGroup();
                    ImGui::TextColored(ImVec4(1.0f, 0.749f, 0.412f, 1.0f), "Network RX (MB/s)");
                    ImGui::PlotLines("##NetRX", vals.data(), static_cast<int>(vals.size()), 0,
                                     nullptr, 0.0f, 20.0f, ImVec2(chart_w, chart_h));
                    ImGui::EndGroup();
                }

                ImGui::EndTabItem();
            }

            // Tab 2: System Events & Threshold Alerts Log
            if (ImGui::BeginTabItem("Threshold Events Log")) {
                const auto& log = event_detector.get_event_log();

                if (ImGui::Button("Clear Log")) {
                    event_detector.clear_events();
                }
                ImGui::SameLine();
                ImGui::TextDisabled("Total recorded events: %zu (Click any event to locate process)", log.size());

                if (ImGui::BeginTable("##EventsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                    ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableSetupColumn("Event Title", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableHeadersRow();

                    for (const auto& ev : log) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%llu", static_cast<unsigned long long>(ev.id));

                        ImGui::TableSetColumnIndex(1);
                        ImVec4 sev_col = (ev.severity == EventSeverity::Critical) ? ImVec4(1.0f, 0.396f, 0.478f, 1.0f) :
                                         (ev.severity == EventSeverity::Warning)  ? ImVec4(1.0f, 0.749f, 0.412f, 1.0f) :
                                                                                    ImVec4(0.365f, 0.922f, 1.0f, 1.0f);
                        ImGui::TextColored(sev_col, "%s", SystemEvent::severity_to_string(ev.severity));

                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%s", ev.title.c_str());

                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%s", ev.description.c_str());

                        ImGui::TableSetColumnIndex(4);
                        if (ev.related_process.pid != 0) {
                            std::string btn_label = "Locate##" + std::to_string(ev.id);
                            if (ImGui::SmallButton(btn_label.c_str())) {
                                graph.select_node(ev.related_process);
                                const auto* node = graph.get_selected_node();
                                if (node) camera.focus_on(node->position, 1.8f);
                            }
                        } else {
                            ImGui::TextDisabled("System");
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void UIManager::render_settings_modal(TelemetryCollector& collector, Renderer& renderer,
                                      EventDetector& event_detector, const HistoryStore& history) {
    ImGui::SetNextWindowSize(ImVec2(520.0f, 440.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("NERV Configuration & Diagnostics", &m_show_settings)) {
        if (ImGui::CollapsingHeader("Telemetry Sampling", ImGuiTreeNodeFlags_DefaultOpen)) {
            int interval = static_cast<int>(collector.get_sampling_interval_ms());
            if (ImGui::SliderInt("Sampling Interval (ms)", &interval, 200, 5000)) {
                collector.set_sampling_interval_ms(interval);
            }
            ImGui::TextDisabled("Lower interval increases responsiveness; higher interval reduces background CPU overhead.");
        }

        if (ImGui::CollapsingHeader("Visual Effects & Accessibility", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool bloom = renderer.is_bloom_enabled();
            if (ImGui::Checkbox("Enable Bloom & Shaders Glow", &bloom)) {
                renderer.set_bloom_enabled(bloom);
            }

            bool reduced = renderer.is_reduced_motion();
            if (ImGui::Checkbox("Reduced Motion Mode", &reduced)) {
                renderer.set_reduced_motion(reduced);
            }

            ImGui::Combo("Node Labels", &m_label_display_mode, "Always Visible\0On Hover / Selection\0Never\0");
        }

        if (ImGui::CollapsingHeader("Shockwave Event Thresholds", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& cfg = event_detector.get_config();
            ImGui::SliderFloat("CPU Spike Alert (%)", &cfg.cpu_threshold_percent, 30.0f, 95.0f, "%.1f%%");
            ImGui::SliderFloat("RAM Usage Alert (%)", &cfg.memory_threshold_percent, 50.0f, 98.0f, "%.1f%%");
            ImGui::SliderFloat("Disk Spike (MB/s)", &cfg.disk_spike_mb_sec, 5.0f, 200.0f, "%.1f MB/s");
            ImGui::SliderFloat("Cooldown Time (sec)", reinterpret_cast<float*>(&cfg.cooldown_seconds), 1.0f, 15.0f, "%.1f s");
        }

        if (ImGui::CollapsingHeader("Data Export", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button("Export Session to CSV")) {
                history.export_to_csv("nerv_telemetry_export.csv");
            }
            ImGui::SameLine();
            if (ImGui::Button("Export Session to JSON")) {
                history.export_to_json("nerv_telemetry_export.json");
            }
        }

        if (ImGui::CollapsingHeader("Engine Diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto& diag = renderer.get_diagnostics();
            ImGui::Text("Render FPS: %.1f (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Text("Visible Nodes: %zu", diag.visible_nodes);
            ImGui::Text("Rendered Edges: %zu", diag.rendered_edges);
            ImGui::Text("Active Shockwaves: %zu", diag.active_shockwaves);
            ImGui::Text("Active Particles: %zu", diag.active_particles);
            ImGui::Text("Render Prep Duration: %.2f ms", diag.cpu_prep_time_ms);
        }

        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(100.0f, 0.0f))) {
            m_show_settings = false;
        }
    }
    ImGui::End();
}

void UIManager::render_node_labels(const ProcessGraph& graph, const Camera& camera) {
    if (m_label_display_mode == 2) return; // Never

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    float zoom = camera.get_zoom();

    bool show_all = (m_label_display_mode == 0) && (zoom > 0.65f);

    // Hub label
    const auto& hub = graph.get_hub_node();
    Vec2 hub_screen = camera.world_to_screen(hub.position);
    draw_list->AddText(ImVec2(hub_screen.x - 30.0f, hub_screen.y - hub.radius - 16.0f),
                       IM_COL32(93, 235, 255, 220), "SYSTEM CORE");

    // Node labels
    for (const auto& [id, node] : graph.get_nodes()) {
        if (node.is_filtered_out) continue;

        bool should_show = show_all || node.is_selected || node.is_hovered;
        if (!should_show) continue;

        Vec2 screen = camera.world_to_screen(node.position);

        // Viewport bounds check
        if (screen.x < -100.0f || screen.x > camera.get_viewport_width() + 100.0f ||
            screen.y < -100.0f || screen.y > camera.get_viewport_height() + 100.0f) {
            continue;
        }

        std::string label = node.name;
        if (node.is_selected || zoom > 1.2f) {
            label += " [" + std::to_string(node.pid) + "]";
        }

        ImU32 text_col = node.is_selected ? IM_COL32(255, 255, 255, 255) :
                         node.is_hovered  ? IM_COL32(93, 235, 255, 240) :
                                            IM_COL32(237, 243, 255, 180);

        float text_width = ImGui::CalcTextSize(label.c_str()).x;
        draw_list->AddText(ImVec2(screen.x - text_width * 0.5f, screen.y - node.radius - 14.0f),
                           text_col, label.c_str());
    }
}

} // namespace nerv
