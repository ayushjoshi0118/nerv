#include "app/application.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>

#ifdef APIENTRY
#undef APIENTRY
#endif

#ifdef _WIN32
#include "platform/windows/windows_system_monitor.hpp"
#endif
#include "telemetry/mock_system_monitor.hpp"

namespace nerv {

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "[GLFW Error " << error << "]: " << description << std::endl;
}

Application::Application(ApplicationConfig config)
    : m_config(config),
      m_camera(static_cast<float>(config.window_width), static_cast<float>(config.window_height)),
      m_particles(200) {}

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // 4x MSAA

    m_window = glfwCreateWindow(m_config.window_width, m_config.window_height,
                                m_config.window_title, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable VSync

    glfwSetWindowUserPointer(m_window, this);

    // Initialize GLAD
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD OpenGL loader" << std::endl;
        return false;
    }

    // Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    m_ui_mgr.initialize();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize Renderer
    if (!m_renderer.initialize("assets")) {
        std::cerr << "Renderer initialization failed" << std::endl;
        return false;
    }

    // Initialize Telemetry Collector
    std::unique_ptr<ISystemMonitor> monitor;
#ifdef _WIN32
    if (!m_config.use_mock_telemetry) {
        monitor = std::make_unique<WindowsSystemMonitor>();
    } else {
        monitor = std::make_unique<MockSystemMonitor>();
    }
#else
    monitor = std::make_unique<MockSystemMonitor>();
#endif

    m_collector = std::make_unique<TelemetryCollector>(std::move(monitor));
    m_collector->start();

    m_last_frame_time = glfwGetTime();

    // Trigger initial central shockwave
    m_shockwaves.trigger({0.0f, 0.0f}, colors::ElectricCyan, 320.0f, 1.8f);

    return true;
}

void Application::shutdown() {
    if (m_collector) {
        m_collector->stop();
        m_collector.reset();
    }

    m_renderer.shutdown();

    if (m_window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
    }
}

void Application::process_input() {
    ImGuiIO& io = ImGui::GetIO();

    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    if (w > 0 && h > 0) {
        m_camera.set_viewport_size(static_cast<float>(w), static_cast<float>(h));
        m_renderer.resize(w, h);
    }

    double mx, my;
    glfwGetCursorPos(m_window, &mx, &my);
    Vec2 cur_mouse{static_cast<float>(mx), static_cast<float>(my)};

    // Update node hover state
    if (!io.WantCaptureMouse) {
        Vec2 world_pos = m_camera.screen_to_world(cur_mouse);
        const auto* hit = m_graph.hit_test(world_pos);

        // Clear hover flags
        for (auto& [id, node] : const_cast<std::unordered_map<ProcessIdentity, GraphNode>&>(m_graph.get_nodes())) {
            node.is_hovered = false;
        }

        if (hit) {
            auto* node_mut = m_graph.get_node_by_identity(hit->identity);
            if (node_mut) node_mut->is_hovered = true;
        }

        // Left Click: Node selection or Pan background
        int left_btn = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT);
        int mid_btn = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE);

        if (left_btn == GLFW_PRESS || mid_btn == GLFW_PRESS) {
            if (!m_is_dragging) {
                // Check if hit node on mouse down
                if (left_btn == GLFW_PRESS && hit) {
                    m_graph.select_node(hit->identity);
                    m_ui_mgr.cancel_following();
                } else if (!hit && left_btn == GLFW_PRESS) {
                    m_graph.clear_selection();
                    m_ui_mgr.cancel_following();
                }
                m_is_dragging = true;
                m_last_mouse_pos = cur_mouse;
            } else {
                // Dragging to pan
                Vec2 delta = cur_mouse - m_last_mouse_pos;
                m_camera.pan(delta);
                m_camera.add_pan_impulse(delta * 0.06f);
                m_ui_mgr.cancel_following();
                m_last_mouse_pos = cur_mouse;
            }
        } else {
            m_is_dragging = false;
        }

        // Mouse Wheel Zoom
        if (io.MouseWheel != 0.0f) {
            float zoom_factor = (io.MouseWheel > 0) ? 1.15f : 0.85f;
            m_camera.zoom_at(cur_mouse, zoom_factor);
        }
    } else {
        m_is_dragging = false;
    }
}

void Application::run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        double current_time = glfwGetTime();
        float delta_time = static_cast<float>(current_time - m_last_frame_time);
        m_last_frame_time = current_time;
        m_total_time += delta_time;

        // Process mouse/keyboard navigation
        process_input();

        // 1. Process Telemetry Snapshot
        auto snapshot = m_collector->get_latest_snapshot();
        if (snapshot && snapshot->is_valid && snapshot->sequence_number > m_last_seen_snapshot_seq) {
            m_last_seen_snapshot_seq = snapshot->sequence_number;

            m_graph.update_from_snapshot(*snapshot);
            m_history.record_snapshot(*snapshot);

            // Check events and trigger signature shockwaves with genuine measurements
            auto events = m_event_detector.update(*snapshot);

            // Trigger visual shockwave animations only if visual effects are enabled
            if (m_event_detector.get_config().enable_visual_effects) {
                for (const auto& ev : events) {
                    Vec2 shock_pos = m_graph.get_hub_node().position;
                    if (ev.related_process.pid != 0) {
                        const auto* node = m_graph.get_node_by_identity(ev.related_process);
                        if (node) {
                            shock_pos = node->position;
                        }
                    }

                    Vec4 shock_col = SystemEvent::severity_to_color(ev.severity);
                    float radius = (ev.severity == EventSeverity::Critical) ? 380.0f :
                                   (ev.severity == EventSeverity::Warning)  ? 280.0f : 200.0f;
                    float lifetime = (ev.severity == EventSeverity::Critical) ? 1.6f : 1.3f;

                    m_shockwaves.trigger(shock_pos, shock_col, radius, lifetime,
                                         ev.badge_text, ev.title, ev.severity, ev.type,
                                         ev.measured_value, ev.unit, ev.id);
                }
            }
        }

        // 2. Update Simulations
        m_graph.update_simulation(delta_time);
        m_camera.update(delta_time);
        m_particles.update(delta_time, m_camera.get_position(), m_camera.get_velocity());
        m_shockwaves.update(delta_time);

        // 3. Render World
        m_renderer.begin_frame(m_camera, m_total_time);
        m_renderer.render_particles(m_particles, m_camera);
        m_renderer.render_graph(m_graph, m_camera, m_total_time);
        m_renderer.render_shockwaves(m_shockwaves, m_camera);
        m_renderer.end_frame();

        // 4. Render UI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        m_ui_mgr.render(m_graph, m_camera, m_history, m_event_detector,
                        *m_collector, m_renderer, m_shockwaves, delta_time);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);
    }
}

} // namespace nerv
