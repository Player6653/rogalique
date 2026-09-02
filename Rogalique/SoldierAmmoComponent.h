#pragma once
#include "IComponent.h"

class ChaseComponent;
class RangedAttackComponent;

// Стрелы у Soldier не бесконечные. Пока они есть — стреляет как обычно (сам вызывает RangedAttackComponent::tryShoot(),
// тот теперь пассивен, autoFire=false). Кончились — ищет ближайший ArrowCrate в радиусе поиска и идёт к нему через
// ChaseComponent::setSeekOverride(); если ящика поблизости нет — вместо этого подходит вплотную к цели и бьётся
// врукопашную (обычный ближний AttackComponent сам сработает, как только окажется в его радиусе).
class SoldierAmmoComponent : public IComponent {
public:
    explicit SoldierAmmoComponent(int maxArrows);

    void update(sf::Time dt) override;
    void reset() override
    {
        m_arrows = m_maxArrows;
    }

    int getArrows() const
    {
        return m_arrows;
    }
    // Только для восстановления сейва (см. SceneFacade) — не боевая логика, обычный расход патронов идёт через
    // update()/tryShoot(). Прижимает к [0, maxArrows] на случай значения из внешнего файла.
    void setArrows(int arrows);

private:
    int m_maxArrows;
    int m_arrows;
};
