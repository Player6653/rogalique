#include "TrapComponent.h"
#include "ChaseTargetComponent.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "Log.h"
#include <cmath>

namespace
{
    // В 2 раза больше вслед за визуалом (см. Trap.cpp VISUAL_SIZE) — чуть меньше видимого спрайта, задевает, только
    // если игрок реально стоит рядом с шипами, не проходит по касательной у самого края.
    constexpr float TRAP_HIT_RADIUS = 40.f;
    constexpr int TRAP_DAMAGE = 1;
    const sf::Time DAMAGE_COOLDOWN = sf::seconds(0.5f);
} // namespace

TrapComponent::TrapComponent(sf::Time cycleDuration, sf::Time dangerousPhaseStart)
    : m_cycleDuration(cycleDuration),
      m_dangerousPhaseStart(dangerousPhaseStart)
{
}

void TrapComponent::update(sf::Time dt)
{
    m_cycleElapsed += dt;
    while (m_cycleElapsed >= m_cycleDuration) {
        m_cycleElapsed -= m_cycleDuration;
    }

    if (m_damageCooldownRemaining > sf::Time::Zero) {
        m_damageCooldownRemaining -= dt;
    }

    bool dangerous = m_cycleElapsed >= m_dangerousPhaseStart;
    if (!dangerous || m_damageCooldownRemaining > sf::Time::Zero) {
        return;
    }

    GameObject* owner = getOwner();
    if (!owner) {
        return;
    }

    GameObject* player = findChaseTarget();
    if (!player) {
        return;
    }

    sf::Vector2f delta = player->getPosition() - owner->getPosition();
    float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (distance > TRAP_HIT_RADIUS) {
        return;
    }

    auto* health = player->getComponent<HealthComponent>();
    if (health && health->takeDamage(TRAP_DAMAGE) > 0) {
        LOG_INFO("Trap: шип задел игрока");
    }
    m_damageCooldownRemaining = DAMAGE_COOLDOWN;
}
