#include "ItemPickupComponent.h"
#include "AudioSystem.h"
#include "ChaseTargetComponent.h"
#include "FocusedInput.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "InventoryComponent.h"
#include "Log.h"
#include "PickupGlowComponent.h"
#include "SpriteComponent.h"
#include "ToastNotificationSystem.h"
#include <cmath>

namespace
{
    // Чуть меньше тайла (TILE_SIZE=48, см. SceneFacade) — подбор срабатывает, когда игрок реально наступил на
    // клетку с предметом, а не просто прошёл мимо соседней.
    constexpr float PICKUP_DISTANCE = 36.f;

    // Категории, у которых лишний экземпляр ничего не даёт: Ring/Boots открывают способность разово (см.
    // InventoryComponent::recomputeEquipmentEffects — Спринт/Рывок), Neck вообще ни на что не влияет, Weapon
    // (арбалет) просто отпирает пистолет — надеть второй такой же предмет означало бы просто вернуть первый в
    // мешок без какой-либо выгоды. В отличие от них Shield/Chest/Pants/Head — расходуемая прочность (см.
    // ItemDefinition::durability), там запасной экземпляр реально полезен (замена сломанному), дедуп их не трогает.
    bool isDedupCategory(ItemCategory category)
    {
        return category == ItemCategory::Ring || category == ItemCategory::Boots || category == ItemCategory::Neck
               || category == ItemCategory::Weapon;
    }
} // namespace

ItemPickupComponent::ItemPickupComponent(
    const ItemDefinition& item, int count, SpriteComponent& icon, PickupGlowComponent& glow, bool requiresInteract)
    : m_item(item),
      m_count(count),
      m_icon(icon),
      m_glow(glow),
      m_requiresInteract(requiresInteract)
{
}

void ItemPickupComponent::update(sf::Time)
{
    if (m_collected) {
        m_interactEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::E));
        return;
    }

    GameObject* owner = getOwner();
    if (!owner) {
        return;
    }

    GameObject* player = findChaseTarget();
    if (!player) {
        m_interactEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::E));
        return;
    }

    sf::Vector2f delta = player->getPosition() - owner->getPosition();
    float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    bool inRange = distance <= PICKUP_DISTANCE;

    if (m_requiresInteract) {
        // poll() зовём каждый кадр (а не только пока в радиусе) — иначе E, зажатая ещё на подходе, засчиталась бы
        // "только что нажали" в первый же кадр, когда игрок реально войдёт в радиус (тот же класс бага, что чинит
        // KeyEdge::sync() в других местах, например MenuOverlayComponent::refreshInputHeldFlags()) — тут же саму
        // проверку "только что нажали" достаточно просто игнорировать, пока не в радиусе.
        bool interactPressed = m_interactEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::E));
        if (!inRange || !interactPressed) {
            return;
        }
    } else if (!inRange) {
        return;
    }

    auto* inventory = player->getComponent<InventoryComponent>();
    if (!inventory || !inventory->addItem(m_item, m_count)) {
        return; // Мешок полон — предмет остаётся на месте, подобрать можно будет позже, когда освободится место.
    }

    // SpriteComponent всегда что-то рисует, явного "скрыт" у него нет — прячем через альфу 0 (тот же приём, что
    // и у ArrowCrateComponent::consume()); заодно гасит и m_glow (см. setCollected()).
    setCollected(true);
    // Звуковой отклик на подбор — переиспользуем уже загруженный "ui_confirm" (см. SceneFacade.cpp), отдельного
    // звука под предметы пока нет, а этот приятный "дзынь" подходит и здесь.
    AudioSystem::instance().playSound("ui_confirm");
    LOG_INFO("ItemPickup: подобрано \"" + m_item.displayName + "\" x" + std::to_string(m_count));
    // Всплывающее уведомление в HUD (см. ToastNotificationSystem.h) — количество дописываем, только если больше
    // одного, "Подобрано: Стрела x5" информативнее, чем "x1" на каждое единичное зелье.
    std::string toastText = "Подобрано: " + m_item.displayName;
    if (m_count > 1) {
        toastText += " x" + std::to_string(m_count);
    }
    ToastNotificationSystem::instance().show(toastText);

    // Дедуп: нашёл один экземпляр категории вроде Кольца/Сапог — остальные такие же на карте больше не нужны,
    // прячем их тоже, чтобы игрок не бегал за бесполезными дублями (см. isDedupCategory выше, почему именно эти).
    if (isDedupCategory(m_item.category)) {
        for (ItemPickupComponent* other : GameWorld::instance().getRoot().getComponentsInChildren<ItemPickupComponent>()) {
            if (other != this && !other->isCollected() && other->m_item.category == m_item.category) {
                other->setCollected(true);
            }
        }
    }
}

void ItemPickupComponent::reset()
{
    setCollected(false);
}

void ItemPickupComponent::setCollected(bool collected)
{
    m_collected = collected;
    if (collected) {
        m_icon.setColor(sf::Color(255, 255, 255, 0));
    } else {
        m_icon.clearColor();
    }
    m_glow.setVisible(!collected);
}
