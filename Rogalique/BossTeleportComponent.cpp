#include "BossTeleportComponent.h"
#include "ChaseComponent.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "NavGrid.h"
#include "ParticleSystem.h"
#include <random>

namespace
{
    const sf::Color TELEPORT_COLOR(170, 60, 210);
    constexpr int TELEPORT_PARTICLE_COUNT = 22;
    const sf::Time TELEPORT_PARTICLE_LIFETIME = sf::seconds(0.5f);
}

BossTeleportComponent::BossTeleportComponent(
    HealthComponent& health, float lowHpFraction, sf::Time teleportInterval, sf::FloatRect arenaBounds)
    : m_health(health)
    , m_lowHpFraction(lowHpFraction)
    , m_teleportInterval(teleportInterval)
    , m_arenaBounds(arenaBounds)
{
}

void BossTeleportComponent::update(sf::Time dt)
{
    if (m_health.isDead()) {
        m_timer = sf::Time::Zero;
        return;
    }

    float fraction = static_cast<float>(m_health.getHp()) / static_cast<float>(m_health.getMaxHp());
    if (fraction > m_lowHpFraction) {
        m_timer = sf::Time::Zero;
        return;
    }

    m_timer += dt;
    if (m_timer < m_teleportInterval) {
        return;
    }
    m_timer = sf::Time::Zero;

    GameObject* owner = getOwner();
    if (!owner) {
        return;
    }

    // Точка должна быть проходима — на арене есть колонны-укрытия (стены на слое Walls в Resources/Level/Arena.tmj),
    // телепорт в случайную точку без этой проверки мог засадить босса прямо в коллайдер колонны (баг, найден при
    // аудите ИИ). Тот же приём (несколько попыток + isWalkableWorld), что и у EnemyBehaviorComponent::pickNewPatrolPoint;
    // если 8 попыток не нашли проходимую точку — просто не телепортируемся в этот раз, следующая попытка через
    // teleportInterval.
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> xDist(m_arenaBounds.left, m_arenaBounds.left + m_arenaBounds.width);
    std::uniform_real_distribution<float> yDist(m_arenaBounds.top, m_arenaBounds.top + m_arenaBounds.height);
    NavGrid* nav = GameWorld::instance().getNavGrid();
    sf::Vector2f newPosition;
    bool found = false;
    for (int attempt = 0; attempt < 8; ++attempt) {
        sf::Vector2f candidate(xDist(rng), yDist(rng));
        if (!nav || nav->isWalkableWorld(candidate)) {
            newPosition = candidate;
            found = true;
            break;
        }
    }
    if (!found) {
        return;
    }

    sf::Vector2f oldPosition = owner->getPosition();
    ParticleSystem::instance().spawnBurst(
        oldPosition, TELEPORT_PARTICLE_COUNT, TELEPORT_COLOR, 60.f, 160.f, 3.f, 6.f, TELEPORT_PARTICLE_LIFETIME);

    owner->setPosition(newPosition);
    // Без этого босс какое-то время шёл бы к вейпоинтам пути, посчитанного из точки ДО телепорта — путь
    // пересчитывается по дрейфу цели, а не по скачку позиции самого владельца (баг, найден при аудите ИИ).
    if (auto* chase = owner->getComponent<ChaseComponent>()) {
        chase->reset();
    }
    m_justTeleported = true;

    ParticleSystem::instance().spawnBurst(
        newPosition, TELEPORT_PARTICLE_COUNT, TELEPORT_COLOR, 60.f, 160.f, 3.f, 6.f, TELEPORT_PARTICLE_LIFETIME);
}

void BossTeleportComponent::reset()
{
    m_timer = sf::Time::Zero;
    m_justTeleported = false;
}
