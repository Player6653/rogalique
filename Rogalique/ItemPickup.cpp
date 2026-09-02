#include "ItemPickup.h"
#include "ItemDefinition.h"
#include "ItemPickupComponent.h"
#include "Log.h"
#include "PickupGlowComponent.h"
#include "SpriteComponent.h"

namespace
{
    const sf::Vector2f VISUAL_SIZE(36.f, 36.f);
    // Заметно больше самой иконки (18px) — маркер должен выпирать за её края, иначе потеряется на фоне иконки же.
    constexpr float GLOW_RADIUS = 22.f;
} // namespace

ItemPickup::ItemPickup(sf::Vector2f position, const ItemDefinition& item, int count, bool requiresInteract)
    : GameObject(position)
{
    // Раньше иконки — компоненты рисуются в порядке добавления (см. GameObject::draw()), маркер должен лечь ПОД
    // иконку, не поверх неё.
    PickupGlowComponent& glow = addComponent<PickupGlowComponent>(GLOW_RADIUS);

    SpriteComponent& icon = addComponent<SpriteComponent>(VISUAL_SIZE);
    icon.setPlaceholderColor(sf::Color(230, 200, 90));
    icon.loadAnimation(item.iconPath, item.iconFrameCount, item.iconFrameDuration, true, 0, 1, true);

    addComponent<ItemPickupComponent>(item, count, icon, glow, requiresInteract);

    LOG_INFO("ItemPickup: \"" + item.displayName + "\" x" + std::to_string(count) + " на позиции (" + std::to_string(position.x)
             + ", " + std::to_string(position.y) + ")");
}
