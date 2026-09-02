#pragma once
#include "IComponent.h"
#include <vector>

class GameObject;

// Метка "я ящик со стрелами" — SoldierAmmoComponent и WeaponComponent (у игрока) ищут ближайший ящик через
// getAll() (см. ниже), не полным обходом дерева сцены каждый кадр, как раньше (getComponentsInChildren —
// с разросшимся деревом декора это была лишняя нагрузка каждый кадр, пока боезапас не полон).
//
// Бесконечная станция пополнения — не расходуется и не прячется при использовании (было иначе, но не понравилось:
// пополнял только Soldier, ходить сквозь ящик было можно, и один раз использованный ящик пропадал до следующего
// рестарта уровня). Сейчас это просто метка + физическое препятствие (ColliderComponent на ArrowCrate) — вся
// логика пополнения целиком в SoldierAmmoComponent/WeaponComponent, здесь ничего не хранится.
class ArrowCrateComponent : public IComponent {
public:
    ArrowCrateComponent();
    ~ArrowCrateComponent() override;

    GameObject* getOwner() const
    {
        return IComponent::getOwner();
    }

    // Все живые ArrowCrate — их обычно 1-2 на уровень, полный обход дерева ради этого не нужен.
    static const std::vector<ArrowCrateComponent*>& getAll()
    {
        return s_all;
    }

private:
    static std::vector<ArrowCrateComponent*> s_all;
};
