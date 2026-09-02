#include "SoldierAmmoComponent.h"
#include "ArrowCrateComponent.h"
#include "ChaseComponent.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "Log.h"
#include "RangedAttackComponent.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    // Насколько далеко Soldier готов идти за ящиком, вместо того чтобы сразу переходить в ближний бой.
    constexpr float ARROW_SEARCH_RADIUS = 500.f;
    // Больше суммы половин коллайдеров Soldier(32) и ArrowCrate(40) = 36 — у ящика теперь есть коллизия
    // (см. ArrowCrate.cpp), вплотную ближе этой суммы физически не подойти, дистанция подбора должна её превышать.
    constexpr float ARROW_PICKUP_DISTANCE = 42.f;
    // Совпадает с тем, на какую дистанцию сближается Orc (ENEMY_CHASE_STOP_DISTANCE) — стоит вплотную в зоне удара.
    constexpr float MELEE_APPROACH_DISTANCE = 30.f;

    // Ближайший ArrowCrate в радиусе, если есть; nullptr иначе (станция бесконечная — "пустых" больше не бывает).
    ArrowCrateComponent* findNearestCrate(GameObject* owner, float searchRadius)
    {
        ArrowCrateComponent* nearest = nullptr;
        float nearestDistance = 0.f;
        for (ArrowCrateComponent* crate : ArrowCrateComponent::getAll()) {
            GameObject* crateOwner = crate->getOwner();
            if (!crateOwner) {
                continue;
            }
            sf::Vector2f delta = crateOwner->getPosition() - owner->getPosition();
            float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (distance > searchRadius) {
                continue;
            }
            if (!nearest || distance < nearestDistance) {
                nearest = crate;
                nearestDistance = distance;
            }
        }
        return nearest;
    }
} // namespace

SoldierAmmoComponent::SoldierAmmoComponent(int maxArrows)
    : m_maxArrows(maxArrows),
      m_arrows(maxArrows)
{
}

void SoldierAmmoComponent::setArrows(int arrows)
{
    m_arrows = std::max(0, std::min(m_maxArrows, arrows));
}

void SoldierAmmoComponent::update(sf::Time dt)
{
    GameObject* owner = getOwner();
    if (!owner) {
        return;
    }

    auto* health = owner->getComponent<HealthComponent>();
    if (health && health->isDead()) {
        return;
    }

    auto* chase = owner->getComponent<ChaseComponent>();
    if (!chase) {
        return;
    }

    if (m_arrows > 0) {
        // Стрелы есть — стреляем сами (RangedAttackComponent теперь пассивен, autoFire=false), обычное поведение
        // ChaseComponent (держаться на дистанции стрельбы) ничем не переопределяем.
        auto* ranged = owner->getComponent<RangedAttackComponent>();
        if (ranged && ranged->tryShoot()) {
            --m_arrows;
            if (m_arrows <= 0) {
                LOG_INFO("Soldier: стрелы кончились");
            }
        }
        return;
    }

    // Стрел нет — ищем ближайший ящик.
    ArrowCrateComponent* crate = findNearestCrate(owner, ARROW_SEARCH_RADIUS);
    if (crate) {
        GameObject* crateOwner = crate->getOwner();
        sf::Vector2f delta = crateOwner->getPosition() - owner->getPosition();
        float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        chase->setSeekOverride(crateOwner->getPosition(), ARROW_PICKUP_DISTANCE);
        if (distance <= ARROW_PICKUP_DISTANCE) {
            m_arrows = m_maxArrows;
            LOG_INFO("Soldier: пополнил стрелы у ящика, снова " + std::to_string(m_arrows) + " стрел");
        }
        return;
    }

    // Ящика в радиусе нет — сближаемся до ближнего боя, обычный меч (AttackComponent) добьёт сам, как только войдём в его радиус.
    const GameObject* target = chase->getTarget();
    if (target) {
        chase->setSeekOverride(target->getPosition(), MELEE_APPROACH_DISTANCE);
    }
}
