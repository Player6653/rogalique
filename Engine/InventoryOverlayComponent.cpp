#include "pch.h"
#include "InventoryOverlayComponent.h"
#include "FocusedInput.h"
#include "GameWorld.h"
#include "Log.h"
#include "RenderSystem.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float SLOT_SIZE = 40.f;
    constexpr float ICON_SIZE = 32.f;
    constexpr float SLOT_GAP = 8.f;

    constexpr int BAG_COLS = 4;
    constexpr int EQUIP_COLS = 4;

    constexpr float GRID_WIDTH = BAG_COLS * SLOT_SIZE + (BAG_COLS - 1) * SLOT_GAP;

    constexpr float PANEL_WIDTH = 360.f;
    constexpr float PANEL_PADDING = 24.f;
    constexpr float TITLE_HEIGHT = 60.f;
    constexpr float LABEL_HEIGHT = 22.f;
    constexpr float SECTION_GAP = 14.f;

    int rowsFor(int count, int cols)
    {
        return count <= 0 ? 0 : (count + cols - 1) / cols;
    }

    float gridHeight(int rows)
    {
        return rows <= 0 ? 0.f : rows * SLOT_SIZE + (rows - 1) * SLOT_GAP;
    }
} // namespace

InventoryOverlayComponent::InventoryOverlayComponent(sf::Vector2f windowSize, const std::string& panelTexturePath,
    const std::string& emptySlotTexturePath, const std::string& fontPath, std::string title, std::string equipLabel,
    std::string bagLabel, int bagSize, std::vector<std::string> equipFrameTexturePaths,
    std::function<InventorySlotView(int)> getBagSlot, std::function<InventorySlotView(int)> getEquipSlot,
    std::function<void(int)> onBagSlotClicked, std::function<void(int)> onEquipSlotClicked, std::function<void()> onClose)
    : m_bagSize(bagSize),
      m_bagCols(BAG_COLS),
      m_bagRows(rowsFor(bagSize, BAG_COLS)),
      m_equipCols(EQUIP_COLS),
      m_equipRows(rowsFor(static_cast<int>(equipFrameTexturePaths.size()), EQUIP_COLS)),
      m_getBagSlot(std::move(getBagSlot)),
      m_getEquipSlot(std::move(getEquipSlot)),
      m_onBagSlotClicked(std::move(onBagSlotClicked)),
      m_onEquipSlotClicked(std::move(onEquipSlotClicked)),
      m_onClose(std::move(onClose)),
      m_windowSize(windowSize),
      m_base(windowSize, panelTexturePath, fontPath, std::move(title), 26, sf::Color(255, 215, 90), true, 190)
{
    m_hasEmptySlotTexture = m_emptySlotTexture.loadFromFile(emptySlotTexturePath);
    if (!m_hasEmptySlotTexture) {
        LOG_WARN("InventoryOverlayComponent: не удалось загрузить текстуру пустого слота \"" + emptySlotTexturePath + "\"");
    }

    m_bagSlots.resize(m_bagSize);
    for (SlotView& view : m_bagSlots) {
        if (m_hasEmptySlotTexture) {
            view.frame.setTexture(m_emptySlotTexture, true);
        }
        if (m_base.hasFont()) {
            view.count.setFont(m_base.getFont());
            view.count.setCharacterSize(14);
            view.count.setFillColor(sf::Color::White);
            view.count.setOutlineColor(sf::Color(0, 0, 0, 220));
            view.count.setOutlineThickness(2.f);
        }
    }

    m_equipFrameTextures.resize(equipFrameTexturePaths.size());
    m_equipSlots.resize(equipFrameTexturePaths.size());
    for (std::size_t i = 0; i < equipFrameTexturePaths.size(); ++i) {
        if (m_equipFrameTextures[i].loadFromFile(equipFrameTexturePaths[i])) {
            m_equipSlots[i].frame.setTexture(m_equipFrameTextures[i], true);
        } else {
            LOG_WARN("InventoryOverlayComponent: не удалось загрузить текстуру слота экипировки \"" + equipFrameTexturePaths[i]
                     + "\"");
        }
        if (m_base.hasFont()) {
            m_equipSlots[i].count.setFont(m_base.getFont());
            m_equipSlots[i].count.setCharacterSize(14);
            m_equipSlots[i].count.setFillColor(sf::Color::White);
            m_equipSlots[i].count.setOutlineColor(sf::Color(0, 0, 0, 220));
            m_equipSlots[i].count.setOutlineThickness(2.f);
        }
    }

    if (m_base.hasFont()) {
        m_equipLabel.setFont(m_base.getFont());
        m_equipLabel.setCharacterSize(16);
        m_equipLabel.setFillColor(sf::Color(220, 220, 220));
        m_equipLabel.setString(sf::String::fromUtf8(equipLabel.begin(), equipLabel.end()));

        m_bagLabel.setFont(m_base.getFont());
        m_bagLabel.setCharacterSize(16);
        m_bagLabel.setFillColor(sf::Color(220, 220, 220));
        m_bagLabel.setString(sf::String::fromUtf8(bagLabel.begin(), bagLabel.end()));
    }

    layout();
}

void InventoryOverlayComponent::layoutSlot(SlotView& view, sf::Vector2f position)
{
    view.frame.setPosition(position);
    const sf::Texture* frameTexture = view.frame.getTexture();
    if (frameTexture && frameTexture->getSize().x > 0) {
        float frameScale = SLOT_SIZE / static_cast<float>(frameTexture->getSize().x);
        view.frame.setScale(frameScale, frameScale);
    }

    // Центр слота — совпадает с центром ICON_SIZE-бокса, вписанного в SLOT_SIZE; origin иконки уже в центре её
    // кадра (см. applyIcon), так что этого простого совпадения центров достаточно для центрирования любых пропорций.
    view.icon.setPosition(position.x + SLOT_SIZE / 2.f, position.y + SLOT_SIZE / 2.f);

    if (m_base.hasFont()) {
        view.count.setPosition(position.x + SLOT_SIZE - 16.f, position.y + SLOT_SIZE - 18.f);
    }
}

void InventoryOverlayComponent::layout()
{
    float equipHeight = gridHeight(m_equipRows);
    float bagHeight = gridHeight(m_bagRows);

    float panelHeight = TITLE_HEIGHT + LABEL_HEIGHT + equipHeight + SECTION_GAP + LABEL_HEIGHT + bagHeight + PANEL_PADDING * 2.f;
    sf::Vector2f panelSize(PANEL_WIDTH, panelHeight);
    sf::Vector2f panelPos = m_base.computePanelPosition(panelSize);
    m_base.layout(panelPos, panelSize, panelPos.y + PANEL_PADDING + TITLE_HEIGHT / 2.f);

    float gridLeft = panelPos.x + (panelSize.x - GRID_WIDTH) / 2.f;
    float y = panelPos.y + PANEL_PADDING + TITLE_HEIGHT;

    if (m_base.hasFont()) {
        m_equipLabel.setPosition(std::round(gridLeft), std::round(y));
    }
    y += LABEL_HEIGHT;

    for (std::size_t i = 0; i < m_equipSlots.size(); ++i) {
        int row = static_cast<int>(i) / m_equipCols;
        int col = static_cast<int>(i) % m_equipCols;
        sf::Vector2f pos(gridLeft + col * (SLOT_SIZE + SLOT_GAP), y + row * (SLOT_SIZE + SLOT_GAP));
        layoutSlot(m_equipSlots[i], pos);
    }
    y += equipHeight + SECTION_GAP;

    if (m_base.hasFont()) {
        m_bagLabel.setPosition(std::round(gridLeft), std::round(y));
    }
    y += LABEL_HEIGHT;

    for (int i = 0; i < m_bagSize; ++i) {
        int row = i / m_bagCols;
        int col = i % m_bagCols;
        sf::Vector2f pos(gridLeft + col * (SLOT_SIZE + SLOT_GAP), y + row * (SLOT_SIZE + SLOT_GAP));
        layoutSlot(m_bagSlots[i], pos);
    }
}

const sf::Texture* InventoryOverlayComponent::iconTexture(const std::string& path)
{
    auto it = m_iconTextures.find(path);
    if (it != m_iconTextures.end()) {
        return &it->second;
    }
    sf::Texture texture;
    if (!texture.loadFromFile(path)) {
        LOG_WARN("InventoryOverlayComponent: не удалось загрузить иконку \"" + path + "\"");
        return nullptr;
    }
    auto inserted = m_iconTextures.emplace(path, std::move(texture));
    return &inserted.first->second;
}

void InventoryOverlayComponent::applyIcon(SlotView& view, const InventorySlotView& data)
{
    if (data.iconPath.empty()) {
        view.hasIcon = false;
        return;
    }
    const sf::Texture* texture = iconTexture(data.iconPath);
    view.hasIcon = (texture != nullptr);
    if (!texture) {
        return;
    }
    int frameCount = std::max(1, data.iconFrameCount);
    int frameWidth = static_cast<int>(texture->getSize().x) / frameCount;
    int frameHeight = static_cast<int>(texture->getSize().y);
    view.icon.setTexture(*texture, true);
    view.icon.setTextureRect(sf::IntRect(0, 0, frameWidth, frameHeight));
    // Origin в центре кадра — вместе с позицией по центру ICON_SIZE-бокса (см. layoutSlot) центрирует иконку
    // независимо от её реальных пропорций, а не только квадратные кадры "от угла", как раньше.
    view.icon.setOrigin(frameWidth / 2.f, frameHeight / 2.f);
    if (frameWidth > 0 && frameHeight > 0) {
        // Один общий множитель, не по осям отдельно — кадр не всегда квадратный (см. ItemDefinition.cpp, паки
        // potions/keys/Skull/crossbow — 16x32), независимое растяжение по X/Y его бы исказило.
        float scale = std::min(ICON_SIZE / static_cast<float>(frameWidth), ICON_SIZE / static_cast<float>(frameHeight));
        view.icon.setScale(scale, scale);
    }
}

void InventoryOverlayComponent::refreshVisuals()
{
    for (int i = 0; i < m_bagSize; ++i) {
        SlotView& view = m_bagSlots[i];
        InventorySlotView data = m_getBagSlot ? m_getBagSlot(i) : InventorySlotView{};
        applyIcon(view, data);
        if (m_base.hasFont()) {
            view.count.setString(data.count > 1 ? std::to_string(data.count) : "");
        }
    }

    for (std::size_t i = 0; i < m_equipSlots.size(); ++i) {
        InventorySlotView data = m_getEquipSlot ? m_getEquipSlot(static_cast<int>(i)) : InventorySlotView{};
        applyIcon(m_equipSlots[i], data);
        if (m_base.hasFont()) {
            // В отличие от мешка (стек, скрываем "1" — там очевидно, что предмет один) здесь count — заряды
            // прочности ломающейся брони: "1" тут важная информация ("вот-вот сломается"), поэтому порог другой.
            m_equipSlots[i].count.setString(data.count > 0 ? std::to_string(data.count) : "");
        }
    }
}

void InventoryOverlayComponent::open()
{
    m_visible = true;
    GameWorld::instance().setPaused(true);
    GameWorld::instance().setModalOpen(true);
    m_escapeEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Escape));
    m_rmbEdge.sync(FocusedInput::isButtonPressed(sf::Mouse::Right));
    m_mouseLeftEdge.sync(FocusedInput::isButtonPressed(sf::Mouse::Left));
    refreshVisuals();
}

void InventoryOverlayComponent::close()
{
    m_visible = false;
    GameWorld::instance().setPaused(false);
    GameWorld::instance().setModalOpen(false);
    if (m_onClose) {
        m_onClose();
    }
}

void InventoryOverlayComponent::update(sf::Time)
{
    bool canToggle = GameWorld::instance().hasStarted() && !GameWorld::instance().isGameOver()
                     && (m_visible || (!GameWorld::instance().isPaused() && !GameWorld::instance().isModalOpen()));

    if (!canToggle) {
        m_toggleEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Tab));
        m_escapeEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Escape));
        m_rmbEdge.sync(FocusedInput::isButtonPressed(sf::Mouse::Right));
        m_mouseLeftEdge.sync(FocusedInput::isButtonPressed(sf::Mouse::Left));
        return;
    }

    // Tab и ПКМ — оба открывают/закрывают инвентарь (ПКМ тем же жестом "назад", что уже везде в игре — см.
    // CreditsOverlayComponent/SettingsOverlayComponent). poll() зовём для обоих безусловно — иначе не опрошенная
    // клавиша осталась бы с протухшим "было зажато" от предыдущего кадра.
    bool togglePressed = m_toggleEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Tab));
    bool rmbTogglePressed = m_rmbEdge.poll(FocusedInput::isButtonPressed(sf::Mouse::Right));
    if (togglePressed || rmbTogglePressed) {
        m_visible ? close() : open();
        return;
    }

    if (!m_visible) {
        return;
    }

    if (m_escapeEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Escape))) {
        close();
        return;
    }

    refreshVisuals();
    updateMouse();
}

void InventoryOverlayComponent::updateMouse()
{
    sf::RenderWindow& window = RenderSystem::instance().getWindow();
    sf::Vector2f mousePos(sf::Mouse::getPosition(window));
    bool clicked = m_mouseLeftEdge.poll(FocusedInput::isButtonPressed(sf::Mouse::Left));
    if (!clicked) {
        return;
    }

    for (int i = 0; i < m_bagSize; ++i) {
        if (m_bagSlots[i].frame.getGlobalBounds().contains(mousePos)) {
            if (m_onBagSlotClicked) {
                m_onBagSlotClicked(i);
            }
            return;
        }
    }
    for (std::size_t i = 0; i < m_equipSlots.size(); ++i) {
        if (m_equipSlots[i].frame.getGlobalBounds().contains(mousePos)) {
            if (m_onEquipSlotClicked) {
                m_onEquipSlotClicked(static_cast<int>(i));
            }
            return;
        }
    }
}

void InventoryOverlayComponent::draw(sf::RenderWindow& window) const
{
    if (!m_visible) {
        return;
    }

    m_base.draw(window);
    if (m_base.hasFont()) {
        window.draw(m_equipLabel);
        window.draw(m_bagLabel);
    }

    for (const SlotView& view : m_equipSlots) {
        window.draw(view.frame);
        if (view.hasIcon) {
            window.draw(view.icon);
            if (m_base.hasFont() && !view.count.getString().isEmpty()) {
                window.draw(view.count);
            }
        }
    }
    for (const SlotView& view : m_bagSlots) {
        window.draw(view.frame);
        if (view.hasIcon) {
            window.draw(view.icon);
            if (m_base.hasFont() && !view.count.getString().isEmpty()) {
                window.draw(view.count);
            }
        }
    }
}
