#include "pch.h"
#include "AttackComponent.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "IDirectionProvider.h"
#include "Log.h"
#include "TargetFinder.h"
#include <algorithm>
#include <cassert>
#include <cmath>

AttackComponent::AttackComponent(std::string label, int damage, float range, sf::Time cooldown, bool autoAttack,
    sf::Time hitDelay, std::function<bool(GameObject*)> targetFilter, bool requireTarget, bool omnidirectional,
    float lifestealFraction, std::function<void(sf::Vector2f)> onAttackStarted, std::function<void(sf::Vector2f)> onHit)
    : m_label(std::move(label)),
      m_damage(damage),
      m_range(range),
      m_cooldown(cooldown),
      m_autoAttack(autoAttack),
      m_requireTarget(requireTarget),
      m_omnidirectional(omnidirectional),
      m_hitDelay(hitDelay),
      m_targetFilter(std::move(targetFilter)),
      m_lifestealFraction(lifestealFraction),
      m_onAttackStarted(std::move(onAttackStarted)),
      m_onHit(std::move(onHit))
{
    assert(damage >= 0 && "AttackComponent: damage must not be negative");
    assert(range > 0.f && "AttackComponent: range must be positive");
    assert(cooldown >= sf::Time::Zero && "AttackComponent: cooldown must not be negative");
    assert(hitDelay >= sf::Time::Zero && "AttackComponent: hitDelay must not be negative");
    assert(lifestealFraction >= 0.f && "AttackComponent: lifestealFraction must not be negative");
}

void AttackComponent::update(sf::Time dt)
{
    if (m_cooldownRemaining > sf::Time::Zero) {
        m_cooldownRemaining -= dt;
    }

    if (m_pendingTarget) {
        m_hitDelayRemaining -= dt;
        if (m_hitDelayRemaining <= sf::Time::Zero) {
            resolvePendingHit();
        }
    }

    if (m_autoAttack) {
        tryAttack();
    }
}

void AttackComponent::resolvePendingHit()
{
    HealthComponent* target = m_pendingTarget;
    m_pendingTarget = nullptr;
    if (!target) {
        return;
    }
    // Цель могла быть не просто убита, а вовсе уничтожена (не только HealthComponent::isDead()==true, а сам
    // GameObject и его HealthComponent разрушены целиком) за время задержки удара — например, реролл уровня зовёт
    // destroyTransientChildren() на временных ботах волны арены/детях деления слизи, пока чей-то удар по такой
    // временной цели ещё "летит". m_pendingTarget тогда указывает на уже освобождённую память — разыменовывать
    // его (в том числе isDead() ниже) нельзя, не проверив сперва, что HealthComponent всё ещё жив: единственный
    // надёжный способ — сверить указатель с актуальным реестром GameWorld (тот же, что и findTarget() ниже),
    // в который HealthComponent сам добавляет/убирает себя в конструкторе/деструкторе.
    const std::vector<HealthComponent*>& liveHealthComponents = GameWorld::instance().getHealthComponents();
    if (std::find(liveHealthComponents.begin(), liveHealthComponents.end(), target) == liveHealthComponents.end()) {
        return;
    }
    if (target->isDead()) {
        return;
    }

    // Атакующий тоже мог умереть уже после того, как удар был запущен (его успели убить первым, пока замах ещё доигрывал), иначе получалось бы, что мёртвый враг всё равно бьёт, хотя tryAttack() специально не даёт мёртвому атаковать в принципе.
    GameObject* owner = getOwner();
    HealthComponent* ownHealth = owner ? owner->getComponent<HealthComponent>() : nullptr;
    if (ownHealth && ownHealth->isDead()) {
        return;
    }

    // Цель могла уйти из радиуса уже после того, как удар начался (например, игрок поймал момент замаха и
    // рывком унёсся прочь) — hitDelay задерживает урон, но не сам факт попадания: без этой проверки удар настиг
    // бы цель на любой дистанции, достаточно было пережить сам старт атаки. Неуязвимость рывка тут не спасает —
    // она короче hitDelay (например, у слизи 0.42с против 0.24с и-фреймов), так что урон проходил бы уже после
    // того, как она закончилась.
    GameObject* targetOwner = target->getOwner();
    if (owner && targetOwner) {
        sf::Vector2f delta = targetOwner->getPosition() - owner->getPosition();
        float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (distance > m_range) {
            return;
        }
    }

    applyHit(target, owner);
}

void AttackComponent::applyHit(HealthComponent* target, GameObject* owner)
{
    int appliedDamage = target->takeDamage(m_damage);
    LOG_INFO(m_label + " attacked for " + std::to_string(appliedDamage) + " damage (of " + std::to_string(m_damage)
             + " before armor; target hp now " + std::to_string(target->getHp()) + ")");

    // Лайфстил — только от реально прошедшего урона (не от нуля, если броня/неуязвимость всё поглотили), и только
    // если владелец сам жив (мёртвый уже не лечится, да и getComponent на разрушенном GameObject не позвать).
    if (appliedDamage > 0 && m_lifestealFraction > 0.f && owner) {
        HealthComponent* ownHealth = owner->getComponent<HealthComponent>();
        if (ownHealth && !ownHealth->isDead()) {
            int healAmount = static_cast<int>(static_cast<float>(appliedDamage) * m_lifestealFraction);
            if (healAmount > 0) {
                // setHp() сам прижимает к [0, maxHp] — не нужно отдельно проверять переполнение.
                ownHealth->setHp(ownHealth->getHp() + healAmount);
            }
        }
    }

    if (m_onHit) {
        GameObject* targetOwner = target->getOwner();
        m_onHit(targetOwner ? targetOwner->getPosition() : sf::Vector2f(0.f, 0.f));
    }
}

bool AttackComponent::tryAttack()
{
    // Пока не разрешился предыдущий удар (см. resolvePendingHit) — новый не начинаем.
    if (isOnCooldown() || m_pendingTarget) {
        return false;
    }

    GameObject* owner = getOwner();
    if (!owner) {
        return false;
    }

    // Мёртвый больше не атакует, иначе уже убитый враг мог бы добить игрока тем же кадром, в который получил смертельный удар.
    HealthComponent* ownHealth = owner->getComponent<HealthComponent>();
    if (ownHealth && (ownHealth->isDead() || ownHealth->isStunned())) {
        return false;
    }

    IDirectionProvider* directionProvider = owner->getComponent<IDirectionProvider>();
    sf::Vector2f facing = directionProvider ? directionProvider->getFacing() : sf::Vector2f(0.f, 0.f);
    float facingLength = std::sqrt(facing.x * facing.x + facing.y * facing.y);

    HealthComponent* health = findBestAttackTarget(owner, facing, facingLength, 0.f, m_range, !m_omnidirectional, m_targetFilter);
    // requireTarget=false (ручная атака игрока) — удар засчитывается даже мимо: кулдаун и анимация должны
    // работать всегда, просто без урона. requireTarget=true (автоботы) — вхолостую не машем.
    if (!health && m_requireTarget) {
        return false;
    }

    // Кулдаун стартует сразу с этого момента isOnCooldown()==true. Само событие "удар начался" отдаём через
    // m_justStarted/consumeJustStarted() — при автоатаке isOnCooldown() в тот же кадр может успеть и обнулиться,
    // и взвестись заново, так что внешний опрос раз в кадр эту границу не поймает.
    m_cooldownRemaining = m_cooldown;
    m_justStarted = true;
    if (m_onAttackStarted) {
        m_onAttackStarted(owner->getPosition());
    }

    if (health) {
        if (m_hitDelay <= sf::Time::Zero) {
            applyHit(health, owner);
        } else {
            m_pendingTarget = health;
            m_hitDelayRemaining = m_hitDelay;
        }
    }

    return true;
}
