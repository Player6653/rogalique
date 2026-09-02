#include "pch.h"
#include "SettingsOverlayComponent.h"
#include "FocusedInput.h"
#include "GameWorld.h"
#include "Log.h"
#include "RenderSystem.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float PANEL_WIDTH = 420.f;
    constexpr float TITLE_HEIGHT = 60.f;
    constexpr float PANEL_PADDING = 28.f;
    constexpr float LABEL_HEIGHT = 24.f;
    constexpr float LABEL_BAR_GAP = 8.f;
    constexpr float BAR_HEIGHT = 18.f;
    constexpr float ROW_HEIGHT = LABEL_HEIGHT + LABEL_BAR_GAP + BAR_HEIGHT;
    constexpr float ROW_SPACING = 26.f;
} // namespace

SettingsOverlayComponent::SettingsOverlayComponent(sf::Vector2f windowSize, const std::string& panelTexturePath,
    const std::string& fontPath, std::string title, std::vector<Slider> sliders)
    : m_sliders(std::move(sliders)),
      m_base(windowSize, panelTexturePath, fontPath, std::move(title), 26, sf::Color(255, 215, 90), true, 190)
{
    for (const Slider& slider : m_sliders) {
        sf::Text label;
        if (m_base.hasFont()) {
            label.setFont(m_base.getFont());
            label.setCharacterSize(17);
            label.setFillColor(sf::Color::White);
        }
        m_labels.push_back(label);

        sf::RectangleShape background;
        background.setFillColor(sf::Color(20, 20, 20, 200));
        background.setOutlineColor(sf::Color(0, 0, 0, 220));
        background.setOutlineThickness(1.f);
        m_trackBackgrounds.push_back(background);

        sf::RectangleShape fill;
        fill.setFillColor(sf::Color(235, 190, 60, 235));
        m_trackFills.push_back(fill);
    }

    layout();
}

void SettingsOverlayComponent::layout()
{
    std::size_t count = m_sliders.size();
    float panelHeight = TITLE_HEIGHT + count * ROW_HEIGHT + (count == 0 ? 0.f : (count - 1) * ROW_SPACING) + PANEL_PADDING * 2.f;
    sf::Vector2f panelSize(PANEL_WIDTH, panelHeight);
    sf::Vector2f panelPos = m_base.computePanelPosition(panelSize);
    m_base.layout(panelPos, panelSize, panelPos.y + PANEL_PADDING + TITLE_HEIGHT / 2.f);

    float barWidth = panelSize.x - PANEL_PADDING * 2.f;
    float y = panelPos.y + PANEL_PADDING + TITLE_HEIGHT;
    for (std::size_t i = 0; i < m_sliders.size(); ++i) {
        if (m_base.hasFont()) {
            m_labels[i].setPosition(std::round(panelPos.x + PANEL_PADDING), std::round(y));
        }

        sf::Vector2f barPos(panelPos.x + PANEL_PADDING, y + LABEL_HEIGHT + LABEL_BAR_GAP);
        m_trackBackgrounds[i].setSize(sf::Vector2f(barWidth, BAR_HEIGHT));
        m_trackBackgrounds[i].setPosition(barPos);
        m_trackFills[i].setPosition(barPos);
        m_trackFills[i].setSize(sf::Vector2f(0.f, BAR_HEIGHT));

        y += ROW_HEIGHT + ROW_SPACING;
    }
}

void SettingsOverlayComponent::show()
{
    m_visible = true;
    GameWorld::instance().setModalOpen(true);
    m_escapeEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Escape));
    m_upEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Up) || FocusedInput::isKeyPressed(sf::Keyboard::W));
    m_downEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Down) || FocusedInput::isKeyPressed(sf::Keyboard::S));
    m_leftEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Left) || FocusedInput::isKeyPressed(sf::Keyboard::A));
    m_rightEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Right) || FocusedInput::isKeyPressed(sf::Keyboard::D));
    m_rmbEdge.sync(FocusedInput::isButtonPressed(sf::Mouse::Right));
}

void SettingsOverlayComponent::adjustSelected(float delta)
{
    if (m_selected < 0 || m_selected >= static_cast<int>(m_sliders.size())) {
        return;
    }
    Slider& slider = m_sliders[m_selected];
    if (!slider.getValue || !slider.setValue) {
        return;
    }
    float value = std::max(0.f, std::min(1.f, slider.getValue() + delta));
    slider.setValue(value);
}

void SettingsOverlayComponent::updateMouse()
{
    sf::RenderWindow& window = RenderSystem::instance().getWindow();
    sf::Vector2f mousePos(sf::Mouse::getPosition(window));

    // Наводка на строку выбирает пункт — как в MenuOverlayComponent, без звука на каждое лёгкое дрожание руки.
    for (std::size_t i = 0; i < m_sliders.size(); ++i) {
        sf::FloatRect rowBounds = m_trackBackgrounds[i].getGlobalBounds();
        rowBounds.top -= LABEL_HEIGHT + LABEL_BAR_GAP;
        rowBounds.height += LABEL_HEIGHT + LABEL_BAR_GAP;
        if (rowBounds.contains(mousePos)) {
            m_selected = static_cast<int>(i);
            break;
        }
    }

    // Пока ЛКМ зажата — тянуть по полосе можно непрерывно, как обычный слайдер.
    if (FocusedInput::isButtonPressed(sf::Mouse::Left)) {
        for (std::size_t i = 0; i < m_sliders.size(); ++i) {
            sf::FloatRect trackBounds = m_trackBackgrounds[i].getGlobalBounds();
            sf::FloatRect grabBounds(trackBounds.left, trackBounds.top - LABEL_HEIGHT - LABEL_BAR_GAP, trackBounds.width,
                trackBounds.height + LABEL_HEIGHT + LABEL_BAR_GAP);
            if (grabBounds.contains(mousePos) && m_sliders[i].setValue && trackBounds.width > 0.f) {
                m_selected = static_cast<int>(i);
                float value = (mousePos.x - trackBounds.left) / trackBounds.width;
                m_sliders[i].setValue(std::max(0.f, std::min(1.f, value)));
                break;
            }
        }
    }
}

void SettingsOverlayComponent::update(sf::Time dt)
{
    if (!m_visible) {
        m_escapeEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Escape));
        return;
    }

    bool escapePressed = m_escapeEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Escape));
    // ПКМ в любом месте экрана тот же назад, что и Escape.
    bool rmbPressed = m_rmbEdge.poll(FocusedInput::isButtonPressed(sf::Mouse::Right));

    if (escapePressed || rmbPressed) {
        m_visible = false;
        GameWorld::instance().setModalOpen(false);
        return;
    }

    updateMouse();

    if (m_upEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Up) || FocusedInput::isKeyPressed(sf::Keyboard::W))
        && !m_sliders.empty()) {
        m_selected = (m_selected - 1 + static_cast<int>(m_sliders.size())) % static_cast<int>(m_sliders.size());
    }

    if (m_downEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Down) || FocusedInput::isKeyPressed(sf::Keyboard::S))
        && !m_sliders.empty()) {
        m_selected = (m_selected + 1) % static_cast<int>(m_sliders.size());
    }

    if (m_leftEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Left) || FocusedInput::isKeyPressed(sf::Keyboard::A))
        && m_selected >= 0 && m_selected < static_cast<int>(m_sliders.size())) {
        adjustSelected(-m_sliders[m_selected].step);
    }

    if (m_rightEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Right) || FocusedInput::isKeyPressed(sf::Keyboard::D))
        && m_selected >= 0 && m_selected < static_cast<int>(m_sliders.size())) {
        adjustSelected(m_sliders[m_selected].step);
    }

    // Значения читаем каждый кадр так текст и полоса остаются верны, даже если значение поменяли откуда-то еще, а не только этим экраном.
    for (std::size_t i = 0; i < m_sliders.size(); ++i) {
        float value = m_sliders[i].getValue ? std::max(0.f, std::min(1.f, m_sliders[i].getValue())) : 0.f;

        if (m_base.hasFont()) {
            std::string valueText;
            if (m_sliders[i].getDisplayText) {
                valueText = m_sliders[i].getDisplayText();
            } else {
                int percent = static_cast<int>(std::lround(value * 100.f));
                valueText = std::to_string(percent) + "%";
            }
            std::string text = m_sliders[i].label + ": " + valueText;
            bool selected = (static_cast<int>(i) == m_selected);
            m_labels[i].setString(sf::String::fromUtf8(text.begin(), text.end()));
            m_labels[i].setFillColor(selected ? sf::Color::Yellow : sf::Color::White);
        }

        sf::Vector2f fillSize(m_trackBackgrounds[i].getSize().x * value, BAR_HEIGHT);
        m_trackFills[i].setSize(fillSize);

        bool selected = (static_cast<int>(i) == m_selected);
        m_trackBackgrounds[i].setOutlineColor(selected ? sf::Color(255, 215, 90, 255) : sf::Color(0, 0, 0, 220));
    }
}

void SettingsOverlayComponent::draw(sf::RenderWindow& window) const
{
    if (!m_visible) {
        return;
    }

    m_base.draw(window);
    for (std::size_t i = 0; i < m_sliders.size(); ++i) {
        window.draw(m_trackBackgrounds[i]);
        window.draw(m_trackFills[i]);
        if (m_base.hasFont()) {
            window.draw(m_labels[i]);
        }
    }
}
