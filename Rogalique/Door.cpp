#include "Door.h"
#include "DoorComponent.h"
#include "Log.h"
#include "SpriteComponent.h"

namespace
{
    // Doble_door_front_wall.png (768x96, 8 кадров 96x96) — вырезаны кадр 0 (закрыта) и кадр 3 (створки открыты
    // нараспашку) в отдельные файлы, см. Resources/Map/Doors/main_door_*.png.
    const sf::Vector2f VISUAL_SIZE(96.f, 96.f);
    const std::string CLOSED_TEXTURE = "Resources/Map/Doors/main_door_closed.png";
    const std::string OPEN_TEXTURE = "Resources/Map/Doors/main_door_open.png";
} // namespace

Door::Door(sf::Vector2f position, std::vector<std::string> requiredKeyIds)
    : GameObject(position)
{
    SpriteComponent& icon = addComponent<SpriteComponent>(VISUAL_SIZE);
    icon.setPlaceholderColor(sf::Color(90, 70, 110));
    icon.loadTexture(CLOSED_TEXTURE);

    addComponent<DoorComponent>(icon, CLOSED_TEXTURE, OPEN_TEXTURE, std::move(requiredKeyIds));

    LOG_INFO("Door: главная дверь на позиции (" + std::to_string(position.x) + ", " + std::to_string(position.y) + ")");
}
