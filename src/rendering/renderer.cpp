#include "rendering/renderer.hpp"
#include <vector>
#include <iostream>
#include <chrono>
#include <cstddef>

namespace nerv {

Renderer::Renderer() = default;

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(const std::string& assets_dir) {
    if (m_initialized) return true;

    // Load Shaders
    m_shader_mgr.load_program("background", assets_dir + "/shaders/background.vert",
                              assets_dir + "/shaders/background.frag");
    m_shader_mgr.load_program("node", assets_dir + "/shaders/node.vert",
                              assets_dir + "/shaders/node.frag");
    m_shader_mgr.load_program("edge", assets_dir + "/shaders/edge.vert",
                              assets_dir + "/shaders/edge.frag");
    m_shader_mgr.load_program("shockwave", assets_dir + "/shaders/shockwave.vert",
                              assets_dir + "/shaders/shockwave.frag");
    m_shader_mgr.load_program("particles", assets_dir + "/shaders/particles.vert",
                              assets_dir + "/shaders/particles.frag");
    m_shader_mgr.load_program("bloom_extract", assets_dir + "/shaders/background.vert",
                              assets_dir + "/shaders/bloom_extract.frag");
    m_shader_mgr.load_program("blur", assets_dir + "/shaders/background.vert",
                              assets_dir + "/shaders/blur.frag");
    m_shader_mgr.load_program("composite", assets_dir + "/shaders/background.vert",
                              assets_dir + "/shaders/composite.frag");

    init_fullscreen_quad();
    init_node_instancing_buffers();
    init_batched_edge_buffers();
    init_particle_buffers();
    init_bloom_fbos(m_viewport_width, m_viewport_height);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);

    m_initialized = true;
    return true;
}

void Renderer::shutdown() {
    if (!m_initialized) return;

    destroy_bloom_fbos();

    if (m_fs_quad_vao) { glDeleteVertexArrays(1, &m_fs_quad_vao); m_fs_quad_vao = 0; }
    if (m_fs_quad_vbo) { glDeleteBuffers(1, &m_fs_quad_vbo); m_fs_quad_vbo = 0; }

    if (m_node_vao) { glDeleteVertexArrays(1, &m_node_vao); m_node_vao = 0; }
    if (m_node_quad_vbo) { glDeleteBuffers(1, &m_node_quad_vbo); m_node_quad_vbo = 0; }
    if (m_node_instance_vbo) { glDeleteBuffers(1, &m_node_instance_vbo); m_node_instance_vbo = 0; }

    if (m_edge_vao) { glDeleteVertexArrays(1, &m_edge_vao); m_edge_vao = 0; }
    if (m_edge_vbo) { glDeleteBuffers(1, &m_edge_vbo); m_edge_vbo = 0; }

    if (m_shockwave_vao) { glDeleteVertexArrays(1, &m_shockwave_vao); m_shockwave_vao = 0; }
    if (m_shockwave_vbo) { glDeleteBuffers(1, &m_shockwave_vbo); m_shockwave_vbo = 0; }

    if (m_particle_vao) { glDeleteVertexArrays(1, &m_particle_vao); m_particle_vao = 0; }
    if (m_particle_vbo) { glDeleteBuffers(1, &m_particle_vbo); m_particle_vbo = 0; }

    m_initialized = false;
}

void Renderer::init_fullscreen_quad() {
    float fs_quad[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_fs_quad_vao);
    glGenBuffers(1, &m_fs_quad_vbo);

    glBindVertexArray(m_fs_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_fs_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fs_quad), fs_quad, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    glBindVertexArray(0);
}

void Renderer::init_node_instancing_buffers() {
    // 1. Quad corner geometry [-1, 1]
    float quad_verts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_node_vao);
    glGenBuffers(1, &m_node_quad_vbo);
    glGenBuffers(1, &m_node_instance_vbo);

    glBindVertexArray(m_node_vao);

    // Quad geometry on location 0
    glBindBuffer(GL_ARRAY_BUFFER, m_node_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // Instanced data on locations 1 to 8
    glBindBuffer(GL_ARRAY_BUFFER, m_node_instance_vbo);
    glBufferData(GL_ARRAY_BUFFER, MaxNodeInstances * sizeof(NodeInstanceData), nullptr, GL_DYNAMIC_DRAW);

    size_t stride = sizeof(NodeInstanceData);

    // loc 1: pos (vec2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(NodeInstanceData, pos)));
    glVertexAttribDivisor(1, 1);

    // loc 2: radius (float)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(NodeInstanceData, radius)));
    glVertexAttribDivisor(2, 1);

    // loc 3: glow (float)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(NodeInstanceData, glow)));
    glVertexAttribDivisor(3, 1);

    // loc 4: color (vec4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(NodeInstanceData, color)));
    glVertexAttribDivisor(4, 1);

    // loc 5: alpha (float)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(NodeInstanceData, alpha)));
    glVertexAttribDivisor(5, 1);

    // loc 6: activity (float)
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(NodeInstanceData, activity)));
    glVertexAttribDivisor(6, 1);

    // loc 7: flags (vec2)
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(NodeInstanceData, flags)));
    glVertexAttribDivisor(7, 1);

    // loc 8: cpu (float)
    glEnableVertexAttribArray(8);
    glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(NodeInstanceData, cpu)));
    glVertexAttribDivisor(8, 1);

    glBindVertexArray(0);

    // Shockwave buffers
    glGenVertexArrays(1, &m_shockwave_vao);
    glGenBuffers(1, &m_shockwave_vbo);
    glBindVertexArray(m_shockwave_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_shockwave_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

void Renderer::init_batched_edge_buffers() {
    glGenVertexArrays(1, &m_edge_vao);
    glGenBuffers(1, &m_edge_vbo);

    glBindVertexArray(m_edge_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_edge_vbo);
    glBufferData(GL_ARRAY_BUFFER, MaxEdgeVertices * sizeof(EdgeVertex), nullptr, GL_DYNAMIC_DRAW);

    size_t stride = sizeof(EdgeVertex);

    // loc 0: pos (vec2)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(EdgeVertex, pos)));

    // loc 1: param_t (float)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(EdgeVertex, param_t)));

    // loc 2: param_s (float)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(EdgeVertex, param_s)));

    // loc 3: color (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(EdgeVertex, color)));

    // loc 4: pulse_progress (float)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(EdgeVertex, pulse_progress)));

    // loc 5: pulse_speed (float)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(EdgeVertex, pulse_speed)));

    // loc 6: alpha (float)
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(stride),
                          reinterpret_cast<void*>(offsetof(EdgeVertex, alpha)));

    glBindVertexArray(0);
}

void Renderer::init_particle_buffers() {
    glGenVertexArrays(1, &m_particle_vao);
    glGenBuffers(1, &m_particle_vbo);

    glBindVertexArray(m_particle_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_particle_vbo);
    glBufferData(GL_ARRAY_BUFFER, MaxParticles * 5 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // loc 0: Position (vec2)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);

    // loc 1: Size (float)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    // loc 2: Alpha (float)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));

    // loc 3: Depth (float)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(4 * sizeof(float)));

    glBindVertexArray(0);
}

void Renderer::init_bloom_fbos(int width, int height) {
    destroy_bloom_fbos();

    // 1. Scene HDR Framebuffer
    glGenFramebuffers(1, &m_scene_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_scene_fbo);

    glGenTextures(1, &m_scene_color_tex);
    glBindTexture(GL_TEXTURE_2D, m_scene_color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_scene_color_tex, 0);

    glGenRenderbuffers(1, &m_scene_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_scene_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_scene_rbo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 2. Ping-Pong Blur Framebuffers (Half-Resolution)
    m_bloom_width = std::max(1, width / 2);
    m_bloom_height = std::max(1, height / 2);

    glGenFramebuffers(2, m_ping_pong_fbo);
    glGenTextures(2, m_ping_pong_tex);

    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_ping_pong_fbo[i]);
        glBindTexture(GL_TEXTURE_2D, m_ping_pong_tex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_bloom_width, m_bloom_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ping_pong_tex[i], 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::destroy_bloom_fbos() {
    if (m_scene_fbo) { glDeleteFramebuffers(1, &m_scene_fbo); m_scene_fbo = 0; }
    if (m_scene_color_tex) { glDeleteTextures(1, &m_scene_color_tex); m_scene_color_tex = 0; }
    if (m_scene_rbo) { glDeleteRenderbuffers(1, &m_scene_rbo); m_scene_rbo = 0; }

    if (m_ping_pong_fbo[0]) { glDeleteFramebuffers(2, m_ping_pong_fbo); m_ping_pong_fbo[0] = 0; m_ping_pong_fbo[1] = 0; }
    if (m_ping_pong_tex[0]) { glDeleteTextures(2, m_ping_pong_tex); m_ping_pong_tex[0] = 0; m_ping_pong_tex[1] = 0; }
}

void Renderer::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    m_viewport_width = width;
    m_viewport_height = height;
    init_bloom_fbos(width, height);
}

void Renderer::begin_frame(const Camera& camera, float total_time) {
    m_diagnostics.total_draw_calls = 0;
    m_diagnostics.total_vertices = 0;

    if (m_bloom_enabled && m_scene_fbo != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_scene_fbo);
        glViewport(0, 0, m_viewport_width, m_viewport_height);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, m_viewport_width, m_viewport_height);
    }

    glClearColor(0.020f, 0.027f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render Deep Atmospheric Background
    render_background(camera, total_time);
}

void Renderer::render_background(const Camera& camera, float total_time) {
    auto* bg_shader = m_shader_mgr.get_program("background");
    if (!bg_shader) return;

    bg_shader->bind();
    bg_shader->set_vec2("u_Resolution", Vec2{static_cast<float>(m_viewport_width), static_cast<float>(m_viewport_height)});
    bg_shader->set_vec2("u_CameraPos", camera.get_position());
    bg_shader->set_float("u_CameraZoom", camera.get_zoom());
    bg_shader->set_float("u_Time", total_time);

    glBindVertexArray(m_fs_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    m_diagnostics.total_draw_calls++;
    m_diagnostics.total_vertices += 6;
}

void Renderer::render_particles(const ParticleSystem& particles, const Camera& camera) {
    const auto& p_list = particles.get_particles();
    if (p_list.empty()) return;

    auto* shader = m_shader_mgr.get_program("particles");
    if (!shader) return;

    std::vector<float> p_data;
    p_data.reserve(p_list.size() * 5);

    float zoom = camera.get_zoom();

    for (const auto& p : p_list) {
        p_data.push_back(p.position.x);
        p_data.push_back(p.position.y);
        p_data.push_back(p.size * std::max(0.6f, std::sqrt(zoom)));
        p_data.push_back(p.alpha);
        p_data.push_back(p.depth_layer);
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
    m_diagnostics.total_draw_calls++;
    m_diagnostics.total_vertices += p_list.size();
}

void Renderer::render_shockwaves(const ShockwaveSystem& shockwaves, const Camera& camera) {
    const auto& sw_list = shockwaves.get_shockwaves();
    if (sw_list.empty()) return;

    auto* shader = m_shader_mgr.get_program("shockwave");
    if (!shader) return;

    shader->bind();
    shader->set_mat4("u_MVP", camera.get_view_projection());

    glBindVertexArray(m_shockwave_vao);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for shockwaves

    for (const auto& sw : sw_list) {
        shader->set_vec2("u_Center", sw.center);
        shader->set_float("u_Radius", sw.current_radius);
        shader->set_vec4("u_Color", sw.color);
        shader->set_float("u_Progress", sw.get_progress());

        glDrawArrays(GL_TRIANGLES, 0, 6);
        m_diagnostics.total_draw_calls++;
        m_diagnostics.total_vertices += 6;
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(0);

    m_diagnostics.active_shockwaves = sw_list.size();
}

void Renderer::render_graph(const ProcessGraph& graph, const Camera& camera, float total_time) {
    auto start_prep = std::chrono::steady_clock::now();

    Mat4 mvp = camera.get_view_projection();
    float view_w = camera.get_viewport_width();
    float view_h = camera.get_viewport_height();
    Vec2 cam_pos = camera.get_position();
    float zoom = camera.get_zoom();

    float half_w = (view_w * 0.5f) / zoom;
    float half_h = (view_h * 0.5f) / zoom;

    float min_x = cam_pos.x - half_w;
    float max_x = cam_pos.x + half_w;
    float min_y = cam_pos.y - half_h;
    float max_y = cam_pos.y + half_h;

    // =========================================================================
    // 1. GPU BATCHED CONNECTIONS (Single Draw Call Ribbon Geometry!)
    // =========================================================================
    auto* edge_shader = m_shader_mgr.get_program("edge");
    if (edge_shader) {
        edge_shader->bind();
        edge_shader->set_mat4("u_MVP", mvp);
        edge_shader->set_float("u_Time", total_time);

        const auto& edges = graph.get_edges();
        std::vector<EdgeVertex> edge_verts;
        edge_verts.reserve(edges.size() * 6);

        size_t rendered_edges_count = 0;

        for (const auto& edge : edges) {
            // Viewport Frustum Culling
            bool in_view = (edge.source_pos.x >= min_x - 60.0f && edge.source_pos.x <= max_x + 60.0f &&
                            edge.source_pos.y >= min_y - 60.0f && edge.source_pos.y <= max_y + 60.0f) ||
                           (edge.target_pos.x >= min_x - 60.0f && edge.target_pos.x <= max_x + 60.0f &&
                            edge.target_pos.y >= min_y - 60.0f && edge.target_pos.y <= max_y + 60.0f);
            if (!in_view) continue;

            Vec2 dir = edge.target_pos - edge.source_pos;
            float len = dir.length();
            if (len < 0.001f) continue;

            Vec2 unit_dir = dir / len;
            Vec2 normal{-unit_dir.y, unit_dir.x};

            // Thickness ribbon width
            float ribbon_w = 3.5f + (edge.pulse_speed * 0.8f);

            Vec2 v0_pos = edge.source_pos + normal * ribbon_w;
            Vec2 v1_pos = edge.source_pos - normal * ribbon_w;
            Vec2 v2_pos = edge.target_pos + normal * ribbon_w;
            Vec2 v3_pos = edge.target_pos - normal * ribbon_w;

            EdgeVertex v0{v0_pos, 0.0f,  1.0f, edge.color, edge.pulse_progress, edge.pulse_speed, edge.alpha};
            EdgeVertex v1{v1_pos, 0.0f, -1.0f, edge.color, edge.pulse_progress, edge.pulse_speed, edge.alpha};
            EdgeVertex v2{v2_pos, 1.0f,  1.0f, edge.color, edge.pulse_progress, edge.pulse_speed, edge.alpha};
            EdgeVertex v3{v3_pos, 1.0f, -1.0f, edge.color, edge.pulse_progress, edge.pulse_speed, edge.alpha};

            // Triangle 1: (v0, v1, v2)
            edge_verts.push_back(v0);
            edge_verts.push_back(v1);
            edge_verts.push_back(v2);

            // Triangle 2: (v2, v1, v3)
            edge_verts.push_back(v2);
            edge_verts.push_back(v1);
            edge_verts.push_back(v3);

            rendered_edges_count++;
            if (edge_verts.size() >= MaxEdgeVertices - 6) break;
        }

        if (!edge_verts.empty()) {
            glBindVertexArray(m_edge_vao);
            glBindBuffer(GL_ARRAY_BUFFER, m_edge_vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, edge_verts.size() * sizeof(EdgeVertex), edge_verts.data());
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(edge_verts.size()));
            glBindVertexArray(0);

            m_diagnostics.edge_draw_calls = 1;
            m_diagnostics.total_draw_calls += 1;
            m_diagnostics.total_vertices += edge_verts.size();
        } else {
            m_diagnostics.edge_draw_calls = 0;
        }

        m_diagnostics.rendered_edges = rendered_edges_count;
    }

    // =========================================================================
    // 2. GPU INSTANCED NODES (Single Instanced Draw Call for all nodes!)
    // =========================================================================
    auto* node_shader = m_shader_mgr.get_program("node");
    if (node_shader) {
        node_shader->bind();
        node_shader->set_mat4("u_MVP", mvp);
        node_shader->set_float("u_Time", total_time);

        std::vector<NodeInstanceData> instances;
        instances.reserve(graph.get_nodes().size() + 1);

        // System Hub Instance
        const auto& hub = graph.get_hub_node();
        NodeInstanceData hub_inst;
        hub_inst.pos = hub.position;
        hub_inst.radius = hub.radius;
        hub_inst.glow = hub.glow_intensity;
        hub_inst.color = hub.base_color;
        hub_inst.alpha = hub.alpha;
        hub_inst.activity = hub.metrics.cpu_percent / 50.0f;
        hub_inst.flags = {hub.is_selected ? 1.0f : 0.0f, hub.is_hovered ? 1.0f : 0.0f};
        hub_inst.cpu = hub.metrics.cpu_percent;
        instances.push_back(hub_inst);

        // Process Instances
        const auto& nodes = graph.get_nodes();
        for (const auto& [id, node] : nodes) {
            if (node.is_filtered_out) continue;

            // Frustum Culling
            float r_margin = node.radius * 3.0f;
            if (node.position.x + r_margin < min_x || node.position.x - r_margin > max_x ||
                node.position.y + r_margin < min_y || node.position.y - r_margin > max_y) {
                continue;
            }

            NodeInstanceData inst;
            inst.pos = node.position;
            inst.radius = node.radius;
            inst.glow = node.glow_intensity;
            inst.color = node.base_color;
            inst.alpha = node.alpha;
            inst.activity = node.metrics.cpu_percent / 40.0f;
            inst.flags = {node.is_selected ? 1.0f : 0.0f, node.is_hovered ? 1.0f : 0.0f};
            inst.cpu = node.metrics.cpu_percent;

            instances.push_back(inst);
            if (instances.size() >= MaxNodeInstances) break;
        }

        if (!instances.empty()) {
            glBindVertexArray(m_node_vao);
            glBindBuffer(GL_ARRAY_BUFFER, m_node_instance_vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, instances.size() * sizeof(NodeInstanceData), instances.data());

            // 1 Single Instanced Draw Call!
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(instances.size()));
            glBindVertexArray(0);

            m_diagnostics.node_draw_calls = 1;
            m_diagnostics.total_draw_calls += 1;
            m_diagnostics.total_vertices += instances.size() * 6;
        } else {
            m_diagnostics.node_draw_calls = 0;
        }

        m_diagnostics.visible_nodes = instances.size();
    }

    auto end_prep = std::chrono::steady_clock::now();
    m_diagnostics.cpu_prep_time_ms =
        std::chrono::duration<double, std::milli>(end_prep - start_prep).count();
}

void Renderer::end_frame() {
    if (m_bloom_enabled && m_scene_fbo != 0) {
        apply_bloom_and_composite();
    }
}

void Renderer::apply_bloom_and_composite() {
    // 1. Bright-pass Extraction from Scene into Ping-Pong FBO 0
    glBindFramebuffer(GL_FRAMEBUFFER, m_ping_pong_fbo[0]);
    glViewport(0, 0, m_bloom_width, m_bloom_height);
    glClear(GL_COLOR_BUFFER_BIT);

    auto* extract_shader = m_shader_mgr.get_program("bloom_extract");
    if (extract_shader) {
        extract_shader->bind();
        extract_shader->set_float("u_Threshold", m_bloom_threshold);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_scene_color_tex);
        extract_shader->set_int("u_SceneTexture", 0);

        glBindVertexArray(m_fs_quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // 2. Separable Gaussian Blur (Horizontal & Vertical ping-pong passes)
    auto* blur_shader = m_shader_mgr.get_program("blur");
    if (blur_shader) {
        blur_shader->bind();
        bool horizontal = true;
        int amount = 6; // 6 blur iterations

        for (int i = 0; i < amount; ++i) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_ping_pong_fbo[horizontal ? 1 : 0]);
            blur_shader->set_int("u_Horizontal", horizontal ? 1 : 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_ping_pong_tex[horizontal ? 0 : 1]);
            blur_shader->set_int("u_Image", 0);

            glDrawArrays(GL_TRIANGLES, 0, 6);
            horizontal = !horizontal;
        }
    }

    // 3. Composite Back to Screen Framebuffer (0)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_viewport_width, m_viewport_height);

    auto* composite_shader = m_shader_mgr.get_program("composite");
    if (composite_shader) {
        composite_shader->bind();
        composite_shader->set_float("u_BloomIntensity", m_bloom_intensity);
        composite_shader->set_int("u_BloomEnabled", m_bloom_enabled ? 1 : 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_scene_color_tex);
        composite_shader->set_int("u_SceneTexture", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_ping_pong_tex[0]);
        composite_shader->set_int("u_BloomTexture", 1);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
}

} // namespace nerv
