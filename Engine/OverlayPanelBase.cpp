#include "pch.h"
#include "OverlayPanelBase.h"
#include "Log.h"
#include <cmath>

namespace
{
    // Измерено по Panel_9Slice_A.png (144x144).
    constexpr int PANEL_CORNER = 32;
} // namespace

OverlayPanelBase::OverlayPanelBase(sf::Vector2f windowSize, const std::string& panelTexturePath, const std::string& fontPath,
    std::string title, unsigned titleCharacterSize, sf::Color titleColor, bool titleBold, sf::Uint8 dimAlpha)
    : m_windowSize(windowSize),
      m_panel(panelTexturePath, PANEL_CORNER)
{
    m_dim.setSize(windowSize);
    m_dim.setFillColor(sf::Color(0, 0, 0, dimAlpha));

    m_hasFont = m_font.loadFromFile(fontPath);
    if (!m_hasFont) {
        LOG_WARN("OverlayPanelBase: не удалось загрузить шрифт \"" + fontPath + "\"");
        return;
    }

    m_title.setFont(m_font);
    m_title.setString(sf::String::fromUtf8(title.begin(), title.end()));
    m_title.setCharacterSize(titleCharacterSize);
    m_title.setFillColor(titleColor);
    if (titleBold) {
        m_title.setStyle(sf::Text::Bold);
    }
}

sf::Vector2f OverlayPanelBase::computePanelPosition(sf::Vector2f panelSize) const
{
    return sf::Vector2f((m_windowSize.x - panelSize.x) / 2.f, (m_windowSize.y - panelSize.y) / 2.f);
}

void OverlayPanelBase::layout(sf::Vector2f panelPos, sf::Vector2f panelSize, float titleCenterY)
{
    m_panel.setRect(panelPos, panelSize);

    if (!m_hasFont) {
        return;
    }
    // Округление origin/позиции до целого пикселя иначе sf::Text рендерится со смещением в доли пикселя и выходит смазанным.
    sf::FloatRect titleBounds = m_title.getLocalBounds();
    m_title.setOrigin(
        std::round(titleBounds.left + titleBounds.width / 2.f), std::round(titleBounds.top + titleBounds.height / 2.f));
    m_title.setPosition(std::round(panelPos.x + panelSize.x / 2.f), std::round(titleCenterY));
}

void OverlayPanelBase::setTitle(const std::string& title)
{
    if (!m_hasFont) {
        return;
    }
    m_title.setString(sf::String::fromUtf8(title.begin(), title.end()));
    // Позицию не трогаем — она уже стоит в центре панели (см. layout()), меняется только origin под новую ширину строки.
    sf::FloatRect bounds = m_title.getLocalBounds();
    m_title.setOrigin(std::round(bounds.left + bounds.width / 2.f), std::round(bounds.top + bounds.height / 2.f));
}

void OverlayPanelBase::draw(sf::RenderWindow& window) const
{
    window.draw(m_dim);
    m_panel.draw(window);
    if (m_hasFont) {
        window.draw(m_title);
    }
}
