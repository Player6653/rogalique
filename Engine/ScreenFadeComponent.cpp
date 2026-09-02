#include "pch.h"
#include "ScreenFadeComponent.h"
#include <algorithm>

ScreenFadeComponent::ScreenFadeComponent(
    sf::Vector2f windowSize, sf::Color fadeColor, sf::Time fadeDuration, std::function<bool()> trigger)
    : m_fadeColor(fadeColor)
    , m_fadeDuration(fadeDuration)
    , m_trigger(std::move(trigger))
{
    m_overlay.setSize(windowSize);
    m_overlay.setFillColor(sf::Color(m_fadeColor.r, m_fadeColor.g, m_fadeColor.b, 0));
}

void ScreenFadeComponent::update(sf::Time dt)
{
    bool triggeredNow = m_trigger && m_trigger();
    if (triggeredNow && !m_wasTriggered) {
        // Фронт false->true — начинаем фейд заново с полной непрозрачности, а не продолжаем с прошлого раза.
        m_fadeElapsed = sf::Time::Zero;
    }
    m_wasTriggered = triggeredNow;

    if (!triggeredNow) {
        m_overlay.setFillColor(sf::Color(m_fadeColor.r, m_fadeColor.g, m_fadeColor.b, 0));
        return;
    }

    m_fadeElapsed += dt;
    float durationSeconds = m_fadeDuration.asSeconds();
    float fraction = durationSeconds > 0.f ? std::min(1.f, m_fadeElapsed.asSeconds() / durationSeconds) : 1.f;
    sf::Uint8 alpha = static_cast<sf::Uint8>(255.f * (1.f - fraction));
    m_overlay.setFillColor(sf::Color(m_fadeColor.r, m_fadeColor.g, m_fadeColor.b, alpha));
}

void ScreenFadeComponent::draw(sf::RenderWindow& window) const
{
    window.draw(m_overlay);
}

void ScreenFadeComponent::reset()
{
    m_wasTriggered = false;
    m_fadeElapsed = sf::Time::Zero;
    m_overlay.setFillColor(sf::Color(m_fadeColor.r, m_fadeColor.g, m_fadeColor.b, 0));
}
