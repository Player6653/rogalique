#pragma once
#include "GameObject.h"
#include "IAnimatedActor.h"

class SpriteComponent;

// Второй тип врага: преследует цель как Enemy, но на дистанции бьёт из лука (RangedAttackComponent), а вплотную
// переходит на меч (обычный AttackComponent, как у Enemy) — приоритет анимаций собирается в ActorAnimationConfig
// прямо в Soldier.cpp.
class Soldier : public GameObject, public IAnimatedActor {
public:
    Soldier(sf::Vector2f position, sf::Vector2f size, float speed, float detectionRadius);

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
