#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include "InputEdge.h"
#include "OverlayPanelBase.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <string>
#include <vector>

// Экран настроек.
class ENGINE_API SettingsOverlayComponent : public IComponent {
public:
    struct Slider {
        std::string label;
        std::function<float()> getValue;     // 0..1.
        std::function<void(float)> setValue; // 0..1, уже прижато к [0,1].
        float step = 0.1f;                   // на сколько сдвигает значение одно нажатие Left/Right.
        std::function<std::string()> getDisplayText;
    };

    SettingsOverlayComponent(sf::Vector2f windowSize, const std::string& panelTexturePath, const std::string& fontPath,
        std::string title, std::vector<Slider> sliders);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;

    void show();

private:
    void layout();
    void adjustSelected(float delta);
    void updateMouse();

    std::vector<Slider> m_sliders;
    int m_selected = 0;
    bool m_visible = false;

    KeyEdge m_escapeEdge;
    KeyEdge m_upEdge;
    KeyEdge m_downEdge;
    KeyEdge m_leftEdge;
    KeyEdge m_rightEdge;
    KeyEdge m_rmbEdge;

    OverlayPanelBase m_base;

    std::vector<sf::Text> m_labels;
    std::vector<sf::RectangleShape> m_trackBackgrounds;
    std::vector<sf::RectangleShape> m_trackFills;
};
