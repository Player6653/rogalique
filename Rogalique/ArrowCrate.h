#pragma once
#include "GameObject.h"

// Ящик со стрелами — статичный объект на сцене. SoldierAmmoComponent находит его по ArrowCrateComponent, идёт к
// нему, когда у Soldier кончились стрелы, и подбирает (см. destroy() в SoldierAmmoComponent — одноразовый).
class ArrowCrate : public GameObject {
public:
    explicit ArrowCrate(sf::Vector2f position);
};
