#pragma once
#include "IComponent.h"

class HealthComponent;

// Единственная задача — заметить смерть игрока и один раз выставить GameWorld::isGameOver(). Мир НЕ ставим на
// паузу: MovementComponent и так не двигает мёртвого, а AttackComponent пропускает мёртвые цели, так что мир
// безопасно продолжает жить — анимация смерти доигрывает сама (она не зациклена, застынет на последнем кадре).
class PlayerDeathComponent : public IComponent {
public:
    explicit PlayerDeathComponent(HealthComponent* playerHealth);

    void update(sf::Time dt) override;

private:
    HealthComponent* m_playerHealth;
};
