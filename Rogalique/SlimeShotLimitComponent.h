#pragma once
#include "IComponent.h"

// Slime3 не бессмертна в дальнем бою — "выдыхается" и умирает сама, расстреляв maxShots снарядов, а не только от
// полученного урона (см. SlimeConfig::maxShotsBeforeDeath, Slime.cpp). notifyShotFired() зовётся из той же lambda
// в Slime.cpp, что уже дёргает RangedAttackComponent::consumeJustFired() для анимации — САМ этот компонент его не
// читает: consumeJustFired() "съедает" флаг за один вызов, второй читатель в том же кадре увидел бы уже false.
class SlimeShotLimitComponent : public IComponent {
public:
    explicit SlimeShotLimitComponent(int maxShots);

    void update(sf::Time dt) override;
    void reset() override
    {
        m_shotsFired = 0;
    }

    void notifyShotFired()
    {
        ++m_shotsFired;
    }
    int getShotsFired() const
    {
        return m_shotsFired;
    }
    int getMaxShots() const
    {
        return m_maxShots;
    }
    // Только для восстановления сейва (см. SceneFacade) — если сохранённое значение уже >= maxShots, следующий
    // update() тут же убьёт слизь, как если бы она сама успела дострелять до сохранения.
    void setShotsFired(int shots)
    {
        m_shotsFired = shots;
    }

private:
    int m_maxShots;
    int m_shotsFired = 0;
};
