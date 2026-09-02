#include "pch.h"
#include "NameEntryOverlayComponent.h"
#include "FocusedInput.h"
#include "GameWorld.h"
#include "NameEntryInput.h"
#include "TextInputBuffer.h"
#include <cmath>

namespace
{
    constexpr float PANEL_WIDTH = 480.f;
    constexpr float LINE_HEIGHT = 28.f;
    constexpr float TITLE_HEIGHT = 50.f;
    constexpr float PANEL_PADDING = 28.f;
    constexpr unsigned LINE_CHARACTER_SIZE = 18;
    constexpr unsigned INPUT_CHARACTER_SIZE = 24;
    // Место под строку ввода отдельно от infoLines — крупнее шрифтом, чтобы не путать с обычным текстом.
    constexpr float INPUT_ROW_HEIGHT = 48.f;
    const sf::Time CURSOR_BLINK_INTERVAL = sf::seconds(0.5f);
} // namespace

NameEntryOverlayComponent::NameEntryOverlayComponent(sf::Vector2f windowSize, const std::string& panelTexturePath,
    const std::string& fontPath, std::string title, std::function<void(const std::string&)> onConfirm)
    : m_title(std::move(title)),
      m_onConfirm(std::move(onConfirm)),
      m_base(windowSize, panelTexturePath, fontPath, m_title, 26, sf::Color(255, 215, 90), true, 190)
{
    if (m_base.hasFont()) {
        m_inputText.setFont(m_base.getFont());
        m_inputText.setCharacterSize(INPUT_CHARACTER_SIZE);
        m_inputText.setFillColor(sf::Color::Yellow);
    }
}

void NameEntryOverlayComponent::layout()
{
    float panelHeight = TITLE_HEIGHT + m_infoLines.size() * LINE_HEIGHT + INPUT_ROW_HEIGHT + PANEL_PADDING * 2.f;
    sf::Vector2f panelSize(PANEL_WIDTH, panelHeight);
    sf::Vector2f panelPos = m_base.computePanelPosition(panelSize);
    m_base.layout(panelPos, panelSize, panelPos.y + PANEL_PADDING + TITLE_HEIGHT / 2.f);

    float centerX = panelPos.x + panelSize.x / 2.f;
    m_firstLinePosition = sf::Vector2f(centerX, panelPos.y + PANEL_PADDING + TITLE_HEIGHT);
    m_lineHeight = LINE_HEIGHT;
    m_inputLineY = m_firstLinePosition.y + m_infoLines.size() * LINE_HEIGHT + INPUT_ROW_HEIGHT / 2.f;
    m_inputText.setPosition(std::round(centerX), std::round(m_inputLineY));
}

void NameEntryOverlayComponent::rebuildInfoTexts()
{
    m_infoTexts.clear();
    m_infoTexts.reserve(m_infoLines.size());
    for (std::size_t i = 0; i < m_infoLines.size(); ++i) {
        sf::Text text;
        if (m_base.hasFont()) {
            text.setFont(m_base.getFont());
            text.setString(sf::String::fromUtf8(m_infoLines[i].begin(), m_infoLines[i].end()));
            text.setCharacterSize(LINE_CHARACTER_SIZE);
            text.setFillColor(sf::Color::White);
            sf::FloatRect bounds = text.getLocalBounds();
            text.setOrigin(std::round(bounds.left + bounds.width / 2.f), std::round(bounds.top + bounds.height / 2.f));
            text.setPosition(
                std::round(m_firstLinePosition.x), std::round(m_firstLinePosition.y + i * m_lineHeight + m_lineHeight / 2.f));
        }
        m_infoTexts.push_back(text);
    }
}

void NameEntryOverlayComponent::rebuildInputText()
{
    if (!m_base.hasFont()) {
        return;
    }
    // "_" курсор — мигает (см. update()), но занимает место в строке всегда, чтобы текст не прыгал вправо-влево
    // при каждом миге.
    std::string display = m_enteredName + (m_cursorVisible ? "_" : " ");
    m_inputText.setString(sf::String::fromUtf8(display.begin(), display.end()));
    sf::FloatRect bounds = m_inputText.getLocalBounds();
    m_inputText.setOrigin(std::round(bounds.left + bounds.width / 2.f), std::round(bounds.top + bounds.height / 2.f));
}

void NameEntryOverlayComponent::show(std::vector<std::string> infoLines)
{
    m_infoLines = std::move(infoLines);
    m_visible = true;
    m_enteredName.clear();
    m_enteredNameLength = 0;
    m_cursorBlinkElapsed = sf::Time::Zero;
    m_cursorVisible = true;
    layout();
    rebuildInfoTexts();
    rebuildInputText();
    GameWorld::instance().setModalOpen(true);
    m_enterEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Enter));
}

void NameEntryOverlayComponent::update(sf::Time dt)
{
    if (!m_visible) {
        m_enterEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Enter));
        return;
    }

    for (sf::Uint32 unicode : TextInputBuffer::charsThisFrame()) {
        NameEntryInput::handleChar(unicode, m_enteredName, m_enteredNameLength);
    }
    rebuildInputText();

    m_cursorBlinkElapsed += dt;
    if (m_cursorBlinkElapsed >= CURSOR_BLINK_INTERVAL) {
        m_cursorBlinkElapsed -= CURSOR_BLINK_INTERVAL;
        m_cursorVisible = !m_cursorVisible;
        rebuildInputText();
    }

    bool enterPressed = m_enterEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Enter));
    if (enterPressed && !m_enteredName.empty()) {
        m_visible = false;
        GameWorld::instance().setModalOpen(false);
        if (m_onConfirm) {
            m_onConfirm(m_enteredName);
        }
    }
}

void NameEntryOverlayComponent::draw(sf::RenderWindow& window) const
{
    if (!m_visible) {
        return;
    }
    m_base.draw(window);
    if (m_base.hasFont()) {
        for (const sf::Text& text : m_infoTexts) {
            window.draw(text);
        }
        window.draw(m_inputText);
    }
}
