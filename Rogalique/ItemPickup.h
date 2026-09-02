#pragma once
#include "GameObject.h"

struct ItemDefinition;

// Предмет, лежащий на карте — статичный объект сцены (как ArrowCrate), см. ItemPickupComponent для логики
// подбора/сброса. requiresInteract=false — подбор проходом рядом (мелкие предметы); true — нужен ещё и явный E
// (сундуки/ящики с добычей).
class ItemPickup : public GameObject {
public:
    ItemPickup(sf::Vector2f position, const ItemDefinition& item, int count = 1, bool requiresInteract = false);
};
