#pragma once
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <string>

class HealthComponent;

// "Крутящийся залп" — редкая специальная атака босса, использующая MULTI/SMALL_PROJECTILE_SPIN_* — для них
// готового боевого тела-ролика в паке нет, только сами снаряды/эффекты. Раз в interval: спавнит две короткие
// вспышки-телеграфы (см. VisualEffect) на позиции босса, ждёт
// channelDelay, затем одним махом выпускает два кольца снарядов по всем направлениям сразу — внутреннее (быстрое,
// SMALL_PROJECTILE_SPIN_ONGOING) и внешнее (MULTI_PROJECTILE_SPIN_ONGOING), с угловым сдвигом между ними, чтобы
// кольца не слипались в одну линию. Не целится — бьёт всей ареной сразу, как и положено "спин-атаке", поэтому не
// зависит от TargetFinder/ChaseTargetComponent, только от того, жив ли сам босс.
class BossSpinBarrageComponent : public IComponent {
public:
    BossSpinBarrageComponent(HealthComponent& health, sf::Time interval, sf::Time channelDelay, int smallRingCount,
        int smallRingDamage, float smallRingSpeed, int multiRingCount, int multiRingDamage, float multiRingSpeed);

    void update(sf::Time dt) override;
    void reset() override;

private:
    void beginChannel();
    void releaseBarrage();

    HealthComponent& m_health;
    sf::Time m_interval;
    sf::Time m_cooldownRemaining;
    sf::Time m_channelDelay;
    sf::Time m_channelRemaining;
    bool m_channeling = false;

    int m_smallRingCount;
    int m_smallRingDamage;
    float m_smallRingSpeed;
    int m_multiRingCount;
    int m_multiRingDamage;
    float m_multiRingSpeed;
};
