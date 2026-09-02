#include "DoorComponent.h"
#include "AudioSystem.h"
#include "FocusedInput.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "InteractionHelpers.h"
#include "InventoryComponent.h"
#include "ItemDefinition.h"
#include "Log.h"
#include "SpriteComponent.h"
#include "ToastNotificationSystem.h"
#include <algorithm>
#include <utility>

namespace
{
    // Крупнее, чем у сундука (44) — дверь физически больше и в 2 тайла шириной, подходить вплотную не обязательно.
    constexpr float OPEN_DISTANCE = 60.f;
} // namespace

DoorComponent::DoorComponent(
    SpriteComponent& icon, std::string closedTexturePath, std::string openTexturePath, std::vector<std::string> requiredKeyIds)
    : m_icon(icon),
      m_closedTexturePath(std::move(closedTexturePath)),
      m_openTexturePath(std::move(openTexturePath)),
      m_requiredKeyIds(std::move(requiredKeyIds))
{
}

void DoorComponent::update(sf::Time)
{
    if (m_open) {
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
    if (!inventory) {
        return;
    }
    for (const std::string& keyId : m_requiredKeyIds) {
        bool has = false;
        for (const InventorySlot& slot : inventory->getBag()) {
            if (!slot.isEmpty() && slot.item->id == keyId) {
                has = true;
                break;
            }
        }
        if (!has) {
            LOG_INFO("Door: заперта, не хватает ключа \"" + keyId + "\"");
            // Отображаемое имя ключа для игрока, а не внутренний id ("north_key") — findItemDefinition() тот же
            // справочник, что и у ItemPickupComponent/ChestComponent.
            const ItemDefinition* keyDef = findItemDefinition(keyId);
            ToastNotificationSystem::instance().show(
                "Дверь заперта — нужен: " + (keyDef ? keyDef->displayName : keyId));
            return;
        }
    }

    m_open = true;
    m_icon.loadTexture(m_openTexturePath);
    AudioSystem::instance().playSound("ui_confirm");
    LOG_INFO("Door: открыта, все " + std::to_string(m_requiredKeyIds.size()) + " ключа собраны");
    ToastNotificationSystem::instance().show("Дверь открыта!");
    if (m_onOpened) {
        m_onOpened();
    }
}

void DoorComponent::reset()
{
    m_open = false;
    m_interactEdge.sync(false);
    m_icon.loadTexture(m_closedTexturePath);
}
