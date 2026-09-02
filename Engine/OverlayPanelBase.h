#pragma once
#include "EngineExport.h"
#include "NineSliceSprite.h"
#include <SFML/Graphics.hpp>
#include <string>

// Общий каркас оверлеев с панелью.
class ENGINE_API OverlayPanelBase {
public:
    OverlayPanelBase(sf::Vector2f windowSize, const std::string& panelTexturePath, const std::string& fontPath, std::string title,
        unsigned titleCharacterSize, sf::Color titleColor, bool titleBold, sf::Uint8 dimAlpha);

    // Верхний левый угол панели заданного размера, центрированной в windowSize наследник считает panelSize сам.
    sf::Vector2f computePanelPosition(sf::Vector2f panelSize) const;

    // Пересчитывает панель под panelPos/panelSize и кладёт заголовок по центру панели на высоте titleCenterY.
    void layout(sf::Vector2f panelPos, sf::Vector2f panelSize, float titleCenterY);

    // Меняет текст заголовка позицию не трогает (она про центр панели, а не про конкретную строку) — нужно
    // постраничным оверлеям (CreditsOverlayComponent), у которых на разных страницах разный заголовок.
    void setTitle(const std::string& title);

    void draw(sf::RenderWindow& window) const;

    bool hasFont() const
    {
        return m_hasFont;
    }
    const sf::Font& getFont() const
    {
        return m_font;
    }

private:
    sf::Vector2f m_windowSize;
    sf::RectangleShape m_dim;
    NineSliceSprite m_panel;

    sf::Font m_font;
    bool m_hasFont = false;
    sf::Text m_title;
};
