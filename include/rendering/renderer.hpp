#pragma once

#include "rendering/camera.hpp"
#include "rendering/shader_manager.hpp"
#include "rendering/particle_system.hpp"
#include "rendering/shockwave_system.hpp"
#include "model/process_graph.hpp"
#include <glad/glad.h>
#include <memory>
#include <string>

namespace nerv {

struct RenderDiagnostics {
    float fps{60.0f};
    float frame_time_ms{16.6f};
    size_t visible_nodes{0};
    size_t rendered_edges{0};
    size_t active_shockwaves{0};
    size_t active_particles{0};
    double cpu_prep_time_ms{0.0};
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool initialize(const std::string& assets_dir = "assets");
    void shutdown();

    void resize(int width, int height);

    void begin_frame();
    void render_graph(const ProcessGraph& graph, const Camera& camera, float total_time);
    void render_particles(const ParticleSystem& particles, const Camera& camera);
    void render_shockwaves(const ShockwaveSystem& shockwaves, const Camera& camera);
    void end_frame();

    const RenderDiagnostics& get_diagnostics() const { return m_diagnostics; }

    void set_bloom_enabled(bool enabled) { m_bloom_enabled = enabled; }
    bool is_bloom_enabled() const { return m_bloom_enabled; }

    void set_reduced_motion(bool enabled) { m_reduced_motion = enabled; }
    bool is_reduced_motion() const { return m_reduced_motion; }

private:
    void init_quad_buffers();
    void init_line_buffers();
    void init_particle_buffers();
    void render_background_grid(const Camera& camera);

    ShaderManager m_shader_mgr;
    bool m_initialized{false};
    bool m_bloom_enabled{true};
    bool m_reduced_motion{false};

    int m_viewport_width{1280};
    int m_viewport_height{720};

    // Quad geometry for nodes and shockwaves
    GLuint m_quad_vao{0};
    GLuint m_quad_vbo{0};

    // Dynamic line buffers for edges
    GLuint m_line_vao{0};
    GLuint m_line_vbo{0};
    static constexpr size_t MaxLineVertices = 16384;

    // Dynamic particle buffers
    GLuint m_particle_vao{0};
    GLuint m_particle_vbo{0};

    RenderDiagnostics m_diagnostics;
};

} // namespace nerv
