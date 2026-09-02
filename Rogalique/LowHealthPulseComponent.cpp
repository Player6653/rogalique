#include "LowHealthPulseComponent.h"
#include "HealthComponent.h"
#include "SpriteComponent.h"
#include <cmath>

namespace
{
    const sf::Color LOW_HP_COLOR(220, 40, 40);
    const sf::Color NORMAL_COLOR(255, 255, 255);
    constexpr float PI = 3.14159265f;

    sf::Color lerpColor(const sf::Color& a, const sf::Color& b, float t)
    {
        return sf::Color(
            static_cast<sf::Uint8>(a.r + (b.r - a.r) * t), static_cast<sf::Uint8>(a.g + (b.g - a.g) * t),
            static_cast<sf::Uint8>(a.b + (b.b - a.b) * t));
    }
}

LowHealthPulseComponent::LowHealthPulseComponent(
    HealthComponent& health, SpriteComponent& sprite, int lowHpThreshold, sf::Time pulsePeriod)
    : m_health(health)
    , m_sprite(sprite)
    , m_lowHpThreshold(lowHpThreshold)
    , m_pulsePeriod(pulsePeriod)
    , m_pulseTimer(sf::Time::Zero)
{
}

void LowHealthPulseComponent::update(sf::Time dt)
{
    if (m_health.isDead() || m_health.getHp() > m_lowHpThreshold) {
        m_sprite.setColor(NORMAL_COLOR);
        m_pulseTimer = sf::Time::Zero;
        return;
    }

    m_pulseTimer += dt;
    float period = m_pulsePeriod.asSeconds();
    if (period <= 0.f) {
        m_sprite.setColor(LOW_HP_COLOR);
        return;
    }
    float phase = std::fmod(m_pulseTimer.asSeconds(), period) / period; // 0..1 внутри текущего цикла.
    float wave = 0.5f - 0.5f * std::cos(phase * 2.f * PI);
    m_sprite.setColor(lerpColor(NORMAL_COLOR, LOW_HP_COLOR, wave));
}

void LowHealthPulseComponent::reset()
{
    m_pulseTimer = sf::Time::Zero;
    m_sprite.setColor(NORMAL_COLOR);
}
