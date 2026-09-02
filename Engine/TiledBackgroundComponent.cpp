#include "pch.h"
#include "TiledBackgroundComponent.h"
#include "Log.h"

TiledBackgroundComponent::TiledBackgroundComponent(
    sf::Vector2f windowSize, const std::string& texturePath, sf::IntRect tileRect, std::function<bool()> isVisible)
    : m_isVisible(std::move(isVisible))
{
    m_hasTexture = m_texture.loadFromFile(texturePath, tileRect);
    if (!m_hasTexture) {
        LOG_WARN("TiledBackgroundComponent: не удалось загрузить текстуру \"" + texturePath + "\"");
        return;
    }

    // Текстура в памяти остаётся размером с один тайл (tileRect) — размножает её по экрану аппаратный wrap, а не N реальных копий.
    m_texture.setRepeated(true);
    m_sprite.setTexture(m_texture);
    m_sprite.setTextureRect(sf::IntRect(0, 0, static_cast<int>(windowSize.x), static_cast<int>(windowSize.y)));
}

void TiledBackgroundComponent::draw(sf::RenderWindow& window) const
{
    if (!m_hasTexture || !m_isVisible || !m_isVisible()) {
        return;
    }
    window.draw(m_sprite);
}
