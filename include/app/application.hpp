#pragma once

#include "rendering/camera.hpp"
#include "rendering/renderer.hpp"
#include "rendering/particle_system.hpp"
#include "rendering/shockwave_system.hpp"
#include "model/process_graph.hpp"
#include "history/history_store.hpp"
#include "events/event_detector.hpp"
#include "telemetry/telemetry_collector.hpp"
#include "ui/ui_manager.hpp"

struct GLFWwindow;

namespace nerv {

struct ApplicationConfig {
    int window_width{1600};
    int window_height{900};
    const char* window_title{"NERV \xE2\x80\x94 The Nervous System of Your Computer"};
    bool use_mock_telemetry{false};
};

class Application {
public:
    explicit Application(ApplicationConfig config = {});
    ~Application();

    bool initialize();
    void run();
    void shutdown();

private:
    void process_input();
    void handle_mouse_picking(const Vec2& mouse_screen);

    ApplicationConfig m_config;
    GLFWwindow* m_window{nullptr};

    std::unique_ptr<TelemetryCollector> m_collector;
    ProcessGraph m_graph;
    HistoryStore m_history;
    EventDetector m_event_detector;
    Camera m_camera;
    ParticleSystem m_particles;
    ShockwaveSystem m_shockwaves;
    Renderer m_renderer;
    UIManager m_ui_mgr;

    uint64_t m_last_seen_snapshot_seq{0};
    double m_last_frame_time{0.0};
    float m_total_time{0.0f};

    // Mouse drag state
    bool m_is_dragging{false};
    Vec2 m_last_mouse_pos{0.0f, 0.0f};
};

} // namespace nerv
