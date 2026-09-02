#pragma once
#include "IComponent.h"

class HealthComponent;

// Один раз, в момент когда HealthComponent владельца переходит в isDead()==true, поднимает всплеск частиц в точке
// смерти (см. ParticleSystem) — общий компонент на Enemy/Soldier/Slime/Boss, а не дублированный код в каждом из
// них по отдельности (тот же приём, что и у HealthChangeFeedbackComponent для игрока, только тут без камеры).
class DeathParticleComponent : public IComponent {
public:
    explicit DeathParticleComponent(HealthComponent& health);

    void update(sf::Time dt) override;
    void reset() override;

private:
    HealthComponent& m_health;
    bool m_wasDead = false;
};
