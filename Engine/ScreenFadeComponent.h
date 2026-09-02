#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <functional>

// Полноэкранный прямоугольник заданного цвета, который тает в прозрачность за fadeDuration, стоит trigger()
// перейти false->true — тем же приёмом "не резко появляется, а всплывает", что и у экрана победы (тот раньше
// появлялся слишком резко). Общий на любой оверлей, не завязан конкретно на победу — просто добавляется
// поверх нужного экрана (позже в дереве сцены, см. GameWorld::getUIRoot) и накрывает его непрозрачным fadeColor
// в момент появления, дальше сам растворяется.
class ENGINE_API ScreenFadeComponent : public IComponent {
public:
    ScreenFadeComponent(sf::Vector2f windowSize, sf::Color fadeColor, sf::Time fadeDuration, std::function<bool()> trigger);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;
    void reset() override;

private:
    sf::Color m_fadeColor;
    sf::Time m_fadeDuration;
    std::function<bool()> m_trigger;

    sf::RectangleShape m_overlay;
    sf::Time m_fadeElapsed;
    bool m_wasTriggered = false;
};
