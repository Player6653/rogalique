#pragma once
#include "GameObject.h"
#include <string>

// Яма/лава на полу — статичный непроходимый (для обычной ходьбы) объект сцены, как Trap. Блокирует движение
// кинематическим ColliderComponent, но помечен PitComponent (см. Engine) — MovementComponent пропускает сквозь
// него того, кто сейчас прыгает (InputComponent::isJumping()), остальных останавливает как обычную стену.
class Pit : public GameObject {
public:
    // texturePath — картинка растягивается под size целиком (не важны исходные пропорции файла, см. Pit.cpp); по
    // умолчанию пусто — Pit тогда вообще без собственного визуала, просто невидимый непроходимый прямоугольник
    // (виден только пол/декор из Tiled под ним). В Tiled задаётся строковым свойством "texture" у объекта Pit
    // (см. SceneFacade::run()) — любой путь к файлу, относительно рабочей директории игры, как и у остальных
    // текстур в игре.
    explicit Pit(sf::Vector2f position, sf::Vector2f size, const std::string& texturePath = "");
};
