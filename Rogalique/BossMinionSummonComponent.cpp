#include "BossMinionSummonComponent.h"
#include "GameObject.h"
#include "HealthComponent.h"
#include <cmath>
#include <random>

namespace
{
    constexpr float TAU = 6.2831853f;
}

BossMinionSummonComponent::BossMinionSummonComponent(
    sf::Time summonInterval, int maxAlive, float spawnRadius, std::function<GameObject*(sf::Vector2f)> spawnMinion)
    : m_summonInterval(summonInterval)
    , m_timer(sf::Time::Zero)
    , m_maxAlive(maxAlive)
    , m_spawnRadius(spawnRadius)
    , m_spawnMinion(std::move(spawnMinion))
{
}

int BossMinionSummonComponent::countAlive() const
{
    int alive = 0;
    for (GameObject* minion : m_spawned) {
        HealthComponent* health = minion->getComponent<HealthComponent>();
        if (health && !health->isDead()) {
            ++alive;
        }
    }
    return alive;
}

void BossMinionSummonComponent::update(sf::Time dt)
{
    m_timer += dt;
    if (m_timer < m_summonInterval) {
        return;
    }
    m_timer = sf::Time::Zero;

    if (countAlive() >= m_maxAlive) {
        return;
    }

    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> angleDist(0.f, TAU);
    float angle = angleDist(rng);
    sf::Vector2f offset(std::cos(angle) * m_spawnRadius, std::sin(angle) * m_spawnRadius);

    GameObject* minion = m_spawnMinion(getOwner()->getPosition() + offset);
    if (minion) {
        m_spawned.push_back(minion);
    }
}

void BossMinionSummonComponent::reset()
{
    m_timer = sf::Time::Zero;
    m_spawned.clear();
}
