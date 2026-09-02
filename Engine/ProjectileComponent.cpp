#include "pch.h"
#include "ProjectileComponent.h"
#include "ColliderComponent.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "Log.h"
#include <cassert>
#include <cmath>

ProjectileComponent::ProjectileComponent(
    sf::Vector2f direction, float speed, int damage, float hitRadius, float maxRange, const GameObject* ignoreOwner)
    : m_direction(direction),
      m_speed(speed),
      m_damage(damage),
      m_hitRadius(hitRadius),
      m_maxRange(maxRange),
      m_ignoreOwner(ignoreOwner)
{
    assert(damage >= 0 && "ProjectileComponent: damage must not be negative");
    assert(speed > 0.f && "ProjectileComponent: speed must be positive");
    assert(hitRadius > 0.f && "ProjectileComponent: hitRadius must be positive");
    assert(maxRange > 0.f && "ProjectileComponent: maxRange must be positive");
}

void ProjectileComponent::update(sf::Time dt)
{
    GameObject* owner = getOwner();
    if (!owner) {
        return;
    }

    float step = m_speed * dt.asSeconds();
    sf::Vector2f proposedPosition = owner->getPosition() + m_direction * step;

    // Стены (кинематические коллайдеры) останавливают стрелу насмерть — актёров коллайдеры тут нарочно не смотрим,
    // сквозь них снаряд физически пролетает, попадание по ним решается отдельно ниже через HealthComponent.
    // Пространственная сетка (см. GameWorld::queryKinematicColliders), не перебор всех коллайдеров сцены.
    sf::FloatRect proposedBounds(
        proposedPosition.x - m_hitRadius, proposedPosition.y - m_hitRadius, m_hitRadius * 2.f, m_hitRadius * 2.f);
    for (ColliderComponent* collider : GameWorld::instance().queryKinematicColliders(proposedBounds)) {
        if (proposedBounds.intersects(collider->getBounds())) {
            owner->destroy();
            return;
        }
    }

    owner->move(m_direction * step);
    m_traveled += step;

    for (HealthComponent* health : GameWorld::instance().getHealthComponents()) {
        GameObject* target = health->getOwner();
        if (!target || target == m_ignoreOwner || health->isDead()) {
            continue;
        }

        sf::Vector2f delta = target->getPosition() - owner->getPosition();
        float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (distance > m_hitRadius) {
            continue;
        }

        int appliedDamage = health->takeDamage(m_damage);
        LOG_INFO("Projectile hit for " + std::to_string(appliedDamage) + " damage (of " + std::to_string(m_damage)
                 + " before armor; target hp now " + std::to_string(health->getHp()) + ")");
        owner->destroy();
        return;
    }

    if (m_traveled >= m_maxRange) {
        owner->destroy();
    }
}
