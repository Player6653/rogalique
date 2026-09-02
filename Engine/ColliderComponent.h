#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>

// Стратегия столкновений — прямоугольная область вокруг владельца.

class ENGINE_API ColliderComponent : public IComponent {
public:
    ColliderComponent(sf::Vector2f size, bool isKinematic);
    // Снимает себя с реестра GameWorld (см. GameWorld::registerCollider) — без этого MovementComponent мог бы столкнуться с уже уничтоженным объектом на следующем кадре.
    ~ColliderComponent() override;

    // Границы объекта в его текущей позиции.
    sf::FloatRect getBounds() const;
    // Границы объекта, если бы он находился в другой точке — нужно MovementComponent'у, чтобы проверить позицию ДО реального перемещения.
    sf::FloatRect getBoundsAt(sf::Vector2f position) const;

    bool isKinematic() const
    {
        return m_isKinematic;
    }

    // Публичный доступ к владельцу нужен MovementComponent, чтобы у чужого коллайдера найти его HealthComponent и пропустить труп (isDead()==true) сквозь себя тот же приём, что и у HealthComponent::getOwner().
    GameObject* getOwner() const
    {
        return IComponent::getOwner();
    }

private:
    sf::Vector2f m_size;
    bool m_isKinematic;
};
