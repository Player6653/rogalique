#include "ArrowCrate.h"
#include "ArrowCrateComponent.h"
#include "ColliderComponent.h"
#include "Log.h"
#include "SpriteComponent.h"

namespace
{
    const sf::Vector2f VISUAL_SIZE(40.f, 40.f);
}

ArrowCrate::ArrowCrate(sf::Vector2f position)
    : GameObject(position)
{
    SpriteComponent& sprite = addComponent<SpriteComponent>(VISUAL_SIZE);
    sprite.setPlaceholderColor(sf::Color(150, 100, 60));
    sprite.loadTexture("Resources/Map/boxes/box_01.png");

    // Кинематический — физический ящик, сквозь него нельзя пройти (раньше можно было).
    addComponent<ColliderComponent>(VISUAL_SIZE, true);
    addComponent<ArrowCrateComponent>();

    LOG_INFO("ArrowCrate создан на позиции (" + std::to_string(position.x) + ", " + std::to_string(position.y) + ")");
}
