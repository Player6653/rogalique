#include "SlimeShotLimitComponent.h"
#include "GameObject.h"
#include "HealthComponent.h"
#include "Log.h"

SlimeShotLimitComponent::SlimeShotLimitComponent(int maxShots)
    : m_maxShots(maxShots)
{
}

void SlimeShotLimitComponent::update(sf::Time)
{
    if (m_shotsFired < m_maxShots) {
        return;
    }
    GameObject* owner = getOwner();
    auto* health = owner ? owner->getComponent<HealthComponent>() : nullptr;
    if (health && !health->isDead()) {
        health->kill();
        LOG_INFO("Slime3: расстреляла весь боезапас (" + std::to_string(m_maxShots) + ") и умирает");
    }
}
