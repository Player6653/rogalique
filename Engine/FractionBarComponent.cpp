#include "pch.h"
#include "FractionBarComponent.h"
#include <algorithm>

FractionBarComponent::FractionBarComponent(std::function<float()> getFraction, sf::Vector2f size,
    sf::Color backgroundColor, sf::Color fillColor, std::function<bool()> isVisible)
    : m_getFraction(std::move(getFraction))
    , m_isVisible(std::move(isVisible))
    , m_size(size)
{
    m_background.setSize(size);
    m_background.setFillColor(backgroundColor);
    m_background.setOutlineColor(sf::Color(0, 0, 0, 220));
    m_background.setOutlineThickness(2.f);

    m_fill.setSize(sf::Vector2f(0.f, size.y));
    m_fill.setFillColor(fillColor);
}

void FractionBarComponent::update(sf::Time)
{
    float fraction = std::max(0.f, std::min(1.f, m_getFraction ? m_getFraction() : 0.f));
    m_fill.setSize(sf::Vector2f(m_size.x * fraction, m_size.y));
}

void FractionBarComponent::onOwnerMoved(sf::Vector2f newPosition)
{
    m_background.setPosition(newPosition);
    m_fill.setPosition(newPosition);
}

void FractionBarComponent::draw(sf::RenderWindow& window) const
{
    if (m_isVisible && !m_isVisible()) {
        return;
    }
    window.draw(m_background);
    window.draw(m_fill);
}
