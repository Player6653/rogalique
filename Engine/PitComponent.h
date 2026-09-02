#pragma once
#include "EngineExport.h"
#include "IComponent.h"

class GameObject;

// Метка "я яма/лава, через меня можно только прыжком" — вешает на себя объект препятствия (см. Rogalique/Pit)
// вместе с кинематическим ColliderComponent. MovementComponent сам ищет эту метку у владельца чужого коллайдера
// (тем же приёмом, что ChaseComponent ищет ChaseTargetComponent) и пропускает сквозь него того, чей
// IDirectionProvider::ignoresObstacles() сейчас true (см. InputComponent::isJumping()).
class ENGINE_API PitComponent : public IComponent {
public:
    PitComponent();

    GameObject* getOwner() const
    {
        return IComponent::getOwner();
    }
};
