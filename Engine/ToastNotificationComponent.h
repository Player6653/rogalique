#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <string>

// Отрисовщик всплывающих уведомлений (см. ToastNotificationSystem.h — там очередь и таймер, здесь только рендер).
// Один экземпляр на весь UI, по центру сверху экрана — как в большинстве игр, чтобы не перекрывать HUD в углах.
class ENGINE_API ToastNotificationComponent : public IComponent {
public:
    ToastNotificationComponent(sf::Vector2f windowSize, const std::string& fontPath, unsigned characterSize = 22);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;

private:
    void rebuildText(const std::string& text);

    sf::Vector2f m_windowSize;
    sf::Font m_font;
    bool m_hasFont = false;
    unsigned m_characterSize;

    sf::Text m_text;
    sf::RectangleShape m_background;
    std::string m_lastText;
};
