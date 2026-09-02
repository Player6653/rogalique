#pragma once
#include "GameObject.h"

// Шип-ловушка на полу — статичный объект сцены (как ArrowCrate), см. TrapComponent для логики урона по циклу
// выдвижения. В отличие от ItemPickup не "подбирается" и не прячется — постоянная часть планировки уровня (см.
// RoomLayout::trapTiles в SceneFacade.cpp).
class Trap : public GameObject {
public:
    explicit Trap(sf::Vector2f position);
};
