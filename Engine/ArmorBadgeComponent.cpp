#include "pch.h"
#include "ArmorBadgeComponent.h"
#include "Log.h"

namespace
{
    constexpr float ICON_SIZE = 32.f;
    constexpr float PADDING = 6.f;
    constexpr unsigned VALUE_CHAR_SIZE = 20;
} // namespace

ArmorBadgeComponent::ArmorBadgeComponent(std::function<int()> getValue, const std::string& iconTexturePath,
    const std::string& fontPath, std::function<bool()> isVisible)
    : m_getValue(std::move(getValue)),
      m_isVisible(std::move(isVisible))
{
    m_hasIcon = m_iconTexture.loadFromFile(iconTexturePath);
    if (!m_hasIcon) {
        LOG_WARN("ArmorBadgeComponent: не удалось загрузить текстуру \"" + iconTexturePath + "\"");
    } else {
        m_iconSprite.setTexture(m_iconTexture, true);
        sf::Vector2u textureSize = m_iconTexture.getSize();
        m_iconSprite.setScale(ICON_SIZE / static_cast<float>(textureSize.x), ICON_SIZE / static_cast<float>(textureSize.y));
    }

    m_hasFont = m_font.loadFromFile(fontPath);
    if (!m_hasFont) {
        LOG_WARN("ArmorBadgeComponent: не удалось загрузить шрифт \"" + fontPath + "\"");
    } else {
        m_valueText.setFont(m_font);
        m_valueText.setCharacterSize(VALUE_CHAR_SIZE);
        m_valueText.setFillColor(sf::Color::White);
        m_valueText.setOutlineColor(sf::Color(0, 0, 0, 200));
        m_valueText.setOutlineThickness(2.f);
    }
}

void ArmorBadgeComponent::update(sf::Time)
{
    if (!m_hasFont) {
        return;
    }
    m_valueText.setString(std::to_string(m_getValue ? m_getValue() : 0));
    sf::FloatRect bounds = m_valueText.getLocalBounds();
    m_valueText.setOrigin(bounds.left, bounds.top + bounds.height / 2.f);
}

void ArmorBadgeComponent::draw(sf::RenderWindow& window) const
{
    if (m_isVisible && !m_isVisible()) {
        return;
    }

    if (m_hasIcon) {
        window.draw(m_iconSprite);
    }
    if (m_hasFont) {
        window.draw(m_valueText);
    }
}

void ArmorBadgeComponent::onOwnerMoved(sf::Vector2f newPosition)
{
    m_iconSprite.setPosition(newPosition);
    m_valueText.setPosition(newPosition + sf::Vector2f(PADDING + ICON_SIZE, ICON_SIZE / 2.f));
}
