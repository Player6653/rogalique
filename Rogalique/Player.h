#pragma once
#include "GameObject.h"

class SpriteComponent;

// Игрок не более чем GameObject, собранный из компонентов-стратегий движка.
class Player : public GameObject {
public:
    Player(sf::Vector2f position, sf::Vector2f size, sf::Vector2f cameraViewSize, float speed = 200.f);

    // PlayerAnimationComponent не может однозначно получить нужный SpriteComponent через getComponent (их несколько), поэтому Player сам отдаёт их по прямой ссылке, полученной при создании.
    SpriteComponent& getBodySprite() const
    {
        return *m_bodySprite;
    }
    SpriteComponent& getShadowSprite() const
    {
        return *m_shadowSprite;
    }
    // Пылевой эффект под рывок/прыжок — отдельным слоем поверх тела, показывается только пока идёт манёвр.
    SpriteComponent& getDustSprite() const
    {
        return *m_dustSprite;
    }

private:
    SpriteComponent* m_shadowSprite = nullptr;
    SpriteComponent* m_bodySprite = nullptr;
    SpriteComponent* m_dustSprite = nullptr;
};
