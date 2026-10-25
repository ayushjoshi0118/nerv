#include "rendering/renderer.hpp"
#include <vector>
#include <iostream>
#include <chrono>

namespace nerv {

Renderer::Renderer() = default;

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(const std::string& assets_dir) {
    if (m_initialized) return true;

    // Load Shaders with embedded fallbacks in case file loading fails
    std::string vert_node_fallback = R"(
        #version 330 core
        layout (location = 0) in vec2 a_Position;
        uniform mat4 u_MVP;
        uniform vec2 u_NodePos;
        uniform float u_Radius;
        out vec2 v_UV;
        void main() {
            v_UV = a_Position;
            vec2 world_pos = u_NodePos + a_Position * (u_Radius * 2.2);
            gl_Position = u_MVP * vec4(world_pos, 0.0, 1.0);
        }
    )";
    std::string frag_node_fallback = R"(
        #version 330 core
        in vec2 v_UV;
        uniform vec4 u_Color;
        uniform float u_Glow;
        uniform float u_Alpha;
        uniform float u_Time;
        uniform int u_IsSelected;
        uniform int u_IsHovered;
        out vec4 FragColor;
        void main() {
            float dist = length(v_UV);
            if (dist > 1.0) discard;
            float core_radius = 0.45;
            float core_mask = 1.0 - smoothstep(core_radius - 0.05, core_radius, dist);
            float glow_falloff = 0.0;
            if (dist > core_radius) {
                float norm_d = (dist - core_radius) / (1.0 - core_radius);
                glow_falloff = exp(-norm_d * 3.2) * u_Glow;
            }
            float pulse = 0.95 + 0.08 * sin(u_Time * 4.0 + dist * 5.0);
            vec3 bright_core = mix(u_Color.rgb, vec3(1.0, 1.0, 1.0), 0.55 * (1.0 - dist / core_radius));
            vec3 final_rgb = bright_core * core_mask * pulse + u_Color.rgb * glow_falloff;
            if (u_IsSelected == 1) {
                float ring = 1.0 - smoothstep(0.0, 0.08, abs(dist - 0.65));
                final_rgb += vec3(0.365, 0.922, 1.0) * ring * (0.8 + 0.2 * sin(u_Time * 8.0));
            } else if (u_IsHovered == 1) {
                float hover_ring = 1.0 - smoothstep(0.0, 0.05, abs(dist - 0.55));
                final_rgb += vec3(1.0, 1.0, 1.0) * hover_ring * 0.5;
            }
            float final_alpha = (core_mask + glow_falloff * 0.7) * u_Alpha;
            FragColor = vec4(final_rgb, clamp(final_alpha, 0.0, 1.0));
        }
    )";

    std::string vert_edge_fallback = R"(
        #version 330 core
        layout (location = 0) in vec2 a_Position;
        layout (location = 1) in float a_ParamT;
        uniform mat4 u_MVP;
        out float v_ParamT;
        void main() {
            v_ParamT = a_ParamT;
            gl_Position = u_MVP * vec4(a_Position, 0.0, 1.0);
        }
    )";
    std::string frag_edge_fallback = R"(
        #version 330 core
        in float v_ParamT;
        uniform vec4 u_Color;
        uniform float u_Alpha;
        uniform float u_PulseProgress;
        uniform float u_Time;
        out vec4 FragColor;
        void main() {
            float base_alpha = u_Alpha * 0.45;
            float pulse_width = 0.08;
            float dist_to_pulse = abs(v_ParamT - u_PulseProgress);
            float packet = 1.0 - smoothstep(0.0, pulse_width, dist_to_pulse);
            vec3 packet_color = vec3(0.365, 0.922, 1.0);
            vec3 col = mix(u_Color.rgb, packet_color, packet * 0.9);
            float alpha = clamp(base_alpha + packet * 0.85, 0.0, 1.0);
            FragColor = vec4(col, alpha);
        }
    )";

    std::string vert_shockwave_fallback = R"(
        #version 330 core
        layout (location = 0) in vec2 a_Position;
        uniform mat4 u_MVP;
        uniform vec2 u_Center;
        uniform float u_Radius;
        out vec2 v_UV;
        void main() {
            v_UV = a_Position;
            vec2 world_pos = u_Center + a_Position * (u_Radius * 1.25);
            gl_Position = u_MVP * vec4(world_pos, 0.0, 1.0);
        }
    )";
    std::string frag_shockwave_fallback = R"(
        #version 330 core
        in vec2 v_UV;
        uniform vec4 u_Color;
        uniform float u_Progress;
        out vec4 FragColor;
        void main() {
            float dist = length(v_UV);
            if (dist > 1.0 || dist < 0.2) discard;
            float ring_pos = 0.8;
            float ring_width = 0.22 * (1.0 - u_Progress * 0.4);
            float ring = 1.0 - smoothstep(0.0, ring_width, abs(dist - ring_pos));
            float alpha = ring * (1.0 - u_Progress) * 0.85;
            vec3 wave_col = mix(u_Color.rgb, vec3(1.0, 1.0, 1.0), 0.35 * ring);
            FragColor = vec4(wave_col, clamp(alpha, 0.0, 1.0));
        }
    )";

    std::string vert_part_fallback = R"(
        #version 330 core
        layout (location = 0) in vec2 a_Position;
        layout (location = 1) in float a_Size;
        layout (location = 2) in float a_Alpha;
        uniform mat4 u_MVP;
        out float v_Alpha;
        void main() {
            v_Alpha = a_Alpha;
            gl_PointSize = a_Size;
            gl_Position = u_MVP * vec4(a_Position, 0.0, 1.0);
        }
    )";
    std::string frag_part_fallback = R"(
        #version 330 core
        in float v_Alpha;
        uniform vec4 u_Color;
        out vec4 FragColor;
        void main() {
            vec2 coord = gl_PointCoord - vec2(0.5);
            float dist = length(coord);
            if (dist > 0.5) discard;
            float soft_circle = 1.0 - smoothstep(0.2, 0.5, dist);
            FragColor = vec4(u_Color.rgb, u_Color.a * v_Alpha * soft_circle);
        }
    )";

    m_shader_mgr.load_program("node", assets_dir + "/shaders/node.vert", assets_dir + "/shaders/node.frag",
                              vert_node_fallback, frag_node_fallback);
    m_shader_mgr.load_program("edge", assets_dir + "/shaders/edge.vert", assets_dir + "/shaders/edge.frag",
                              vert_edge_fallback, frag_edge_fallback);
    m_shader_mgr.load_program("shockwave", assets_dir + "/shaders/shockwave.vert", assets_dir + "/shaders/shockwave.frag",
                              vert_shockwave_fallback, frag_shockwave_fallback);
    m_shader_mgr.load_program("particles", assets_dir + "/shaders/particles.vert", assets_dir + "/shaders/particles.frag",
                              vert_part_fallback, frag_part_fallback);

    init_quad_buffers();
    init_line_buffers();
    init_particle_buffers();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);

    m_initialized = true;
    return true;
}

void Renderer::shutdown() {
    if (!m_initialized) return;

    if (m_quad_vao) { glDeleteVertexArrays(1, &m_quad_vao); m_quad_vao = 0; }
    if (m_quad_vbo) { glDeleteBuffers(1, &m_quad_vbo); m_quad_vbo = 0; }
    if (m_line_vao) { glDeleteVertexArrays(1, &m_line_vao); m_line_vao = 0; }
    if (m_line_vbo) { glDeleteBuffers(1, &m_line_vbo); m_line_vbo = 0; }
    if (m_particle_vao) { glDeleteVertexArrays(1, &m_particle_vao); m_particle_vao = 0; }
    if (m_particle_vbo) { glDeleteBuffers(1, &m_particle_vbo); m_particle_vbo = 0; }

    m_initialized = false;
}

void Renderer::init_quad_buffers() {
    // Two triangles covering [-1, 1] quad
    float quad_verts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);

    glBindVertexArray(m_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));

    glBindVertexArray(0);
}

void Renderer::init_line_buffers() {
    glGenVertexArrays(1, &m_line_vao);
    glGenBuffers(1, &m_line_vbo);

    glBindVertexArray(m_line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_line_vbo);
    glBufferData(GL_ARRAY_BUFFER, MaxLineVertices * sizeof(float) * 3, nullptr, GL_DYNAMIC_DRAW);

    // Attribute 0: position (x, y)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));

    // Attribute 1: paramT (0.0 to 1.0)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(0);
}

void Renderer::init_particle_buffers() {
    glGenVertexArrays(1, &m_particle_vao);
    glGenBuffers(1, &m_particle_vbo);

    glBindVertexArray(m_particle_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_particle_vbo);
    glBufferData(GL_ARRAY_BUFFER, 1024 * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Attribute 0: Position (vec2)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));

    // Attribute 1: Size (float)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    // Attribute 2: Alpha (float)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));

    glBindVertexArray(0);
}

void Renderer::resize(int width, int height) {
    m_viewport_width = width;
    m_viewport_height = height;
    glViewport(0, 0, width, height);
}

void Renderer::begin_frame() {
    // Clear to official background black: #05070D
    const auto& bg = colors::BackgroundBlack;
    glClearColor(bg.r, bg.g, bg.b, bg.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::end_frame() {
    // Frame complete
}

void Renderer::render_background_grid(const Camera& camera) {
    // Renders subtle orbital rings around center hub
    auto* shader = m_shader_mgr.get_program("edge");
    if (!shader) return;

    shader->bind();
    Mat4 mvp = camera.get_view_projection();
    shader->set_mat4("u_MVP", mvp);
    shader->set_vec4("u_Color", Vec4{0.043f, 0.071f, 0.125f, 0.4f}); // Deep navy
    shader->set_float("u_Alpha", 0.35f);
    shader->set_float("u_PulseProgress", -1.0f); // No pulse on grid

    std::vector<float> ring_verts;
    const float radii[] = {120.0f, 240.0f, 380.0f, 540.0f};
    const int segments = 64;

    for (float r : radii) {
        for (int i = 0; i <= segments; ++i) {
            float a1 = (i * 6.2831853f) / segments;
            float a2 = ((i + 1) * 6.2831853f) / segments;

            ring_verts.push_back(std::cos(a1) * r);
            ring_verts.push_back(std::sin(a1) * r);
            ring_verts.push_back(0.0f);

            ring_verts.push_back(std::cos(a2) * r);
            ring_verts.push_back(std::sin(a2) * r);
            ring_verts.push_back(1.0f);
        }
    }

    glBindVertexArray(m_line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_line_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, ring_verts.size() * sizeof(float), ring_verts.data());
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(ring_verts.size() / 3));
    glBindVertexArray(0);
}

void Renderer::render_particles(const ParticleSystem& particles, const Camera& camera) {
    const auto& p_list = particles.get_particles();
    if (p_list.empty()) return;

    auto* shader = m_shader_mgr.get_program("particles");
    if (!shader) return;

    std::vector<float> p_data;
    p_data.reserve(p_list.size() * 4);

    for (const auto& p : p_list) {
        p_data.push_back(p.position.x);
        p_data.push_back(p.position.y);
        p_data.push_back(p.size * camera.get_zoom());
        p_data.push_back(p.alpha);
    }

    shader->bind();
    shader->set_mat4("u_MVP", camera.get_view_projection());
    shader->set_vec4("u_Color", colors::ElectricCyan);

    glBindVertexArray(m_particle_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_particle_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, p_data.size() * sizeof(float), p_data.data());
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(p_list.size()));
    glBindVertexArray(0);

    m_diagnostics.active_particles = p_list.size();
}

void Renderer::render_shockwaves(const ShockwaveSystem& shockwaves, const Camera& camera) {
    const auto& sw_list = shockwaves.get_shockwaves();
    if (sw_list.empty()) return;

    auto* shader = m_shader_mgr.get_program("shockwave");
    if (!shader) return;

    shader->bind();
    shader->set_mat4("u_MVP", camera.get_view_projection());

    glBindVertexArray(m_quad_vao);

    // Additive blending for shockwaves
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (const auto& sw : sw_list) {
        shader->set_vec2("u_Center", sw.center);
        shader->set_float("u_Radius", sw.current_radius);
        shader->set_vec4("u_Color", sw.color);
        shader->set_float("u_Progress", sw.get_progress());

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // Restore standard alpha blending
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(0);

    m_diagnostics.active_shockwaves = sw_list.size();
}

void Renderer::render_graph(const ProcessGraph& graph, const Camera& camera, float total_time) {
    auto start_prep = std::chrono::steady_clock::now();

    render_background_grid(camera);

    Mat4 mvp = camera.get_view_projection();
    float view_w = camera.get_viewport_width();
    float view_h = camera.get_viewport_height();
    Vec2 cam_pos = camera.get_position();
    float zoom = camera.get_zoom();

    float half_w = (view_w * 0.5f) / zoom;
    float half_h = (view_h * 0.5f) / zoom;

    // View frustum bounding box
    float min_x = cam_pos.x - half_w;
    float max_x = cam_pos.x + half_w;
    float min_y = cam_pos.y - half_h;
    float max_y = cam_pos.y + half_h;

    // 1. Render Edges
    auto* edge_shader = m_shader_mgr.get_program("edge");
    if (edge_shader) {
        edge_shader->bind();
        edge_shader->set_mat4("u_MVP", mvp);
        edge_shader->set_float("u_Time", total_time);

        const auto& edges = graph.get_edges();
        size_t rendered_edges_count = 0;

        glBindVertexArray(m_line_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_line_vbo);

        for (const auto& edge : edges) {
            // Frustum check line segment
            bool in_view = (edge.source_pos.x >= min_x - 50.0f && edge.source_pos.x <= max_x + 50.0f &&
                            edge.source_pos.y >= min_y - 50.0f && edge.source_pos.y <= max_y + 50.0f) ||
                           (edge.target_pos.x >= min_x - 50.0f && edge.target_pos.x <= max_x + 50.0f &&
                            edge.target_pos.y >= min_y - 50.0f && edge.target_pos.y <= max_y + 50.0f);
            if (!in_view) continue;

            float line_verts[] = {
                edge.source_pos.x, edge.source_pos.y, 0.0f,
                edge.target_pos.x, edge.target_pos.y, 1.0f
            };

            edge_shader->set_vec4("u_Color", edge.color);
            edge_shader->set_float("u_Alpha", edge.alpha);
            edge_shader->set_float("u_PulseProgress", edge.pulse_progress);

            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(line_verts), line_verts);
            glDrawArrays(GL_LINES, 0, 2);
            rendered_edges_count++;
        }

        glBindVertexArray(0);
        m_diagnostics.rendered_edges = rendered_edges_count;
    }

    // 2. Render Nodes
    auto* node_shader = m_shader_mgr.get_program("node");
    if (node_shader) {
        node_shader->bind();
        node_shader->set_mat4("u_MVP", mvp);
        node_shader->set_float("u_Time", total_time);

        glBindVertexArray(m_quad_vao);

        size_t visible_count = 0;

        // Render central System Hub
        const auto& hub = graph.get_hub_node();
        node_shader->set_vec2("u_NodePos", hub.position);
        node_shader->set_float("u_Radius", hub.radius);
        node_shader->set_vec4("u_Color", hub.base_color);
        node_shader->set_float("u_Glow", hub.glow_intensity);
        node_shader->set_float("u_Alpha", hub.alpha);
        node_shader->set_int("u_IsSelected", hub.is_selected ? 1 : 0);
        node_shader->set_int("u_IsHovered", hub.is_hovered ? 1 : 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        visible_count++;

        // Render Process Nodes
        const auto& nodes = graph.get_nodes();
        for (const auto& [id, node] : nodes) {
            if (node.is_filtered_out) continue;

            // Frustum Culling
            float r_margin = node.radius * 2.5f;
            if (node.position.x + r_margin < min_x || node.position.x - r_margin > max_x ||
                node.position.y + r_margin < min_y || node.position.y - r_margin > max_y) {
                continue;
            }

            node_shader->set_vec2("u_NodePos", node.position);
            node_shader->set_float("u_Radius", node.radius);
            node_shader->set_vec4("u_Color", node.base_color);
            node_shader->set_float("u_Glow", node.glow_intensity);
            node_shader->set_float("u_Alpha", node.alpha);
            node_shader->set_int("u_IsSelected", node.is_selected ? 1 : 0);
            node_shader->set_int("u_IsHovered", node.is_hovered ? 1 : 0);

            glDrawArrays(GL_TRIANGLES, 0, 6);
            visible_count++;
        }

        glBindVertexArray(0);
        m_diagnostics.visible_nodes = visible_count;
    }

    auto end_prep = std::chrono::steady_clock::now();
    m_diagnostics.cpu_prep_time_ms =
        std::chrono::duration<double, std::milli>(end_prep - start_prep).count();
}

} // namespace nerv
