#include "pch.h"
#include "HudTextComponent.h"
#include "Log.h"
#include <cmath>

HudTextComponent::HudTextComponent(const std::string& fontPath, unsigned characterSize, sf::Color color,
    std::function<std::string()> getText, bool alignRight, std::function<bool()> isVisible)
    : m_getText(std::move(getText)),
      m_isVisible(std::move(isVisible)),
      m_alignRight(alignRight)
{
    m_hasFont = m_font.loadFromFile(fontPath);
    if (!m_hasFont) {
        LOG_WARN("HudTextComponent: не удалось загрузить шрифт \"" + fontPath + "\"");
        return;
    }
    m_text.setFont(m_font);
    m_text.setCharacterSize(characterSize);
    m_text.setFillColor(color);
    m_text.setOutlineColor(sf::Color(0, 0, 0, 200));
    m_text.setOutlineThickness(2.f);
}

void HudTextComponent::update(sf::Time)
{
    if (!m_hasFont || !m_getText) {
        return;
    }
    std::string text = m_getText();
    if (text == m_lastText) {
        return;
    }
    m_lastText = text;
    m_text.setString(sf::String::fromUtf8(text.begin(), text.end()));
    // Origin по левому/правому верхнему углу (не центру) — так HUD-лейбл проще прибить к краю окна снаружи.
    sf::FloatRect bounds = m_text.getLocalBounds();
    float originX = m_alignRight ? bounds.left + bounds.width : bounds.left;
    m_text.setOrigin(std::round(originX), std::round(bounds.top));
    m_text.setPosition(m_ownerPosition);
}

void HudTextComponent::draw(sf::RenderWindow& window) const
{
    if (!m_hasFont) {
        return;
    }
    if (m_isVisible && !m_isVisible()) {
        return;
    }
    window.draw(m_text);
}

void HudTextComponent::onOwnerMoved(sf::Vector2f newPosition)
{
    m_ownerPosition = newPosition;
    m_text.setPosition(newPosition);
}
