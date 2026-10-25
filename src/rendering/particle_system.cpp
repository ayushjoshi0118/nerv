#include "rendering/particle_system.hpp"
#include <random>
#include <cmath>

namespace nerv {

static std::mt19937 g_rng(12345);

ParticleSystem::ParticleSystem(size_t count) {
    set_count(count);
}

void ParticleSystem::init_particle(AmbientParticle& p, const Vec2& center) {
    std::uniform_real_distribution<float> dist_pos(-900.0f, 900.0f);
    std::uniform_real_distribution<float> dist_vel(-12.0f, 12.0f);
    std::uniform_real_distribution<float> dist_size(1.5f, 4.0f);
    std::uniform_real_distribution<float> dist_phase(0.0f, 6.2831853f);

    p.position = center + Vec2{dist_pos(g_rng), dist_pos(g_rng)};
    p.velocity = {dist_vel(g_rng), dist_vel(g_rng)};
    p.size = dist_size(g_rng);
    p.alpha = 0.3f;
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

void ParticleSystem::update(float delta_time, const Vec2& center_pos) {
    for (auto& p : m_particles) {
        p.position += p.velocity * delta_time;
        p.pulse_phase += delta_time * 1.5f;
        p.alpha = 0.25f + 0.2f * std::sin(p.pulse_phase);

        // Respawn if too far
        if (p.position.distance(center_pos) > 1200.0f) {
            init_particle(p, center_pos);
        }
    }
}

} // namespace nerv
