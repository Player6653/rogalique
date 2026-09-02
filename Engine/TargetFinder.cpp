#include "pch.h"
#include "TargetFinder.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "NavGrid.h"
#include <cmath>

HealthComponent* findBestAttackTarget(GameObject* owner, sf::Vector2f facing, float facingLength, float minRange, float maxRange,
    bool checkCone, const std::function<bool(GameObject*)>& targetFilter, NavGrid* navGridForLineOfSight)
{
    HealthComponent* best = nullptr;
    float bestDistance = 0.f;

    for (HealthComponent* health : GameWorld::instance().getHealthComponents()) {
        GameObject* target = health->getOwner();
        if (!target || target == owner || health->isDead()) {
            continue;
        }
        if (targetFilter && !targetFilter(target)) {
            continue;
        }

        sf::Vector2f delta = target->getPosition() - owner->getPosition();
        float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (distance < minRange || distance > maxRange) {
            continue;
        }

        if (checkCone && facingLength > 0.0001f && distance > 0.0001f) {
            sf::Vector2f facingNormalized = facing / facingLength;
            sf::Vector2f toTargetNormalized = delta / distance;
            float dot = facingNormalized.x * toTargetNormalized.x + facingNormalized.y * toTargetNormalized.y;
            if (dot < TARGET_CONE_DOT_THRESHOLD) {
                continue;
            }
        }

        if (navGridForLineOfSight && !navGridForLineOfSight->hasLineOfSight(owner->getPosition(), target->getPosition())) {
            continue;
        }

        // Из всех подходящих берём ближайшего, а не первого зарегистрированного — иначе при нескольких целях в
        // радиусе+конусе удар/выстрел всегда доставался бы тому, кто раньше появился на сцене.
        if (!best || distance < bestDistance) {
            best = health;
            bestDistance = distance;
        }
    }

    return best;
}
