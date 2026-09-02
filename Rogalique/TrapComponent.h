#pragma once
#include "IComponent.h"

// Шип-ловушка на полу: анимация (см. Trap.cpp) крутится по кругу сама через SpriteComponent, а этот компонент
// параллельно считает то же самое время, чтобы знать текущую фазу — бьёт игрока, только пока шипы выдвинуты
// (последняя часть цикла), не весь цикл целиком (первые кадры — убранные точки, безопасно наступить).
class TrapComponent : public IComponent {
public:
    // cycleDuration — полная длина ролика (frameCount*frameDuration, см. Trap.cpp); dangerousPhaseStart — с какого
    // момента внутри цикла шипы считаются выдвинутыми (и до конца цикла).
    TrapComponent(sf::Time cycleDuration, sf::Time dangerousPhaseStart);

    void update(sf::Time dt) override;
    void reset() override;

private:
    sf::Time m_cycleDuration;
    sf::Time m_dangerousPhaseStart;
    sf::Time m_cycleElapsed;
    // Отдельный от анимации кулдаун урона — иначе стоящий на шипе игрок терял бы HP каждый кадр, пока фаза
    // "выдвинут" длится.
    sf::Time m_damageCooldownRemaining;
};
