#include "rendering/shockwave_system.hpp"
#include <algorithm>

namespace nerv {

void ShockwaveSystem::trigger(const Vec2& center, const Vec4& color, float max_radius, float lifetime) {
    ActiveShockwave sw;
    sw.center = center;
    sw.current_radius = 0.0f;
    sw.max_radius = max_radius;
    sw.lifetime = lifetime;
    sw.elapsed = 0.0f;
    sw.color = color;

    m_shockwaves.push_back(sw);
}

void ShockwaveSystem::update(float delta_time) {
    for (auto it = m_shockwaves.begin(); it != m_shockwaves.end();) {
        it->elapsed += delta_time;
        float progress = it->get_progress();
        
        // Non-linear ease-out expansion
        float eased = 1.0f - std::pow(1.0f - progress, 3.0f);
        it->current_radius = it->max_radius * eased;

        if (it->is_finished()) {
            it = m_shockwaves.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace nerv
