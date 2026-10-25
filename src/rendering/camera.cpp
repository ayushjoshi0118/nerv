#include "rendering/camera.hpp"
#include <cmath>

namespace nerv {

Camera::Camera(float viewport_width, float viewport_height)
    : m_viewport_width(viewport_width), m_viewport_height(viewport_height) {
    m_zoom_pivot_screen = {viewport_width * 0.5f, viewport_height * 0.5f};
}

void Camera::set_viewport_size(float width, float height) {
    if (width > 0.0f) m_viewport_width = width;
    if (height > 0.0f) m_viewport_height = height;
}

void Camera::update(float delta_time) {
    float dt = std::clamp(delta_time, 0.001f, 0.05f);

    // 1. Smooth exponential zoom with pivot preservation
    if (std::abs(m_target_zoom - m_zoom) > 0.0001f) {
        Vec2 pivot_before = screen_to_world(m_zoom_pivot_screen);

        float zoom_blend = 1.0f - std::exp(-12.0f * dt);
        m_zoom += (m_target_zoom - m_zoom) * zoom_blend;

        Vec2 pivot_after = screen_to_world(m_zoom_pivot_screen);
        m_position += (pivot_before - pivot_after);
        m_target_position = m_position;
    }

    // 2. Focus or Inertial pan
    if (m_is_focusing) {
        float pos_blend = 1.0f - std::exp(-10.0f * dt);
        m_position += (m_target_position - m_position) * pos_blend;
        if (m_position.distance(m_target_position) < 0.5f) {
            m_position = m_target_position;
            m_is_focusing = false;
        }
    } else {
        // Apply inertia
        m_position += m_velocity * dt;
        m_velocity *= std::pow(0.04f, dt);
        m_target_position = m_position;
    }
}

Vec2 Camera::screen_to_world(const Vec2& screen_pos) const {
    float centered_x = screen_pos.x - (m_viewport_width * 0.5f);
    float centered_y = (m_viewport_height * 0.5f) - screen_pos.y;

    return {
        m_position.x + (centered_x / m_zoom),
        m_position.y + (centered_y / m_zoom)
    };
}

Vec2 Camera::world_to_screen(const Vec2& world_pos) const {
    float diff_x = (world_pos.x - m_position.x) * m_zoom;
    float diff_y = (world_pos.y - m_position.y) * m_zoom;

    return {
        (m_viewport_width * 0.5f) + diff_x,
        (m_viewport_height * 0.5f) - diff_y
    };
}

void Camera::pan(const Vec2& delta_screen_pixels) {
    Vec2 world_delta{-delta_screen_pixels.x / m_zoom, delta_screen_pixels.y / m_zoom};
    m_position += world_delta;
    m_target_position = m_position;
    m_is_focusing = false;
}

void Camera::add_pan_impulse(const Vec2& delta_screen_pixels) {
    Vec2 world_delta{-delta_screen_pixels.x / m_zoom, delta_screen_pixels.y / m_zoom};
    m_velocity = world_delta * 40.0f; // Velocity in world units/sec
}

void Camera::zoom_at(const Vec2& screen_pivot, float zoom_factor) {
    m_zoom_pivot_screen = screen_pivot;
    m_target_zoom = std::clamp(m_target_zoom * zoom_factor, MinZoom, MaxZoom);
    m_is_focusing = false;
}

void Camera::reset_view() {
    m_target_position = {0.0f, 0.0f};
    m_target_zoom = 1.0f;
    m_zoom_pivot_screen = {m_viewport_width * 0.5f, m_viewport_height * 0.5f};
    m_velocity = {0.0f, 0.0f};
    m_is_focusing = true;
}

void Camera::focus_on(const Vec2& world_pos, float target_zoom) {
    m_target_position = world_pos;
    m_target_zoom = std::clamp(target_zoom, MinZoom, MaxZoom);
    m_zoom_pivot_screen = {m_viewport_width * 0.5f, m_viewport_height * 0.5f};
    m_velocity = {0.0f, 0.0f};
    m_is_focusing = true;
}

Mat4 Camera::get_view_projection() const {
    float half_w = (m_viewport_width * 0.5f) / m_zoom;
    float half_h = (m_viewport_height * 0.5f) / m_zoom;

    float left = m_position.x - half_w;
    float right = m_position.x + half_w;
    float bottom = m_position.y - half_h;
    float top = m_position.y + half_h;

    return Mat4::orthographic(left, right, bottom, top, -1.0f, 1.0f);
}

} // namespace nerv
