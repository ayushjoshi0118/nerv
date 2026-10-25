#pragma once

#include "rendering/math_types.hpp"
#include <vector>

namespace nerv {

struct ActiveShockwave {
    Vec2 center{0.0f, 0.0f};
    float current_radius{0.0f};
    float max_radius{280.0f};
    float lifetime{1.4f};
    float elapsed{0.0f};
    Vec4 color{colors::ElectricCyan};

    float get_progress() const {
        return (lifetime > 0.0f) ? std::min(1.0f, elapsed / lifetime) : 1.0f;
    }

    bool is_finished() const {
        return elapsed >= lifetime;
    }
};

class ShockwaveSystem {
public:
    ShockwaveSystem() = default;

    void trigger(const Vec2& center, const Vec4& color = colors::ElectricCyan,
                 float max_radius = 280.0f, float lifetime = 1.4f);

    void update(float delta_time);
    void clear() { m_shockwaves.clear(); }

    const std::vector<ActiveShockwave>& get_shockwaves() const { return m_shockwaves; }

private:
    std::vector<ActiveShockwave> m_shockwaves;
};

} // namespace nerv
