#pragma once
#include "GameObject.h"

struct ItemDefinition;

// Сундук на карте — статичный объект сцены (как ArrowCrate/Trap), см. ChestComponent для логики открытия по E.
// В отличие от ItemPickup не пропадает после срабатывания — переключается на спрайт "открыт" и остаётся видимым.
class Chest : public GameObject {
public:
    Chest(sf::Vector2f position, const ItemDefinition& item, int count = 1);
};
