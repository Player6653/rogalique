#include "LowHealthPulseComponent.h"
#include "HealthComponent.h"
#include "SpriteComponent.h"

namespace
{
    const sf::Color LOW_HP_COLOR(220, 40, 40);
    const sf::Color NORMAL_COLOR(255, 255, 255);
}

LowHealthPulseComponent::LowHealthPulseComponent(
    HealthComponent& health, SpriteComponent& sprite, int lowHpThreshold, sf::Time blinkInterval)
    : m_health(health)
    , m_sprite(sprite)
    , m_lowHpThreshold(lowHpThreshold)
    , m_blinkInterval(blinkInterval)
    , m_blinkTimer(sf::Time::Zero)
{
}

void LowHealthPulseComponent::update(sf::Time dt)
{
    if (m_health.isDead() || m_health.getHp() > m_lowHpThreshold) {
        if (m_flashOn) {
            m_sprite.setColor(NORMAL_COLOR);
            m_flashOn = false;
        }
        m_blinkTimer = sf::Time::Zero;
        return;
    }

    m_blinkTimer += dt;
    if (m_blinkTimer < m_blinkInterval) {
        return;
    }
    m_blinkTimer = sf::Time::Zero;
    m_flashOn = !m_flashOn;
    m_sprite.setColor(m_flashOn ? LOW_HP_COLOR : NORMAL_COLOR);
}

void LowHealthPulseComponent::reset()
{
    m_blinkTimer = sf::Time::Zero;
    m_flashOn = false;
    m_sprite.setColor(NORMAL_COLOR);
}
