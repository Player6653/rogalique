#pragma once
#include <string>

// Gameplay: явное понятие "текущая локация" (см. docs/DESIGN_DOC.md, направление Gameplay Developer, лёгкий
// уровень — "несколько карт, между которыми можно переходить"). До этого игра уже умела переключаться между
// подземельем и ареной волн (см. SceneFacade.cpp — DoorComponent::setOnOpened переключает camera->setBounds() и
// запускает ArenaWaveComponent, HP/инвентарь при этом не сбрасываются, потому что это один и тот же GameWorld, а
// не отдельная сцена) — этот enum ничего не меняет в самом переключении, просто делает его состояние именованным
// и наблюдаемым (см. HUD в SceneFacade::run() — текущая локация видна прямо в игре), а не подразумеваемым по
// границам камеры.
enum class Location {
    Dungeon,
    Arena
};

inline std::string locationDisplayName(Location location)
{
    switch (location) {
    case Location::Arena:
        return "Арена";
    case Location::Dungeon:
    default:
        return "Подземелье";
    }
}
