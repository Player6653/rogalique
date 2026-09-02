#include "DeathParticleComponent.h"
#include "GameObject.h"
#include "HealthComponent.h"
#include "ParticleSystem.h"

namespace
{
    const sf::Color DEATH_PARTICLE_COLOR(180, 40, 40);
    constexpr int DEATH_PARTICLE_COUNT = 18;
}

DeathParticleComponent::DeathParticleComponent(HealthComponent& health)
    : m_health(health)
{
}

void DeathParticleComponent::update(sf::Time dt)
{
    bool isDeadNow = m_health.isDead();
    if (isDeadNow && !m_wasDead) {
        ParticleSystem::instance().spawnBurst(
            getOwner()->getPosition(), DEATH_PARTICLE_COUNT, DEATH_PARTICLE_COLOR, 30.f, 120.f, 2.f, 5.f, sf::seconds(0.5f));
    }
    m_wasDead = isDeadNow;
}

void DeathParticleComponent::reset()
{
    m_wasDead = false;
}
