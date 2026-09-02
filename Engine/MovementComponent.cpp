#include "pch.h"
#include "MovementComponent.h"
#include "ColliderComponent.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "IDirectionProvider.h"
#include "PitComponent.h"
#include <cmath>

namespace
{
    // Криво не долетевший прыжок через яму/лаву (см. PitComponent) — не даём "походить" внутри как по обычному
    // полу: тот же урон, что у Trap (см. TrapComponent.cpp), и плавное выталкивание назад, туда, откуда прыгали,
    // чтобы попробовать заново, а не застрять намертво (тем более не идти вперёд как ни в чём не бывало).
    constexpr int PIT_STUCK_DAMAGE = 1;
    constexpr float PIT_STUCK_EJECT_STEP = 4.f;
    constexpr float PIT_STUCK_EJECT_MAX_DISTANCE = 400.f; // с запасом перекрывает любой разумный размер Pit.
    // За сколько доигрывает отскок (см. m_pitEjectElapsed/m_pitEjectDuration в .h) — не мгновенный телепорт, а
    // короткая анимация с замедлением к концу (см. update()), попросил игрок вместо резкого рывка.
    const sf::Time PIT_EJECT_DURATION = sf::seconds(0.25f);

    // easeOutCubic — быстрый старт, плавное замедление к цели; ощущается как настоящий "отскок", а не равномерная
    // езда с резкой остановкой в конце (как было бы с линейной интерполяцией).
    float easeOutCubic(float t)
    {
        float inv = 1.f - t;
        return 1.f - inv * inv * inv;
    }
} // namespace

MovementComponent::MovementComponent(float speed)
    : m_speed(speed)
{
}

void MovementComponent::update(sf::Time dt)
{
    GameObject* owner = getOwner();
    if (!owner) {
        return;
    }

    // Направление ищем у соседнего IDirectionProvider того же объекта.
    auto* directionSource = owner->getComponent<IDirectionProvider>();
    if (!directionSource) {
        return;
    }

    // Уже доигрываем отскок из ямы (см. ниже) — принудительное движение, идёт даже пока стан от того же самого
    // удара блокирует обычное управление ниже (иначе отскок навсегда замирал бы на месте старта на все 0.3с стана).
    if (m_pitEjectElapsed < m_pitEjectDuration) {
        m_pitEjectElapsed += dt;
        float t = m_pitEjectDuration > sf::Time::Zero
                      ? std::min(1.f, m_pitEjectElapsed.asSeconds() / m_pitEjectDuration.asSeconds())
                      : 1.f;
        owner->setPosition(m_pitEjectStart + (m_pitEjectTarget - m_pitEjectStart) * easeOutCubic(t));
        return;
    }

    // Мёртвый не двигается никогда. Оглушённый (стан от удара) — обычно тоже, но манёвр, который сам пробивает стан.
    HealthComponent* health = owner->getComponent<HealthComponent>();
    if (health && (health->isDead() || (health->isStunned() && !directionSource->isUninterruptible()))) {
        return;
    }

    ColliderComponent* myCollider = owner->getComponent<ColliderComponent>();
    bool ignoresPits = directionSource->ignoresObstacles();

    if (myCollider && !ignoresPits && isOverlappingAnyPit(owner, myCollider)) {
        if (health) {
            // Тот же вызов ставит игроку и небольшой стан (HealthComponent::takeDamage — 0.3с STUN_DURATION на
            // любой прошедший урон), отдельно делать ничего не нужно.
            health->takeDamage(PIT_STUCK_DAMAGE);
        }
        // Facing во время прыжка не меняется (см. InputComponent::update() — WASD не читается, пока идёт прыжок),
        // поэтому прямо тут это всё ещё направление того самого прыжка — отскакиваем строго в обратную сторону.
        sf::Vector2f ejectDirection = -directionSource->getFacing();
        m_pitEjectStart = owner->getPosition();
        m_pitEjectTarget = findSafeEjectPosition(owner, myCollider, ejectDirection);
        m_pitEjectElapsed = sf::Time::Zero;
        m_pitEjectDuration = PIT_EJECT_DURATION;
        return;
    }

    sf::Vector2f offset = directionSource->getMoveDirection() * m_speed * dt.asSeconds();
    if (offset.x == 0.f && offset.y == 0.f) {
        return;
    }

    if (!myCollider) {
        owner->move(offset);
        return;
    }

    bool passThroughActors = directionSource->isUninterruptible();

    // Проверяем каждую ось отдельно, чтобы движение вдоль стены не залипало из-за столкновения по другой оси.
    sf::Vector2f position = owner->getPosition();
    sf::Vector2f allowedOffset(0.f, 0.f);

    if (offset.x != 0.f
        && !collidesAt(owner, myCollider, sf::Vector2f(position.x + offset.x, position.y), passThroughActors, ignoresPits)) {
        allowedOffset.x = offset.x;
    }
    if (offset.y != 0.f
        && !collidesAt(owner, myCollider, sf::Vector2f(position.x + allowedOffset.x, position.y + offset.y), passThroughActors,
            ignoresPits)) {
        allowedOffset.y = offset.y;
    }

    if (allowedOffset.x != 0.f || allowedOffset.y != 0.f) {
        owner->move(allowedOffset);
    }
}

bool MovementComponent::collidesAt(GameObject* owner, ColliderComponent* myCollider, sf::Vector2f proposedPosition,
    bool passThroughActors, bool ignoresPits) const
{
    sf::FloatRect myBounds = myCollider->getBoundsAt(proposedPosition);

    // Кинематические (стены/Pit) — через пространственную сетку (см. GameWorld::queryKinematicColliders), не
    // перебором всех коллайдеров сцены: их уже многие сотни и растёт с каждой новой комнатой от игрока, раньше
    // это было основной причиной просадки FPS в бою.
    for (ColliderComponent* other : GameWorld::instance().queryKinematicColliders(myBounds)) {
        if (other == myCollider) {
            continue;
        }
        GameObject* otherOwner = other->getOwner();
        // Во время прыжка яма/лава (см. PitComponent) не преграда — перепрыгиваем; обычная стена так же блокирует, как и без прыжка.
        if (ignoresPits && otherOwner && otherOwner->getComponent<PitComponent>()) {
            continue;
        }
        if (myBounds.intersects(other->getBounds())) {
            return true;
        }
    }

    // Актёры (не кинематические) — их единицы-десятки, отдельный маленький список, прямой перебор ничего не стоит.
    if (!passThroughActors) {
        for (ColliderComponent* other : GameWorld::instance().getDynamicColliders()) {
            if (other == myCollider) {
                continue;
            }
            GameObject* otherOwner = other->getOwner();
            // Труп (HealthComponent::isDead()) больше не преграда — можно пройти сквозь, а не упираться в мёртвое тело.
            HealthComponent* otherHealth = otherOwner ? otherOwner->getComponent<HealthComponent>() : nullptr;
            if (otherHealth && otherHealth->isDead()) {
                continue;
            }
            if (myBounds.intersects(other->getBounds())) {
                return true;
            }
        }
    }
    return false;
}

bool MovementComponent::isOverlappingAnyPit(GameObject* owner, ColliderComponent* myCollider) const
{
    sf::FloatRect myBounds = myCollider->getBoundsAt(owner->getPosition());
    for (ColliderComponent* other : GameWorld::instance().queryKinematicColliders(myBounds)) {
        if (other == myCollider) {
            continue;
        }
        GameObject* otherOwner = other->getOwner();
        if (otherOwner && otherOwner->getComponent<PitComponent>() && myBounds.intersects(other->getBounds())) {
            return true;
        }
    }
    return false;
}

sf::Vector2f MovementComponent::findSafeEjectPosition(
    GameObject* owner, ColliderComponent* myCollider, sf::Vector2f direction) const
{
    float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    sf::Vector2f normalized = (length > 0.0001f) ? direction / length : sf::Vector2f(0.f, 1.f);
    sf::Vector2f position = owner->getPosition();

    for (float traveled = PIT_STUCK_EJECT_STEP; traveled <= PIT_STUCK_EJECT_MAX_DISTANCE; traveled += PIT_STUCK_EJECT_STEP) {
        sf::Vector2f candidate = position + normalized * traveled;
        sf::FloatRect candidateBounds = myCollider->getBoundsAt(candidate);
        bool stillInPit = false;
        for (ColliderComponent* other : GameWorld::instance().queryKinematicColliders(candidateBounds)) {
            if (other == myCollider) {
                continue;
            }
            GameObject* otherOwner = other->getOwner();
            if (otherOwner && otherOwner->getComponent<PitComponent>() && candidateBounds.intersects(other->getBounds())) {
                stillInPit = true;
                break;
            }
        }
        if (!stillInPit) {
            return candidate;
        }
    }
    // Не нашли чистую точку за разумное расстояние (очень большая яма?) — остаёмся на месте, хотя бы без урона
    // каждый кадр не спамим (см. update() — takeDamage() зовётся один раз до этого вызова).
    return position;
}
