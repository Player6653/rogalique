#include "pch.h"
#include "ParticleSystem.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
    constexpr float PI = 3.14159265f;
    // Лёгкое затухание скорости — частицы не летят по идеально прямой до самой смерти, а плавно "оседают" к
    // концу жизни, читается живее, чем равномерный разлёт по прямой.
    constexpr float VELOCITY_DAMPING_PER_SEC = 0.94f;

    float randomInRange(float minValue, float maxValue)
    {
        float fraction = static_cast<float>(std::rand()) / RAND_MAX;
        return minValue + fraction * (maxValue - minValue);
    }
}

ParticleSystem& ParticleSystem::instance()
{
    static ParticleSystem instance;
    return instance;
}

void ParticleSystem::spawnBurst(sf::Vector2f position, int count, sf::Color color, float speedMin, float speedMax,
    float sizeMin, float sizeMax, sf::Time lifetime)
{
    for (int i = 0; i < count; ++i) {
        float angle = randomInRange(0.f, 2.f * PI);
        float speed = randomInRange(speedMin, speedMax);
        sf::Vector2f velocity(std::cos(angle) * speed, std::sin(angle) * speed);
        float radius = randomInRange(sizeMin, sizeMax);

        Particle particle;
        particle.position = position;
        particle.velocity = velocity;
        particle.color = color;
        particle.radius = radius;
        particle.initialRadius = radius;
        particle.lifetime = lifetime;
        particle.remaining = lifetime;
        m_particles.push_back(particle);
    }
}

void ParticleSystem::update(sf::Time dt)
{
    for (Particle& particle : m_particles) {
        particle.remaining -= dt;
        particle.position += particle.velocity * dt.asSeconds();
        particle.velocity *= std::pow(VELOCITY_DAMPING_PER_SEC, dt.asSeconds());
        // Уменьшается вместе с угасанием — к концу жизни "схлопывается", а не гаснет резко на полном размере.
        float fraction = std::max(0.f, particle.remaining.asSeconds() / particle.lifetime.asSeconds());
        particle.radius = particle.initialRadius * fraction;
    }
    m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
                           [](const Particle& p) { return p.remaining <= sf::Time::Zero; }),
        m_particles.end());
}

void ParticleSystem::draw(sf::RenderWindow& window) const
{
    for (const Particle& particle : m_particles) {
        float fraction = std::max(0.f, std::min(1.f, particle.remaining.asSeconds() / particle.lifetime.asSeconds()));
        sf::CircleShape shape(particle.radius);
        sf::Color color = particle.color;
        color.a = static_cast<sf::Uint8>(255.f * fraction);
        shape.setFillColor(color);
        shape.setOrigin(particle.radius, particle.radius);
        shape.setPosition(particle.position);
        window.draw(shape);
    }
}
