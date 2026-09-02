#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include "InputEdge.h"
#include "OverlayPanelBase.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <string>
#include <vector>

// Оверлей меню.
class ENGINE_API MenuOverlayComponent : public IComponent {
public:
    struct Item {
        std::string label;
        std::function<void()> onActivate;
        std::function<bool()> isEnabled;
    };

    // moveSoundName/confirmSoundName — имена звуков, уже загруженных в AudioSystem. bottomMargin < 0 (по умолчанию)
    // значит "по центру экрана", как раньше; bottomMargin >= 0 прижимает панель к низу экрана на это расстояние
    // от нижнего края — нужно, например, экрану поражения, чтобы не закрывать собой сцену за спиной.
    MenuOverlayComponent(sf::Vector2f windowSize, const std::string& panelTexturePath, const std::string& buttonTexturePath,
        const std::string& fontPath, std::string title, std::vector<Item> items, std::function<bool()> isVisible,
        std::string moveSoundName = "", std::string confirmSoundName = "", float bottomMargin = -1.f);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;

private:
    void layout();
    void moveSelection(int direction);
    void activateSelected();
    // Наводка/клик мышью по кнопкам.
    void updateMouse();
    // Держит KeyEdge-поля свежими, пока меню не обрабатывает ввод.
    void refreshInputHeldFlags();

    std::function<bool()> m_isVisible;
    sf::Vector2f m_windowSize;
    float m_bottomMargin;

    std::vector<Item> m_items;
    int m_selected = 0;

    std::string m_moveSoundName;
    std::string m_confirmSoundName;

    KeyEdge m_upEdge;
    KeyEdge m_downEdge;
    KeyEdge m_activateEdge;
    KeyEdge m_mouseLeftEdge;

    OverlayPanelBase m_base;

    sf::Texture m_buttonTexture;
    bool m_hasButtonTexture = false;
    std::vector<sf::Sprite> m_buttonSprites;
    std::vector<sf::Text> m_buttonTexts;
};
