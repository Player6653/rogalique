#pragma once
#include "EngineExport.h"
#include "IComponent.h"

class GameObject;

// Метка "меня можно преследовать" — вешает на себя игрок. ChaseComponent сам ищет объект с этой меткой в дереве
// сцены (GameWorld), а не получает цель напрямую от игрового проекта.
class ENGINE_API ChaseTargetComponent : public IComponent {
public:
    ChaseTargetComponent();
    // Снимает себя с реестра (см. findChaseTarget() ниже) — тот же приём, что у HealthComponent/CameraComponent.
    ~ChaseTargetComponent() override;

    GameObject* getOwner() const
    {
        return IComponent::getOwner();
    }
};

// Текущий (и на практике единственный) объект с ChaseTargetComponent в дереве сцены — то есть игрок — или nullptr,
// если такого нет. Общий приём "где игрок", которым независимо друг от друга пользовались ChestComponent/
// DoorComponent/ItemPickupComponent/TrapComponent (Rogalique) — вынесен сюда одним местом, чтобы починка или
// замена этого поиска не требовала правки в четырёх файлах разом. Раньше искал полным обходом дерева сцены
// (getComponentsInChildren) — вызывается КАЖДЫЙ кадр из ItemPickupComponent (по разу на каждый ещё не подобранный
// предмет на карте, их бывает по 15-18) и из Chest/Door/Trap, так что с разросшимся деревом декора это была
// главная причина просадки FPS в бою, а не A*-путь ботов. Теперь — простой реестр:
// O(1), а не O(размер дерева) на каждый такой вызов.
ENGINE_API GameObject* findChaseTarget();
