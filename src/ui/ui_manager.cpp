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
                       Renderer& renderer, const ShockwaveSystem& shockwaves, float delta_time) {
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

    render_top_bar(graph, camera, collector, event_detector, snapshot);
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
    render_shockwave_labels(shockwaves, camera);
}

void UIManager::render_top_bar(ProcessGraph& graph, Camera& camera, TelemetryCollector& collector,
                               EventDetector& event_detector, const TelemetrySnapshot& snapshot) {
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
        float right_width = 560.0f;
        ImGui::SetCursorPosX(vp->Size.x - right_width);

        // Search Input
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::InputTextWithHint("##Search", "Search PID/Name", m_search_buffer, sizeof(m_search_buffer))) {
            m_filter.search_query = m_search_buffer;
            graph.set_filter(m_filter);
        }
        ImGui::SameLine();

        // Layout Dropdown
        const char* layouts[] = {"Neural", "Hierarchy", "Clusters", "Timeline"};
        int cur_layout = static_cast<int>(graph.get_layout());
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::Combo("##Layout", &cur_layout, layouts, IM_ARRAYSIZE(layouts))) {
            graph.set_layout(static_cast<GraphLayoutType>(cur_layout));
        }
        ImGui::SameLine();

        // Visual effects toggle button
        bool& fx_enabled = event_detector.get_config().enable_visual_effects;
        if (ImGui::Button(fx_enabled ? "FX: ON" : "FX: OFF")) {
            fx_enabled = !fx_enabled;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toggle visual event shockwaves without affecting event detection");
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

            // Relationship integrity label
            const char* rel_label = "Inferred (System Root Grouping)";
            ImVec4 rel_col = ImVec4(0.365f, 0.922f, 1.0f, 1.0f);
            for (const auto& e : graph.get_edges()) {
                if (e.target_id == selected->identity) {
                    rel_label = e.get_relationship_label();
                    rel_col = e.is_verified() ? ImVec4(0.604f, 0.482f, 1.0f, 1.0f) : ImVec4(1.0f, 0.749f, 0.412f, 1.0f);
                    break;
                }
            }
            ImGui::TextDisabled("Link:");
            ImGui::SameLine();
            ImGui::TextColored(rel_col, "%s", rel_label);

            // State Badge
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
            if (ImGui::Checkbox("Enable Restrained HDR Bloom", &bloom)) {
                renderer.set_bloom_enabled(bloom);
            }
            if (bloom) {
                float intensity = renderer.get_bloom_intensity();
                if (ImGui::SliderFloat("Bloom Intensity", &intensity, 0.1f, 2.0f, "%.2f")) {
                    renderer.set_bloom_intensity(intensity);
                }
                float threshold = renderer.get_bloom_threshold();
                if (ImGui::SliderFloat("Bloom Threshold", &threshold, 0.3f, 0.95f, "%.2f")) {
                    renderer.set_bloom_threshold(threshold);
                }
            }

            bool reduced = renderer.is_reduced_motion();
            if (ImGui::Checkbox("Reduced Motion Mode", &reduced)) {
                renderer.set_reduced_motion(reduced);
            }

            ImGui::Combo("Node Labels", &m_label_display_mode, "Always Visible\0On Hover / Selection\0Never\0");
        }

        if (ImGui::CollapsingHeader("Event Detection & Visual Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& cfg = event_detector.get_config();

            ImGui::Checkbox("Enable Visual Shockwave Effects", &cfg.enable_visual_effects);
            ImGui::TextDisabled("When disabled, telemetry events are still accurately logged and detected without visual ripples.");
            ImGui::Separator();

            ImGui::TextColored(ImVec4(0.365f, 0.922f, 1.0f, 1.0f), "Thresholds & Hysteresis");
            ImGui::SliderFloat("System CPU Alert (%)", &cfg.cpu_threshold_percent, 30.0f, 95.0f, "%.1f%%");
            ImGui::SliderFloat("System CPU Hysteresis (%)", &cfg.cpu_hysteresis_percent, 1.0f, 20.0f, "%.1f%%");
            ImGui::TextDisabled("Re-arms after falling below %.1f%%", std::max(0.0f, cfg.cpu_threshold_percent - cfg.cpu_hysteresis_percent));

            ImGui::Spacing();
            ImGui::SliderFloat("Process CPU Spike (%)", &cfg.process_cpu_spike_percent, 20.0f, 95.0f, "%.1f%%");
            ImGui::SliderFloat("Process CPU Hysteresis (%)", &cfg.process_cpu_hysteresis_percent, 2.0f, 20.0f, "%.1f%%");

            ImGui::Spacing();
            ImGui::SliderFloat("RAM Usage Alert (%)", &cfg.memory_threshold_percent, 50.0f, 98.0f, "%.1f%%");
            ImGui::SliderFloat("RAM Hysteresis (%)", &cfg.memory_hysteresis_percent, 1.0f, 15.0f, "%.1f%%");

            ImGui::Spacing();
            ImGui::SliderFloat("Disk Activity Surge (MB/s)", &cfg.disk_spike_mb_sec, 5.0f, 200.0f, "%.1f MB/s");
            ImGui::SliderFloat("Disk Hysteresis (MB/s)", &cfg.disk_hysteresis_mb_sec, 2.0f, 30.0f, "%.1f MB/s");

            ImGui::Spacing();
            ImGui::SliderFloat("Network Bandwidth Surge (MB/s)", &cfg.net_spike_mb_sec, 2.0f, 100.0f, "%.1f MB/s");
            ImGui::SliderFloat("Network Hysteresis (MB/s)", &cfg.net_hysteresis_mb_sec, 1.0f, 20.0f, "%.1f MB/s");

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.365f, 0.922f, 1.0f, 1.0f), "Cooldowns & Filtering");
            float cd = static_cast<float>(cfg.cooldown_seconds);
            if (ImGui::SliderFloat("System Cooldown (sec)", &cd, 1.0f, 20.0f, "%.1f s")) {
                cfg.cooldown_seconds = static_cast<double>(cd);
            }
            float pcd = static_cast<float>(cfg.process_cooldown_seconds);
            if (ImGui::SliderFloat("Process Cooldown (sec)", &pcd, 2.0f, 30.0f, "%.1f s")) {
                cfg.process_cooldown_seconds = static_cast<double>(pcd);
            }

            ImGui::Checkbox("Detect Process Spawns", &cfg.enable_spawn_events);
            ImGui::SameLine();
            ImGui::Checkbox("Detect Process Exits", &cfg.enable_exit_events);
            ImGui::Checkbox("Detect Per-Process CPU Spikes", &cfg.enable_process_cpu_spikes);
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
            ImGui::Text("GPU Draw Calls (Total): %zu", diag.total_draw_calls);
            ImGui::BulletText("Node Draw Calls: %zu (GPU Instanced)", diag.node_draw_calls);
            ImGui::BulletText("Edge Draw Calls: %zu (GPU Batched)", diag.edge_draw_calls);
            ImGui::Text("Total Vertices: %zu", diag.total_vertices);
            ImGui::Text("Visible Nodes: %zu", diag.visible_nodes);
            ImGui::Text("Rendered Connections: %zu", diag.rendered_edges);
            ImGui::Text("Active Particles: %zu", diag.active_particles);
            ImGui::Text("Active Shockwaves: %zu", diag.active_shockwaves);
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
    GraphLOD lod = graph.compute_lod(zoom);

    bool show_all = (m_label_display_mode == 0) && (lod != GraphLOD::LowDetail);

    // Hub label
    const auto& hub = graph.get_hub_node();
    Vec2 hub_screen = camera.world_to_screen(hub.position);
    ImVec2 hub_text_size = ImGui::CalcTextSize("SYSTEM CORE");
    ImVec2 hub_min = ImVec2(hub_screen.x - hub_text_size.x * 0.5f - 8.0f, hub_screen.y - hub.radius - 22.0f);
    ImVec2 hub_max = ImVec2(hub_screen.x + hub_text_size.x * 0.5f + 8.0f, hub_screen.y - hub.radius - 4.0f);
    draw_list->AddRectFilled(hub_min, hub_max, IM_COL32(11, 18, 32, 220), 4.0f);
    draw_list->AddRect(hub_min, hub_max, IM_COL32(93, 235, 255, 120), 4.0f);
    draw_list->AddText(ImVec2(hub_screen.x - hub_text_size.x * 0.5f, hub_screen.y - hub.radius - 20.0f),
                       IM_COL32(93, 235, 255, 255), "SYSTEM CORE");

    struct ScreenRect { float x1, y1, x2, y2; };
    std::vector<ScreenRect> occupied_labels;
    occupied_labels.push_back({hub_min.x, hub_min.y, hub_max.x, hub_max.y});

    // Helper for AABB collision detection
    auto overlaps = [](const ScreenRect& a, const ScreenRect& b) {
        return !(a.x2 < b.x1 || a.x1 > b.x2 || a.y2 < b.y1 || a.y1 > b.y2);
    };

    // Node labels with LOD and screen-space collision culling
    for (const auto& [id, node] : graph.get_nodes()) {
        if (node.is_filtered_out) continue;

        // Level-of-Detail filtering
        bool priority_node = node.is_selected || node.is_hovered;
        bool should_show = false;

        if (priority_node) {
            should_show = true;
        } else if (lod == GraphLOD::LowDetail) {
            should_show = false; // Cull all non-selected/hovered labels when zoomed out
        } else if (lod == GraphLOD::MediumDetail) {
            should_show = (node.metrics.cpu_percent > 12.0f);
        } else {
            should_show = show_all;
        }

        if (!should_show) continue;

        Vec2 screen = camera.world_to_screen(node.position);

        // Viewport bounds check
        if (screen.x < -120.0f || screen.x > camera.get_viewport_width() + 120.0f ||
            screen.y < -120.0f || screen.y > camera.get_viewport_height() + 120.0f) {
            continue;
        }

        std::string label = node.name;
        if (node.is_selected || zoom > 1.2f) {
            label += " [" + std::to_string(node.pid) + "]";
        }

        ImVec2 text_sz = ImGui::CalcTextSize(label.c_str());
        float pad_x = 7.0f;
        float pad_y = 3.0f;
        float dot_offset = 10.0f;

        float pill_w = text_sz.x + pad_x * 2.0f + dot_offset;
        float pill_h = text_sz.y + pad_y * 2.0f;

        ImVec2 pill_min(screen.x - pill_w * 0.5f, screen.y - node.radius - pill_h - 6.0f);
        ImVec2 pill_max(pill_min.x + pill_w, pill_min.y + pill_h);

        ScreenRect candidate_rect{pill_min.x - 4.0f, pill_min.y - 4.0f, pill_max.x + 4.0f, pill_max.y + 4.0f};

        // Collision culling for non-priority labels
        if (!priority_node) {
            bool collision = false;
            for (const auto& occ : occupied_labels) {
                if (overlaps(candidate_rect, occ)) {
                    collision = true;
                    break;
                }
            }
            if (collision) continue; // Skip to prevent unreadable text overlap
        }

        occupied_labels.push_back(candidate_rect);

        // Glassmorphic pill backdrop
        ImU32 bg_col = node.is_selected ? IM_COL32(11, 24, 48, 235) :
                       node.is_hovered  ? IM_COL32(16, 28, 54, 225) :
                                          IM_COL32(8, 14, 26, 205);

        ImU32 border_col = node.is_selected ? IM_COL32(93, 235, 255, 180) :
                           node.is_hovered  ? IM_COL32(93, 235, 255, 100) :
                                              IM_COL32(35, 55, 85, 90);

        draw_list->AddRectFilled(pill_min, pill_max, bg_col, 4.0f);
        draw_list->AddRect(pill_min, pill_max, border_col, 4.0f);

        // Activity indicator dot
        ImU32 dot_col = (node.metrics.cpu_percent > 70.0f) ? IM_COL32(255, 101, 122, 255) : // Critical red
                        (node.metrics.cpu_percent > 30.0f) ? IM_COL32(255, 191, 105, 255) : // Warning amber
                        (!node.metrics.is_accessible)      ? IM_COL32(120, 140, 170, 200) : // Inaccessible dim
                                                             IM_COL32(99, 245, 176, 255);  // Active green

        draw_list->AddCircleFilled(ImVec2(pill_min.x + pad_x + 3.0f, pill_min.y + pill_h * 0.5f), 3.0f, dot_col);

        // Crisp text
        ImU32 text_col = node.is_selected ? IM_COL32(255, 255, 255, 255) :
                         node.is_hovered  ? IM_COL32(93, 235, 255, 245) :
                                            IM_COL32(237, 243, 255, 200);

        draw_list->AddText(ImVec2(pill_min.x + pad_x + dot_offset, pill_min.y + pad_y), text_col, label.c_str());
    }
}

void UIManager::render_shockwave_labels(const ShockwaveSystem& shockwaves, const Camera& camera) {
    const auto& sw_list = shockwaves.get_shockwaves();
    if (sw_list.empty()) return;

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    float view_w = camera.get_viewport_width();
    float view_h = camera.get_viewport_height();
    float zoom = camera.get_zoom();

    for (const auto& sw : sw_list) {
        if (sw.badge_text.empty()) continue;

        Vec2 sc = camera.world_to_screen(sw.center);
        if (sc.x < -150.0f || sc.x > view_w + 150.0f || sc.y < -150.0f || sc.y > view_h + 150.0f) {
            continue;
        }

        float p = sw.get_progress();
        float alpha_factor = (1.0f - p * p);
        if (alpha_factor <= 0.02f) continue;

        // Position: float cleanly above the expanding ripple wavefront with gentle upward drift
        float ring_radius_screen = sw.current_radius * zoom;
        float drift = p * 34.0f;
        float y_pos = sc.y - (ring_radius_screen * 0.42f) - 20.0f - drift;
        float x_pos = sc.x;

        ImVec2 text_sz = ImGui::CalcTextSize(sw.badge_text.c_str());
        float pad_x = 10.0f;
        float pad_y = 5.0f;
        float pill_w = text_sz.x + pad_x * 2.0f + 14.0f;
        float pill_h = text_sz.y + pad_y * 2.0f;

        ImVec2 pill_min(x_pos - pill_w * 0.5f, y_pos - pill_h * 0.5f);
        ImVec2 pill_max(pill_min.x + pill_w, pill_min.y + pill_h);

        int alpha_bg = static_cast<int>(alpha_factor * 235.0f);
        int alpha_border = static_cast<int>(alpha_factor * 255.0f);
        int alpha_text = static_cast<int>(alpha_factor * 255.0f);

        ImU32 bg_col = IM_COL32(11, 18, 32, alpha_bg);

        // Border and accent color based on severity
        ImU32 border_col = (sw.severity == EventSeverity::Critical) ? IM_COL32(255, 101, 122, alpha_border) :
                           (sw.severity == EventSeverity::Warning)  ? IM_COL32(255, 191, 105, alpha_border) :
                                                                      IM_COL32(93, 235, 255, alpha_border);

        // Render rounded frosted capsule badge (non-interactive, zero mouse interception)
        draw_list->AddRectFilled(pill_min, pill_max, bg_col, 8.0f);
        draw_list->AddRect(pill_min, pill_max, border_col, 8.0f, 0, 1.5f);

        // Glowing indicator dot
        ImVec2 dot_pos(pill_min.x + pad_x + 3.0f, y_pos);
        draw_list->AddCircleFilled(dot_pos, 3.5f, border_col);

        // Crisp measurement label text displaying genuine metric measurement
        ImVec2 text_pos(pill_min.x + pad_x + 12.0f, pill_min.y + pad_y);
        draw_list->AddText(text_pos, IM_COL32(237, 243, 255, alpha_text), sw.badge_text.c_str());
    }
}

} // namespace nerv
