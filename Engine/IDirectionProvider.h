#pragma once
#include <SFML/System/Vector2.hpp>

// Общий интерфейс для всего, что может подсказать MovementComponent направление движения.
class IDirectionProvider {
public:
    virtual ~IDirectionProvider() = default;
    virtual sf::Vector2f getMoveDirection() const = 0;

    // Куда смотрит объект прямо сейчас — по умолчанию совпадает с направлением движения.
    virtual sf::Vector2f getFacing() const
    {
        return getMoveDirection();
    }

    // Пока это true, MovementComponent применяет направление, даже если владелец сейчас оглушён.
    virtual bool isUninterruptible() const
    {
        return false;
    }

    // Пока это true, MovementComponent пропускает владельца сквозь коллайдеры с PitComponent (ямы/лава) — нужно
    // прыжку (см. InputComponent::isJumping()), чтобы перепрыгивать препятствие, а не тыкаться в него, как в
    // обычную стену. По умолчанию false — ходьба по яме не проходит ни у кого, кроме как во время прыжка.
    virtual bool ignoresObstacles() const
    {
        return false;
    }
};
