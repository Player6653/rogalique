#pragma once
#include "IComponent.h"
#include <SFML/Graphics.hpp>

class HealthComponent;

// "Фаза ярости": пока HP владельца не выше lowHpFraction от максимума, раз в teleportInterval телепортирует его в
// случайную точку в пределах arenaBounds, с всплеском частиц на старой и новой позиции (см. ParticleSystem) —
// на низком HP босс не просто медленнее убивается, а ещё и сбивает игрока с ритма боя, а не только раньше умирает.
class BossTeleportComponent : public IComponent {
public:
    BossTeleportComponent(HealthComponent& health, float lowHpFraction, sf::Time teleportInterval, sf::FloatRect arenaBounds);

    void update(sf::Time dt) override;
    void reset() override;

    // Тот же одноразовый флаг-событие, что у AttackComponent::consumeJustStarted() — взводится в момент реального
    // телепорта (после успешного поиска проходимой точки, см. .cpp), потребляется ActorAnimationComponent для
    // показа BAT_TELEPORT_EFFECT (см. Boss.cpp) ровно один раз на срабатывание.
    bool consumeJustTeleported()
    {
        bool result = m_justTeleported;
        m_justTeleported = false;
        return result;
    }

private:
    HealthComponent& m_health;
    float m_lowHpFraction;
    sf::Time m_teleportInterval;
    sf::FloatRect m_arenaBounds;
    sf::Time m_timer;
    bool m_justTeleported = false;
};
