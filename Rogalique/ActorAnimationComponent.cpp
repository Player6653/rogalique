#include "ActorAnimationComponent.h"
#include "GameObject.h"
#include "HealthComponent.h"
#include "IAnimatedActor.h"
#include "IDirectionProvider.h"
#include "SpriteComponent.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float EPS = 0.0001f;
}

ActorAnimationComponent::ActorAnimationComponent(ActorAnimationConfig config)
    : m_config(std::move(config)),
      m_attackVisualTimeRemaining(m_config.attacks.size(), sf::Time::Zero),
      m_attackClipIndex(m_config.attacks.size(), 0)
{
}

void ActorAnimationComponent::applyClip(SpriteComponent& sprite, const ActorAnimClip& clip) const
{
    sprite.loadAnimation(clip.path, clip.frameCount, clip.frameDuration, clip.loop, clip.row, clip.rowCount);
}

void ActorAnimationComponent::applyShadowIfChanged(SpriteComponent& shadow, const ActorAnimClip& clip)
{
    if (clip.path == m_currentShadowPath) {
        return;
    }
    m_currentShadowPath = clip.path;
    applyClip(shadow, clip);
}

void ActorAnimationComponent::update(sf::Time dt)
{
    GameObject* owner = getOwner();
    auto* actor = dynamic_cast<IAnimatedActor*>(owner);
    if (!actor) {
        return;
    }

    SpriteComponent& body = actor->getBodySprite();
    SpriteComponent& shadow = actor->getShadowSprite();

    auto* health = owner->getComponent<HealthComponent>();
    auto* direction = owner->getComponent<IDirectionProvider>();
    if (!health || !direction) {
        return;
    }

    // Смерть необратима и перекрывает всё остальное — застываем на последнем кадре, больше ролик не трогаем.
    if (health->isDead()) {
        if (m_currentClip != "Death") {
            m_currentClip = "Death";
            applyClip(body, m_config.death);
        }
        applyShadowIfChanged(shadow, m_config.deathShadow);
        return;
    }

    bool hurtNow = health->isStunned();
    bool hurtJustStarted = hurtNow && !m_wasHurt;
    if (hurtJustStarted) {
        m_hurtVisualTimeRemaining = m_config.hurtVisualDuration;
    }
    m_wasHurt = hurtNow;
    if (m_hurtVisualTimeRemaining > sf::Time::Zero) {
        m_hurtVisualTimeRemaining -= dt;
    }
    bool isHurt = m_hurtVisualTimeRemaining > sf::Time::Zero;

    // Тикаем ВСЕ источники атаки каждый кадр (не только победивший по приоритету) — так же, как раньше делали
    // отдельные m_meleeVisualTimeRemaining/m_rangedVisualTimeRemaining в SoldierAnimationComponent. Победитель —
    // первый по порядку (см. класс-комментарий ActorAnimationConfig::attacks), у кого таймер ещё не истёк.
    bool anyAttackJustTriggered = false;
    int activeAttack = -1;
    for (std::size_t i = 0; i < m_config.attacks.size(); ++i) {
        ActorAttackAnim& attack = m_config.attacks[i];
        bool justTriggered = attack.consumeJustTriggered && attack.consumeJustTriggered();
        if (justTriggered) {
            m_attackVisualTimeRemaining[i] = attack.visualDuration;
            if (attack.clips.size() > 1) {
                m_attackClipIndex[i] = (m_attackClipIndex[i] + 1) % attack.clips.size();
            }
            anyAttackJustTriggered = true;
        }
        if (m_attackVisualTimeRemaining[i] > sf::Time::Zero) {
            m_attackVisualTimeRemaining[i] -= dt;
        }
        if (activeAttack < 0 && m_attackVisualTimeRemaining[i] > sf::Time::Zero) {
            activeAttack = static_cast<int>(i);
        }
    }

    sf::Vector2f moveDirection = direction->getMoveDirection();
    float length = std::sqrt(moveDirection.x * moveDirection.x + moveDirection.y * moveDirection.y);
    bool isMoving = length > EPS;

    // Флип по getFacing(), не по getMoveDirection() — ChaseComponent продолжает "смотреть" на цель и после
    // остановки на stopDistance, а голое направление движения там уже (0,0) и не развернуло бы спрайт к цели,
    // подошедшей сбоку/сзади, пока существо стоит на месте.
    sf::Vector2f facing = direction->getFacing();
    if (facing.x < -EPS) {
        m_flippedX = true;
    } else if (facing.x > EPS) {
        m_flippedX = false;
    }
    body.setFlippedX(m_flippedX);
    shadow.setFlippedX(m_flippedX);

    std::string state;
    const ActorAnimClip* clip = nullptr;
    const ActorAnimClip* shadowClip = &m_config.normalShadow;
    if (isHurt) {
        state = "Hurt";
        clip = &m_config.hurt;
    } else if (activeAttack >= 0 && !m_config.attacks[activeAttack].clips.empty()) {
        std::size_t clipIndex = m_attackClipIndex[activeAttack];
        state = "Attack" + std::to_string(activeAttack) + "_" + std::to_string(clipIndex);
        ActorAttackAnim& attack = m_config.attacks[activeAttack];
        // clipIndex сам по себе всегда < attack.clips.size() (см. update() выше — увеличивается только по модулю
        // clips.size(), когда тот > 1), но это не защищает от clips.size()==0 — тогда clipIndex==0 читал бы за
        // границей пустого вектора; проверка в условии выше — единственное, что от этого спасает.
        clip = &attack.clips[clipIndex];
        // Своя тень под этот конкретный клип атаки, если задана и не пустая (см. ActorAttackAnim::shadowOverrides) —
        // иначе остаётся обычная normalShadow, назначенная выше.
        if (clipIndex < attack.shadowOverrides.size() && !attack.shadowOverrides[clipIndex].path.empty()) {
            shadowClip = &attack.shadowOverrides[clipIndex];
        }
    } else if (isMoving) {
        state = "Walk";
        clip = &m_config.walk;
    } else if (m_config.isAlert && m_config.isAlert()) {
        state = "IdleAlert";
        clip = &m_config.alertIdle;
    } else {
        state = "Idle";
        clip = &m_config.idle;
    }

    applyShadowIfChanged(shadow, *shadowClip);

    // hurtJustStarted/anyAttackJustTriggered форсируют перезагрузку, даже если строка состояния не поменялась
    // (второй удар/выстрел подряд, пока предыдущий ролик ещё доигрывает).
    if (state == m_currentClip && !hurtJustStarted && !anyAttackJustTriggered) {
        return;
    }
    m_currentClip = state;
    applyClip(body, *clip);
}

void ActorAnimationComponent::reset()
{
    m_wasHurt = false;
    m_hurtVisualTimeRemaining = sf::Time::Zero;
    std::fill(m_attackVisualTimeRemaining.begin(), m_attackVisualTimeRemaining.end(), sf::Time::Zero);
}
