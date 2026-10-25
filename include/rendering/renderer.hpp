#pragma once

#include "rendering/camera.hpp"
#include "rendering/shader_manager.hpp"
#include "rendering/particle_system.hpp"
#include "rendering/shockwave_system.hpp"
#include "model/process_graph.hpp"
#include <glad/glad.h>
#include <memory>
#include <string>
#include <vector>

namespace nerv {

struct NodeInstanceData {
    Vec2 pos;
    float radius{10.0f};
    float glow{0.2f};
    Vec4 color{colors::ElectricCyan};
    float alpha{1.0f};
    float activity{0.0f};
    Vec2 flags{0.0f, 0.0f}; // x: is_selected, y: is_hovered
    float cpu{0.0f};
};

struct EdgeVertex {
    Vec2 pos;
    float param_t{0.0f};
    float param_s{0.0f};
    Vec4 color{colors::NeuralViolet};
    float pulse_progress{0.0f};
    float pulse_speed{1.0f};
    float alpha{0.4f};
};

struct RenderDiagnostics {
    float fps{60.0f};
    float frame_time_ms{16.6f};
    size_t visible_nodes{0};
    size_t rendered_edges{0};
    size_t active_shockwaves{0};
    size_t active_particles{0};
    size_t node_draw_calls{0};
    size_t edge_draw_calls{0};
    size_t total_draw_calls{0};
    size_t total_vertices{0};
    double cpu_prep_time_ms{0.0};
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool initialize(const std::string& assets_dir = "assets");
    void shutdown();

    void resize(int width, int height);

    void begin_frame(const Camera& camera, float total_time);
    void render_graph(const ProcessGraph& graph, const Camera& camera, float total_time);
    void render_particles(const ParticleSystem& particles, const Camera& camera);
    void render_shockwaves(const ShockwaveSystem& shockwaves, const Camera& camera);
    void end_frame();

    const RenderDiagnostics& get_diagnostics() const { return m_diagnostics; }

    void set_bloom_enabled(bool enabled) { m_bloom_enabled = enabled; }
    bool is_bloom_enabled() const { return m_bloom_enabled; }

    void set_bloom_intensity(float intensity) { m_bloom_intensity = intensity; }
    float get_bloom_intensity() const { return m_bloom_intensity; }

    void set_bloom_threshold(float threshold) { m_bloom_threshold = threshold; }
    float get_bloom_threshold() const { return m_bloom_threshold; }

    void set_reduced_motion(bool enabled) { m_reduced_motion = enabled; }
    bool is_reduced_motion() const { return m_reduced_motion; }

private:
    void init_fullscreen_quad();
    void init_node_instancing_buffers();
    void init_batched_edge_buffers();
    void init_particle_buffers();
    void init_bloom_fbos(int width, int height);
    void destroy_bloom_fbos();

    void render_background(const Camera& camera, float total_time);
    void apply_bloom_and_composite();

    ShaderManager m_shader_mgr;
    bool m_initialized{false};
    bool m_bloom_enabled{true};
    float m_bloom_intensity{0.75f};
    float m_bloom_threshold{0.68f};
    bool m_reduced_motion{false};

    int m_viewport_width{1280};
    int m_viewport_height{720};

    // Fullscreen quad for background and post-processing
    GLuint m_fs_quad_vao{0};
    GLuint m_fs_quad_vbo{0};

    // GPU Instanced Node buffers
    GLuint m_node_vao{0};
    GLuint m_node_quad_vbo{0};
    GLuint m_node_instance_vbo{0};
    static constexpr size_t MaxNodeInstances = 8192;

    // GPU Batched Edge ribbon buffers
    GLuint m_edge_vao{0};
    GLuint m_edge_vbo{0};
    static constexpr size_t MaxEdgeVertices = 65536;

    // Shockwave quad geometry
    GLuint m_shockwave_vao{0};
    GLuint m_shockwave_vbo{0};

    // Dynamic particle buffers
    GLuint m_particle_vao{0};
    GLuint m_particle_vbo{0};
    static constexpr size_t MaxParticles = 2048;

    // HDR Scene FBO & Textures
    GLuint m_scene_fbo{0};
    GLuint m_scene_color_tex{0};
    GLuint m_scene_rbo{0};

    // Ping-Pong Bloom Blur FBOs (Half Resolution)
    GLuint m_ping_pong_fbo[2]{0, 0};
    GLuint m_ping_pong_tex[2]{0, 0};
    int m_bloom_width{640};
    int m_bloom_height{360};

    RenderDiagnostics m_diagnostics;
};

} // namespace nerv
