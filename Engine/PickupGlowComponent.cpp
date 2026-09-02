#include "pch.h"
#include "PickupGlowComponent.h"
#include <cmath>

namespace
{
    // Скорость мерцания и амплитуда — подобраны на глаз, лишь бы заметно, но не раздражающе быстро.
    constexpr float PULSE_SPEED = 2.4f;
    constexpr float PULSE_AMPLITUDE = 0.22f;
    constexpr float PI = 3.14159265f;
} // namespace

PickupGlowComponent::PickupGlowComponent(float baseRadius)
    : m_baseRadius(baseRadius)
{
}

void PickupGlowComponent::update(sf::Time dt)
{
    m_elapsed += dt;
}

void PickupGlowComponent::draw(sf::RenderWindow& window) const
{
    if (!m_visible) {
        return;
    }

    float pulse = 1.f + PULSE_AMPLITUDE * std::sin(m_elapsed.asSeconds() * PULSE_SPEED * 2.f * PI);
    float radius = m_baseRadius * pulse;

    sf::CircleShape glow(radius);
    glow.setOrigin(radius, radius);
    glow.setPosition(m_position);
    glow.setFillColor(sf::Color(255, 235, 120, 70));
    glow.setOutlineColor(sf::Color(255, 245, 180, 160));
    glow.setOutlineThickness(2.f);
    window.draw(glow);
}

void PickupGlowComponent::onOwnerMoved(sf::Vector2f newPosition)
{
    m_position = newPosition;
}
