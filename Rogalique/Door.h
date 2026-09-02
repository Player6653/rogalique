#pragma once
#include "GameObject.h"
#include <string>
#include <vector>

// Главная дверь в центре Hub (см. DoorComponent.h): открывается по E, только если в мешке игрока есть все ключи
// из requiredKeyIds (id предметов, см. ItemDefinition.cpp). Дальнейшее поведение при открытии (телепорт на арену
// волн) вешается снаружи через getComponent<DoorComponent>()->setOnOpened(...), см. SceneFacade::run().
class Door : public GameObject {
public:
    Door(sf::Vector2f position, std::vector<std::string> requiredKeyIds);
};
