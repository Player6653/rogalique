#include "pch.h"
#include "ToastNotificationComponent.h"
#include "ToastNotificationSystem.h"
#include "Log.h"
#include <cmath>

namespace
{
    constexpr float TOP_MARGIN = 90.f;
    constexpr float PADDING_X = 16.f;
    constexpr float PADDING_Y = 10.f;
}

ToastNotificationComponent::ToastNotificationComponent(sf::Vector2f windowSize, const std::string& fontPath, unsigned characterSize)
    : m_windowSize(windowSize)
    , m_characterSize(characterSize)
{
    m_hasFont = m_font.loadFromFile(fontPath);
    if (!m_hasFont) {
        LOG_WARN("ToastNotificationComponent: не удалось загрузить шрифт \"" + fontPath + "\"");
        return;
    }
    m_text.setFont(m_font);
    m_text.setCharacterSize(characterSize);
    m_background.setFillColor(sf::Color(20, 20, 20, 200));
    m_background.setOutlineColor(sf::Color(255, 215, 90, 220));
    m_background.setOutlineThickness(2.f);
}

void ToastNotificationComponent::rebuildText(const std::string& text)
{
    m_lastText = text;
    m_text.setString(sf::String::fromUtf8(text.begin(), text.end()));
    sf::FloatRect bounds = m_text.getLocalBounds();
    m_text.setOrigin(std::round(bounds.left + bounds.width / 2.f), std::round(bounds.top + bounds.height / 2.f));

    float centerX = m_windowSize.x / 2.f;
    m_text.setPosition(std::round(centerX), std::round(TOP_MARGIN));

    sf::Vector2f backgroundSize(bounds.width + PADDING_X * 2.f, bounds.height + PADDING_Y * 2.f);
    m_background.setSize(backgroundSize);
    m_background.setOrigin(std::round(backgroundSize.x / 2.f), std::round(backgroundSize.y / 2.f));
    m_background.setPosition(std::round(centerX), std::round(TOP_MARGIN));
}

void ToastNotificationComponent::update(sf::Time dt)
{
    // Синглтон продвигает свою очередь/таймер здесь же — единственное место, где это делается (см. класс-
    // комментарий ToastNotificationSystem.h), поэтому у этого компонента должен быть ровно один экземпляр в сцене.
    ToastNotificationSystem::instance().update(dt);
    if (!m_hasFont) {
        return;
    }

    const std::string& text = ToastNotificationSystem::instance().getActiveText();
    if (text != m_lastText) {
        rebuildText(text);
    }
}

void ToastNotificationComponent::draw(sf::RenderWindow& window) const
{
    if (!m_hasFont || !ToastNotificationSystem::instance().hasActive()) {
        return;
    }
    float alpha = ToastNotificationSystem::instance().getActiveAlpha();

    sf::RectangleShape background = m_background;
    sf::Color bgColor = background.getFillColor();
    bgColor.a = static_cast<sf::Uint8>(200.f * alpha);
    background.setFillColor(bgColor);
    sf::Color outlineColor = background.getOutlineColor();
    outlineColor.a = static_cast<sf::Uint8>(220.f * alpha);
    background.setOutlineColor(outlineColor);
    window.draw(background);

    sf::Text text = m_text;
    text.setFillColor(sf::Color(255, 255, 255, static_cast<sf::Uint8>(255.f * alpha)));
    window.draw(text);
}
