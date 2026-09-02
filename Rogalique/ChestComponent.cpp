#include "ChestComponent.h"
#include "AudioSystem.h"
#include "FocusedInput.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "InteractionHelpers.h"
#include "InventoryComponent.h"
#include "Log.h"
#include "PickupGlowComponent.h"
#include "SpriteComponent.h"
#include "ToastNotificationSystem.h"
#include <utility>

namespace
{
    // Чуть больше, чем у ItemPickupComponent (36) — сундук физически крупнее иконки предмета, открывать удобнее с
    // небольшим запасом дистанции.
    constexpr float OPEN_DISTANCE = 44.f;

    // Resources/Map/Chest/Skull_chest_idle.png и _open.png — оба 768x64, 12 кадров 64x64 (проверено по размеру
    // файла). Idle — лёгкое "мерцание" по кругу (loop); open — ролик открытия целиком один раз (не loop),
    // последний кадр — открытый сундук, там и остаётся стоять после проигрывания.
    constexpr int CHEST_FRAME_COUNT = 12;
    const sf::Time CHEST_FRAME_DURATION = sf::seconds(0.1f);
} // namespace

ChestComponent::ChestComponent(const ItemDefinition& item, int count, SpriteComponent& icon, PickupGlowComponent& glow,
    std::string idleTexturePath, std::string openTexturePath)
    : m_item(item),
      m_count(count),
      m_icon(icon),
      m_glow(glow),
      m_idleTexturePath(std::move(idleTexturePath)),
      m_openTexturePath(std::move(openTexturePath))
{
}

void ChestComponent::update(sf::Time)
{
    if (m_opened) {
        m_interactEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::E));
        return;
    }

    GameObject* owner = getOwner();
    if (!owner) {
        return;
    }

    GameObject* player = nullptr;
    if (!isPlayerInRangeAndInteractPressed(*owner, OPEN_DISTANCE, m_interactEdge, &player)) {
        return;
    }

    auto* inventory = player->getComponent<InventoryComponent>();
    if (!inventory || !inventory->addItem(m_item, m_count)) {
        return; // Мешок полон — сундук остаётся закрытым, откроется позже, когда освободится место.
    }

    m_opened = true;
    m_icon.loadAnimation(m_openTexturePath, CHEST_FRAME_COUNT, CHEST_FRAME_DURATION, false);
    m_glow.setVisible(false);
    AudioSystem::instance().playSound("ui_confirm");
    LOG_INFO("Chest: открыт, получено \"" + m_item.displayName + "\" x" + std::to_string(m_count));
    std::string toastText = "Сундук: " + m_item.displayName;
    if (m_count > 1) {
        toastText += " x" + std::to_string(m_count);
    }
    ToastNotificationSystem::instance().show(toastText);
}

void ChestComponent::reset()
{
    m_opened = false;
    m_interactEdge.sync(false);
    m_icon.loadAnimation(m_idleTexturePath, CHEST_FRAME_COUNT, CHEST_FRAME_DURATION, true);
    m_glow.setVisible(true);
}

void ChestComponent::markOpenedFromSave()
{
    m_opened = true;
    m_interactEdge.sync(false);
    m_icon.loadAnimation(m_openTexturePath, CHEST_FRAME_COUNT, CHEST_FRAME_DURATION, false);
    m_glow.setVisible(false);
}
