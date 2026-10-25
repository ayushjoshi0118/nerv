#pragma once

#include "model/process_graph.hpp"
#include "rendering/camera.hpp"
#include "rendering/renderer.hpp"
#include "rendering/shockwave_system.hpp"
#include "history/history_store.hpp"
#include "events/event_detector.hpp"
#include "telemetry/telemetry_collector.hpp"
#include <string>

namespace nerv {

class UIManager {
public:
    UIManager();

    void initialize();
    void render(ProcessGraph& graph, Camera& camera, HistoryStore& history,
                EventDetector& event_detector, TelemetryCollector& collector,
                Renderer& renderer, const ShockwaveSystem& shockwaves, float delta_time);

    bool is_mouse_over_ui() const { return m_mouse_over_ui; }
    bool is_following_process() const { return m_follow_selected; }
    void cancel_following() { m_follow_selected = false; }

private:
    void render_top_bar(ProcessGraph& graph, Camera& camera, TelemetryCollector& collector,
                        EventDetector& event_detector, const TelemetrySnapshot& snapshot);
    void render_inspector(ProcessGraph& graph, Camera& camera, const HistoryStore& history);
    void render_bottom_panel(ProcessGraph& graph, Camera& camera, const HistoryStore& history,
                             EventDetector& event_detector);
    void render_settings_modal(TelemetryCollector& collector, Renderer& renderer,
                               EventDetector& event_detector, const HistoryStore& history);
    void render_node_labels(const ProcessGraph& graph, const Camera& camera);
    void render_shockwave_labels(const ShockwaveSystem& shockwaves, const Camera& camera);

    static std::string format_bytes(uint64_t bytes);
    static std::string format_throughput(uint64_t bytes_sec);
    static void open_containing_folder(const std::string& path);

    bool m_mouse_over_ui{false};
    bool m_show_settings{false};
    bool m_show_bottom_panel{true};
    bool m_show_inspector{true};
    bool m_follow_selected{false};

    char m_search_buffer[128]{};
    GraphFilter m_filter;

    // Node label display preference: 0 = Always, 1 = On Hover/Selected, 2 = Off
    int m_label_display_mode{1};
};

} // namespace nerv
