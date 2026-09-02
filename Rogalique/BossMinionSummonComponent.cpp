#include "BossMinionSummonComponent.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "NavGrid.h"
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

    // Точка должна быть проходима — на арене есть колонны-укрытия и ящик со стрелами (оба кинематические
    // коллайдеры, см. ArrowCrate.cpp), точка кольцом вокруг босса без этой проверки могла засадить миньона прямо
    // в стену/ящик. Тот же приём (несколько попыток + isWalkableWorld), что и у
    // EnemyBehaviorComponent::pickNewPatrolPoint/BossTeleportComponent; если 8 попыток не нашли проходимую точку —
    // просто не призываем в этот раз, следующая попытка через summonInterval.
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> angleDist(0.f, TAU);
    NavGrid* nav = GameWorld::instance().getNavGrid();
    sf::Vector2f spawnPosition;
    bool found = false;
    for (int attempt = 0; attempt < 8; ++attempt) {
        float angle = angleDist(rng);
        sf::Vector2f candidate
            = getOwner()->getPosition() + sf::Vector2f(std::cos(angle) * m_spawnRadius, std::sin(angle) * m_spawnRadius);
        if (!nav || nav->isWalkableWorld(candidate)) {
            spawnPosition = candidate;
            found = true;
            break;
        }
    }
    if (!found) {
        return;
    }

    GameObject* minion = m_spawnMinion(spawnPosition);
    if (minion) {
        m_spawned.push_back(minion);
    }
}

void BossMinionSummonComponent::reset()
{
    m_timer = sf::Time::Zero;
    m_spawned.clear();
}
