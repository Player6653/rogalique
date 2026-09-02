#include "pch.h"
#include "RangedAttackComponent.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "IDirectionProvider.h"
#include "Log.h"
#include "NavGrid.h"
#include "Projectile.h"
#include "TargetFinder.h"
#include <cassert>
#include <cmath>
#include <memory>

RangedAttackComponent::RangedAttackComponent(std::string label, int damage, float minRange, float maxRange, sf::Time cooldown,
    float projectileSpeed, float projectileHitRadius, std::string projectileTexturePath, sf::Vector2f projectileVisualSize,
    sf::Time shotDelay, std::function<bool(GameObject*)> targetFilter, bool autoFire, bool requireTarget,
    int projectileFrameCount, sf::Time projectileFrameDuration)
    : m_label(std::move(label)),
      m_damage(damage),
      m_minRange(minRange),
      m_maxRange(maxRange),
      m_cooldown(cooldown),
      m_projectileSpeed(projectileSpeed),
      m_projectileHitRadius(projectileHitRadius),
      m_projectileTexturePath(std::move(projectileTexturePath)),
      m_projectileVisualSize(projectileVisualSize),
      m_projectileFrameCount(projectileFrameCount),
      m_projectileFrameDuration(projectileFrameDuration),
      m_shotDelay(shotDelay),
      m_targetFilter(std::move(targetFilter)),
      m_autoFire(autoFire),
      m_requireTarget(requireTarget)
{
    assert(damage >= 0 && "RangedAttackComponent: damage must not be negative");
    assert(minRange >= 0.f && "RangedAttackComponent: minRange must not be negative");
    assert(maxRange > minRange && "RangedAttackComponent: maxRange must be greater than minRange");
    assert(cooldown >= sf::Time::Zero && "RangedAttackComponent: cooldown must not be negative");
    assert(projectileSpeed > 0.f && "RangedAttackComponent: projectileSpeed must be positive");
    assert(shotDelay >= sf::Time::Zero && "RangedAttackComponent: shotDelay must not be negative");
}

void RangedAttackComponent::update(sf::Time dt)
{
    if (m_cooldownRemaining > sf::Time::Zero) {
        m_cooldownRemaining -= dt;
    }

    if (m_hasPendingShot) {
        m_shotDelayRemaining -= dt;
        if (m_shotDelayRemaining <= sf::Time::Zero) {
            resolvePendingShot();
        }
    }

    if (m_autoFire && !m_hasPendingShot) {
        tryShoot();
    }
}

void RangedAttackComponent::resolvePendingShot()
{
    m_hasPendingShot = false;

    GameObject* owner = getOwner();
    if (!owner) {
        return;
    }

    // Стрелявший мог умереть, пока лук натягивался (см. AttackComponent::resolvePendingHit — тот же приём).
    HealthComponent* ownHealth = owner->getComponent<HealthComponent>();
    if (ownHealth && ownHealth->isDead()) {
        return;
    }

    auto projectile = std::make_unique<Projectile>(owner->getPosition(), m_pendingDirection, m_projectileSpeed, m_damage,
        m_projectileHitRadius, m_maxRange + m_projectileHitRadius, owner, m_projectileTexturePath, m_projectileVisualSize,
        m_projectileFrameCount, m_projectileFrameDuration);
    GameWorld::instance().spawnInRoot(std::move(projectile));

    LOG_INFO(m_label + " fired a projectile");
}

bool RangedAttackComponent::findTargetDirection(
    GameObject* owner, sf::Vector2f facing, float facingLength, sf::Vector2f& outDirection) const
{
    // За стеной снаряд всё равно не пролетит (см. ProjectileComponent) — не стрелять, если цели даже не видно,
    // иначе лучник впустую тратит стрелы/анимацию по игроку, спрятавшемуся за углом. Ближний бой (AttackComponent)
    // этой проверки не делает — см. findBestAttackTarget, зачем navGridForLineOfSight там опционален.
    NavGrid* nav = GameWorld::instance().getNavGrid();
    HealthComponent* best = findBestAttackTarget(owner, facing, facingLength, m_minRange, m_maxRange,
        /*checkCone=*/true, m_targetFilter, nav);
    if (!best) {
        return false;
    }

    GameObject* target = best->getOwner();
    sf::Vector2f delta = target->getPosition() - owner->getPosition();
    float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (distance <= 0.0001f) {
        return false; // Та же защита от деления на ноль, что была в исходной проверке дистанции.
    }
    outDirection = delta / distance;
    return true;
}

bool RangedAttackComponent::tryShoot()
{
    if (isOnCooldown()) {
        return false;
    }

    GameObject* owner = getOwner();
    if (!owner) {
        return false;
    }

    HealthComponent* ownHealth = owner->getComponent<HealthComponent>();
    if (ownHealth && (ownHealth->isDead() || ownHealth->isStunned())) {
        return false;
    }

    IDirectionProvider* directionProvider = owner->getComponent<IDirectionProvider>();
    sf::Vector2f facing = directionProvider ? directionProvider->getFacing() : sf::Vector2f(0.f, 0.f);
    float facingLength = std::sqrt(facing.x * facing.x + facing.y * facing.y);

    sf::Vector2f direction;
    bool hasTarget = findTargetDirection(owner, facing, facingLength, direction);

    if (!hasTarget) {
        if (m_requireTarget) {
            return false;
        }
        // requireTarget=false (ручной выстрел игрока) без цели в конусе — стреляем "от бедра" в направлении facing владельца.
        if (facingLength <= 0.0001f) {
            return false; // совсем без направления — стрелять некуда
        }
        direction = facing / facingLength;
    }

    // Кулдаун и "выстрел начался" стартуют сразу, как у AttackComponent — от этого зависит SoldierAnimationComponent.
    m_cooldownRemaining = m_cooldown;
    m_justFired = true;

    if (m_shotDelay <= sf::Time::Zero) {
        auto projectile = std::make_unique<Projectile>(owner->getPosition(), direction, m_projectileSpeed, m_damage,
            m_projectileHitRadius, m_maxRange + m_projectileHitRadius, owner, m_projectileTexturePath, m_projectileVisualSize,
            m_projectileFrameCount, m_projectileFrameDuration);
        GameWorld::instance().spawnInRoot(std::move(projectile));
        LOG_INFO(m_label + " fired a projectile");
    } else {
        m_hasPendingShot = true;
        m_pendingDirection = direction;
        m_shotDelayRemaining = m_shotDelay;
    }
    return true;
}
