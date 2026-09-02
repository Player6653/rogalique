#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <string>

class HealthComponent;

// Летит по прямой с постоянной скоростью. Раз в кадр проверяет, не оказался ли в hitRadius от живого HealthComponent
// (кроме ignoreOwner — того, кто выстрелил) если да, наносит damage и уничтожает свой GameObject. Точно так же
// уничтожается сам, если пролетел дальше maxRange, ни в кого не попав стены не задевает, снаряд их пролетает насквозь.
class ENGINE_API ProjectileComponent : public IComponent {
public:
    ProjectileComponent(
        sf::Vector2f direction, float speed, int damage, float hitRadius, float maxRange, const GameObject* ignoreOwner);

    void update(sf::Time dt) override;

private:
    sf::Vector2f m_direction;
    float m_speed;
    int m_damage;
    float m_hitRadius;
    float m_maxRange;
    float m_traveled = 0.f;
    const GameObject* m_ignoreOwner;
};
