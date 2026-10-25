#include "rendering/particle_system.hpp"
#include <random>
#include <cmath>

namespace nerv {

static std::mt19937 g_rng(12345);

ParticleSystem::ParticleSystem(size_t count) {
    set_count(count);
}

void ParticleSystem::init_particle(AmbientParticle& p, const Vec2& center) {
    std::uniform_real_distribution<float> dist_pos(-1200.0f, 1200.0f);
    std::uniform_real_distribution<float> dist_vel(-8.0f, 8.0f);
    std::uniform_real_distribution<float> dist_phase(0.0f, 6.2831853f);
    std::uniform_int_distribution<int> dist_tier(0, 2);

    int tier = dist_tier(g_rng);
    if (tier == 0) {
        // Deep background layer: small, faint, slow
        p.depth_layer = 0.35f;
        p.size = 1.6f;
        p.velocity = Vec2{dist_vel(g_rng) * 0.4f, dist_vel(g_rng) * 0.4f};
        p.alpha = 0.18f;
    } else if (tier == 1) {
        // Midground layer: standard motes
        p.depth_layer = 0.75f;
        p.size = 2.6f;
        p.velocity = Vec2{dist_vel(g_rng) * 0.8f, dist_vel(g_rng) * 0.8f};
        p.alpha = 0.32f;
    } else {
        // Foreground layer: larger glowing bokeh dust
        p.depth_layer = 1.35f;
        p.size = 4.8f;
        p.velocity = Vec2{dist_vel(g_rng) * 1.2f, dist_vel(g_rng) * 1.2f};
        p.alpha = 0.45f;
    }

    p.position = center + Vec2{dist_pos(g_rng), dist_pos(g_rng)};
    p.pulse_phase = dist_phase(g_rng);
}

void ParticleSystem::set_count(size_t count) {
    size_t old_size = m_particles.size();
    m_particles.resize(count);
    Vec2 center{0.0f, 0.0f};
    for (size_t i = old_size; i < count; ++i) {
        init_particle(m_particles[i], center);
    }
}

void ParticleSystem::update(float delta_time, const Vec2& center_pos, const Vec2& camera_velocity) {
    for (auto& p : m_particles) {
        // Gentle organic drift + parallax motion from camera velocity
        Vec2 effective_vel = p.velocity - camera_velocity * (p.depth_layer * 0.15f);
        p.position += effective_vel * delta_time;

        // Subtle alpha breathing
        p.pulse_phase += delta_time * (1.2f * p.depth_layer);
        float base_a = (p.depth_layer > 1.0f) ? 0.35f : 0.22f;
        p.alpha = base_a + 0.15f * std::sin(p.pulse_phase);

        // Respawn if drifted too far from viewport center
        if (p.position.distance(center_pos) > 1600.0f) {
            init_particle(p, center_pos);
        }
    }
}

} // namespace nerv
