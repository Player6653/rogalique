#include "KillStreakComponent.h"
#include "HealthComponent.h"

KillStreakComponent::KillStreakComponent(HealthComponent& health, std::function<void()> onKilled)
    : m_health(health)
    , m_onKilled(std::move(onKilled))
{
}

void KillStreakComponent::update(sf::Time dt)
{
    bool isDeadNow = m_health.isDead();
    if (isDeadNow && !m_wasDead && m_onKilled) {
        m_onKilled();
    }
    m_wasDead = isDeadNow;
}

void KillStreakComponent::reset()
{
    m_wasDead = false;
}

void KillStreakComponent::syncToCurrentState()
{
    m_wasDead = m_health.isDead();
}
