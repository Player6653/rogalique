#include "pch.h"
#include "MenuOverlayComponent.h"
#include "AudioSystem.h"
#include "FocusedInput.h"
#include "GameWorld.h"
#include "Log.h"
#include "RenderSystem.h"
#include <cmath>

namespace
{
    constexpr float PANEL_WIDTH = 360.f;
    constexpr float ITEM_HEIGHT = 44.f;
    constexpr float ITEM_SPACING = 12.f;
    constexpr float TITLE_HEIGHT = 60.f;
    constexpr float PANEL_PADDING = 24.f;
} // namespace

MenuOverlayComponent::MenuOverlayComponent(sf::Vector2f windowSize, const std::string& panelTexturePath,
    const std::string& buttonTexturePath, const std::string& fontPath, std::string title, std::vector<Item> items,
    std::function<bool()> isVisible, std::string moveSoundName, std::string confirmSoundName, float bottomMargin)
    : m_isVisible(std::move(isVisible)),
      m_windowSize(windowSize),
      m_bottomMargin(bottomMargin),
      m_items(std::move(items)),
      m_moveSoundName(std::move(moveSoundName)),
      m_confirmSoundName(std::move(confirmSoundName)),
      m_base(windowSize, panelTexturePath, fontPath, std::move(title), 28, sf::Color::White, false, 170)
{
    m_hasButtonTexture = m_buttonTexture.loadFromFile(buttonTexturePath);
    if (!m_hasButtonTexture) {
        LOG_WARN("MenuOverlayComponent: не удалось загрузить текстуру кнопки \"" + buttonTexturePath + "\"");
    }

    for (const Item& item : m_items) {
        sf::Sprite sprite;
        if (m_hasButtonTexture) {
            sprite.setTexture(m_buttonTexture);
        }
        m_buttonSprites.push_back(sprite);

        sf::Text text;
        if (m_base.hasFont()) {
            text.setFont(m_base.getFont());
            text.setString(sf::String::fromUtf8(item.label.begin(), item.label.end()));
            text.setCharacterSize(18);
            text.setFillColor(sf::Color::White);
        }
        m_buttonTexts.push_back(text);
    }

    layout();
}

void MenuOverlayComponent::layout()
{
    float panelHeight = TITLE_HEIGHT + m_items.size() * ITEM_HEIGHT
                        + (m_items.empty() ? 0.f : (m_items.size() - 1) * ITEM_SPACING) + PANEL_PADDING * 2.f;
    sf::Vector2f panelSize(PANEL_WIDTH, panelHeight);
    sf::Vector2f panelPos = m_base.computePanelPosition(panelSize);
    if (m_bottomMargin >= 0.f) {
        panelPos.y = m_windowSize.y - panelSize.y - m_bottomMargin;
    }
    m_base.layout(panelPos, panelSize, panelPos.y + PANEL_PADDING + TITLE_HEIGHT / 2.f);

    float buttonWidth = panelSize.x - PANEL_PADDING * 2.f;
    float y = panelPos.y + PANEL_PADDING + TITLE_HEIGHT;
    for (std::size_t i = 0; i < m_items.size(); ++i) {
        sf::Vector2f buttonPos(panelPos.x + PANEL_PADDING, y);
        if (m_hasButtonTexture) {
            sf::Vector2u textureSize = m_buttonTexture.getSize();
            int frameWidth = static_cast<int>(textureSize.x) / 4;
            m_buttonSprites[i].setTextureRect(sf::IntRect(0, 0, frameWidth, textureSize.y));
            m_buttonSprites[i].setPosition(buttonPos);
            m_buttonSprites[i].setScale(
                buttonWidth / static_cast<float>(frameWidth), ITEM_HEIGHT / static_cast<float>(textureSize.y));
        }
        if (m_base.hasFont()) {
            sf::FloatRect textBounds = m_buttonTexts[i].getLocalBounds();
            m_buttonTexts[i].setOrigin(
                std::round(textBounds.left + textBounds.width / 2.f), std::round(textBounds.top + textBounds.height / 2.f));
            m_buttonTexts[i].setPosition(
                std::round(buttonPos.x + buttonWidth / 2.f), std::round(buttonPos.y + ITEM_HEIGHT / 2.f));
        }
        y += ITEM_HEIGHT + ITEM_SPACING;
    }
}

void MenuOverlayComponent::update(sf::Time dt)
{
    if (!m_isVisible || !m_isVisible()) {
        refreshInputHeldFlags();
        m_wasVisible = false;
        return;
    }
    if (GameWorld::instance().isModalOpen()) {
        refreshInputHeldFlags();
        return;
    }

    if (!m_wasVisible) {
        // Только что появилось (переход invisible->visible) — см. класс-комментарий m_wasVisible в .h. Считаем
        // текущую позицию курсора уже "учтённой" (hover ниже сработает заново только после РЕАЛЬНОГО движения
        // мыши, см. updateMouse()) и синхронизируем KeyEdge'ы без активации — тот же физический Enter, которым
        // подтвердили предыдущий экран (например, ввод имени на экране победы), иначе мог долететь и досюда в
        // тот же кадр и активировать первый попавшийся пункт раньше, чем игрок вообще увидел это меню.
        m_wasVisible = true;
        m_hasLastMousePos = true;
        m_lastMousePos = sf::Vector2f(sf::Mouse::getPosition(RenderSystem::instance().getWindow()));
        refreshInputHeldFlags();
        return;
    }

    if (m_upEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Up) || FocusedInput::isKeyPressed(sf::Keyboard::W))) {
        moveSelection(-1);
    }

    if (m_downEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Down) || FocusedInput::isKeyPressed(sf::Keyboard::S))) {
        moveSelection(1);
    }

    // Пока мир на паузе, InputComponent::update() не вызывается вовсе, и его флаг клавиша уже была зажата не обновляется.
    if (m_activateEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Enter) || FocusedInput::isKeyPressed(sf::Keyboard::Space))) {
        activateSelected();
    }

    updateMouse();

    // Подсветка выбранного пункта второй кадр листа кнопки (hover), у остальных первый (normal).
    if (m_hasButtonTexture) {
        sf::Vector2u textureSize = m_buttonTexture.getSize();
        int frameWidth = static_cast<int>(textureSize.x) / 4;
        for (std::size_t i = 0; i < m_buttonSprites.size(); ++i) {
            int frame = (static_cast<int>(i) == m_selected) ? 1 : 0;
            m_buttonSprites[i].setTextureRect(sf::IntRect(frame * frameWidth, 0, frameWidth, textureSize.y));
        }
    }

    // Недоступный пункт (isEnabled есть и вернул false) — серый текст.
    if (m_base.hasFont()) {
        for (std::size_t i = 0; i < m_buttonTexts.size(); ++i) {
            bool enabled = !m_items[i].isEnabled || m_items[i].isEnabled();
            m_buttonTexts[i].setFillColor(enabled ? sf::Color::White : sf::Color(120, 120, 120));
        }
    }
}

void MenuOverlayComponent::moveSelection(int direction)
{
    if (m_items.empty()) {
        return;
    }
    int count = static_cast<int>(m_items.size());
    m_selected = (m_selected + direction + count) % count;
    if (!m_moveSoundName.empty()) {
        AudioSystem::instance().playSound(m_moveSoundName);
    }
}

void MenuOverlayComponent::activateSelected()
{
    if (m_selected < 0 || m_selected >= static_cast<int>(m_items.size())) {
        return;
    }
    Item& item = m_items[m_selected];
    if (!item.onActivate || (item.isEnabled && !item.isEnabled())) {
        return;
    }
    if (!m_confirmSoundName.empty()) {
        AudioSystem::instance().playSound(m_confirmSoundName);
    }
    item.onActivate();
}

void MenuOverlayComponent::updateMouse()
{
    sf::RenderWindow& window = RenderSystem::instance().getWindow();
    sf::Vector2f mousePos(sf::Mouse::getPosition(window));

    // Наводка мышью меняет выбор — но только если курсор РЕАЛЬНО сдвинулся с прошлого кадра (см. класс-комментарий
    // m_lastMousePos в .h): иначе курсор, просто отдыхающий над какой-то кнопкой, молча перехватывал бы выбор у
    // клавиатуры каждый кадр, даже когда игрок явно листает меню стрелками, а не мышью.
    bool moved = !m_hasLastMousePos || mousePos != m_lastMousePos;
    m_lastMousePos = mousePos;
    m_hasLastMousePos = true;
    if (moved) {
        for (std::size_t i = 0; i < m_buttonSprites.size(); ++i) {
            if (m_buttonSprites[i].getGlobalBounds().contains(mousePos)) {
                m_selected = static_cast<int>(i);
                break;
            }
        }
    }

    if (!m_mouseLeftEdge.poll(FocusedInput::isButtonPressed(sf::Mouse::Left))) {
        return;
    }

    for (std::size_t i = 0; i < m_buttonSprites.size(); ++i) {
        if (m_buttonSprites[i].getGlobalBounds().contains(mousePos)) {
            m_selected = static_cast<int>(i);
            activateSelected();
            return;
        }
    }
}

void MenuOverlayComponent::refreshInputHeldFlags()
{
    m_upEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Up) || FocusedInput::isKeyPressed(sf::Keyboard::W));
    m_downEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Down) || FocusedInput::isKeyPressed(sf::Keyboard::S));
    m_activateEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Enter) || FocusedInput::isKeyPressed(sf::Keyboard::Space));
    m_mouseLeftEdge.sync(FocusedInput::isButtonPressed(sf::Mouse::Left));
}

void MenuOverlayComponent::draw(sf::RenderWindow& window) const
{
    if (!m_isVisible || !m_isVisible()) {
        return;
    }

    m_base.draw(window);
    for (std::size_t i = 0; i < m_buttonSprites.size(); ++i) {
        if (m_hasButtonTexture) {
            window.draw(m_buttonSprites[i]);
        }
        if (m_base.hasFont()) {
            window.draw(m_buttonTexts[i]);
        }
    }
}
