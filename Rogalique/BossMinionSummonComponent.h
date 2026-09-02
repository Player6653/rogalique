#pragma once
#include "IComponent.h"
#include <SFML/System/Time.hpp>
#include <functional>
#include <vector>

class GameObject;

// Раз в summonInterval, если живых миньонов, заспавненных этим компонентом, меньше maxAlive — зовёт spawnMinion
// рядом с текущей позицией босса. GameObject* хранятся так же, как ArenaWaveComponent::m_currentWaveEnemies — не
// владеющие, живость каждый раз перепроверяется через HealthComponent::isDead(), отдельного события "миньон умер"
// нет и не нужно.
class BossMinionSummonComponent : public IComponent {
public:
    BossMinionSummonComponent(sf::Time summonInterval, int maxAlive, float spawnRadius,
        std::function<GameObject*(sf::Vector2f)> spawnMinion);

    void update(sf::Time dt) override;
    void reset() override;

private:
    int countAlive() const;

    sf::Time m_summonInterval;
    sf::Time m_timer;
    int m_maxAlive;
    float m_spawnRadius;
    std::function<GameObject*(sf::Vector2f)> m_spawnMinion;

    std::vector<GameObject*> m_spawned;
};
