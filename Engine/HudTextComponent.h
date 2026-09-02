#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

// Простой текстовый HUD-лейбл: getText() зовётся каждый кадр, компонент ничего не знает об источнике строки —
// не плодить под каждую надпись (оружие+патроны, подсказки управления и т.п.) свой Engine-side класс. "\n" в
// строке — обычный перенос строки (sf::Text умеет сам), удобно для многострочных подсказок.
class ENGINE_API HudTextComponent : public IComponent {
public:
    // alignRight — origin по правому краю текста, а не по левому (удобно у правого края окна).
    HudTextComponent(const std::string& fontPath, unsigned characterSize, sf::Color color, std::function<std::string()> getText,
        bool alignRight = false, std::function<bool()> isVisible = nullptr);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;
    void onOwnerMoved(sf::Vector2f newPosition) override;

private:
    std::function<std::string()> m_getText;
    std::function<bool()> m_isVisible;
    bool m_alignRight;
    sf::Vector2f m_ownerPosition;

    sf::Font m_font;
    bool m_hasFont = false;
    sf::Text m_text;
    std::string m_lastText;
};
