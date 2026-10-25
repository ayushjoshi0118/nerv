#pragma once

#include "rendering/math_types.hpp"
#include <algorithm>

namespace nerv {

class Camera {
public:
    Camera(float viewport_width = 1280.0f, float viewport_height = 720.0f);

    void set_viewport_size(float width, float height);
    void update(float delta_time);

    // Coordinate conversions
    Vec2 screen_to_world(const Vec2& screen_pos) const;
    Vec2 world_to_screen(const Vec2& world_pos) const;

    // Camera navigation
    void pan(const Vec2& delta_screen_pixels);
    void zoom_at(const Vec2& screen_pivot, float zoom_factor);
    void reset_view();
    void focus_on(const Vec2& world_pos, float target_zoom = 1.0f);

    // Getters
    Vec2 get_position() const { return m_position; }
    float get_zoom() const { return m_zoom; }
    float get_viewport_width() const { return m_viewport_width; }
    float get_viewport_height() const { return m_viewport_height; }

    Mat4 get_view_projection() const;

private:
    float m_viewport_width{1280.0f};
    float m_viewport_height{720.0f};

    Vec2 m_position{0.0f, 0.0f};
    Vec2 m_target_position{0.0f, 0.0f};

    float m_zoom{1.0f};
    float m_target_zoom{1.0f};

    static constexpr float MinZoom = 0.15f;
    static constexpr float MaxZoom = 6.0f;
};

} // namespace nerv
