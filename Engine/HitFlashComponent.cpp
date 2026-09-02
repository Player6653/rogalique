#include "pch.h"
#include "HitFlashComponent.h"
#include "GameObject.h"
#include "HealthComponent.h"
#include "SpriteComponent.h"

HitFlashComponent::HitFlashComponent(
    SpriteComponent& sprite, sf::Time flashDuration, sf::Time blinkInterval, sf::Color flashColor)
    : m_sprite(sprite),
      m_flashDuration(flashDuration),
      m_blinkInterval(blinkInterval),
      m_flashColor(flashColor)
{
}

void HitFlashComponent::update(sf::Time dt)
{
    GameObject* owner = getOwner();
    auto* health = owner ? owner->getComponent<HealthComponent>() : nullptr;
    bool stunnedNow = health && health->isStunned();

    if (stunnedNow && !m_wasStunned) {
        // Перезапускаем мигание с начала, даже если предыдущее ещё не доиграло.
        m_flashTimeRemaining = m_flashDuration;
        m_blinkTimer = sf::Time::Zero;
        m_flashOn = true;
        m_sprite.setColor(m_flashColor);
    }
    m_wasStunned = stunnedNow;

    if (m_flashTimeRemaining <= sf::Time::Zero) {
        return;
    }

    m_flashTimeRemaining -= dt;
    if (m_flashTimeRemaining <= sf::Time::Zero) {
        m_sprite.clearColor();
        return;
    }

    m_blinkTimer += dt;
    if (m_blinkTimer >= m_blinkInterval) {
        m_blinkTimer -= m_blinkInterval;
        m_flashOn = !m_flashOn;
        // Выключенная фаза — родной цвет объекта (White для текстуры, свой цвет для заглушки).
        if (m_flashOn) {
            m_sprite.setColor(m_flashColor);
        } else {
            m_sprite.clearColor();
        }
    }
}

void HitFlashComponent::reset()
{
    m_flashTimeRemaining = sf::Time::Zero;
    m_blinkTimer = sf::Time::Zero;
    m_wasStunned = false;
    m_flashOn = false;
    m_sprite.clearColor();
}
