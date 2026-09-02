#pragma once
#include "GameObject.h"
#include "IAnimatedActor.h"

class SpriteComponent;

// Враг преследует ближайший объект с меткой ChaseTargetComponent (обычно игрока).
class Enemy : public GameObject, public IAnimatedActor {
public:
    Enemy(sf::Vector2f position, sf::Vector2f size, float speed, float detectionRadius);

    // ActorAnimationComponent не может однозначно получить нужный SpriteComponent через getComponent (их два) —
    // достаёт оба через этот интерфейс, тем же приёмом, что и Player со своими собственными спрайтами.
    SpriteComponent& getBodySprite() const override
    {
        return *m_bodySprite;
    }
    SpriteComponent& getShadowSprite() const override
    {
        return *m_shadowSprite;
    }

private:
    SpriteComponent* m_shadowSprite = nullptr;
    SpriteComponent* m_bodySprite = nullptr;
};
