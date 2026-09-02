#pragma once
#include <SFML/Graphics.hpp>

class GameObject;

// Базовый интерфейс компонента — паттерн Стратегия.
class IComponent {
public:
    virtual ~IComponent() = default;

    virtual void update(sf::Time dt) {}
    virtual void draw(sf::RenderWindow& window) const {}

    // Вызывается GameObject, когда его позиция изменилась.
    virtual void onOwnerMoved(sf::Vector2f newPosition) {}

    // Возвращает компонент к начальному состоянию (HP, кулдауны, таймеры и т.п.) — нужно для рестарта.
    virtual void reset() {}

    // Владелец нужен компонентам вроде MovementComponent, которым надо подвинуть сам GameObject или заглянуть в соседний компонент.
    void setOwner(GameObject* owner)
    {
        m_owner = owner;
    }

protected:
    GameObject* getOwner() const
    {
        return m_owner;
    }

private:
    GameObject* m_owner = nullptr;
};
