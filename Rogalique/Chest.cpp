#include "Chest.h"
#include "ChestComponent.h"
#include "ItemDefinition.h"
#include "Log.h"
#include "PickupGlowComponent.h"
#include "SpriteComponent.h"

namespace
{
    const sf::Vector2f VISUAL_SIZE(48.f, 48.f);
    const std::string IDLE_TEXTURE = "Resources/Map/Chest/Skull_chest_idle.png";
    const std::string OPEN_TEXTURE = "Resources/Map/Chest/Skull_chest_open.png";
    // Крупнее, чем у мелких предметов (см. ItemPickup.cpp) — сундук и физически больше.
    constexpr float GLOW_RADIUS = 30.f;
} // namespace

Chest::Chest(sf::Vector2f position, const ItemDefinition& item, int count)
    : GameObject(position)
{
    // Раньше иконки — см. тот же порядок в ItemPickup.cpp и почему.
    PickupGlowComponent& glow = addComponent<PickupGlowComponent>(GLOW_RADIUS);

    SpriteComponent& icon = addComponent<SpriteComponent>(VISUAL_SIZE);
    icon.setPlaceholderColor(sf::Color(90, 70, 40));
    // Кадры/loop настраивает сам ChestComponent (см. reset()) — тут только самая первая загрузка на старте.
    icon.loadAnimation(IDLE_TEXTURE, 12, sf::seconds(0.1f), true);

    addComponent<ChestComponent>(item, count, icon, glow, IDLE_TEXTURE, OPEN_TEXTURE);

    LOG_INFO("Chest: \"" + item.displayName + "\" x" + std::to_string(count) + " на позиции (" + std::to_string(position.x) + ", "
             + std::to_string(position.y) + ")");
}
