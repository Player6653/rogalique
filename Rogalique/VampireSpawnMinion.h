#pragma once
#include "GameObject.h"
#include "IAnimatedActor.h"

class SpriteComponent;

// Подкрепление босса (см. Boss.h/BossMinionSummonComponent) — та же тема "Vampire Lord & Spawns" (@ElectricLemon,
// см. титры), что и у самого босса, вместо нейтрального слизня-плейсхолдера. Простой ближний боец, композиция как
// у Enemy — самостоятельный GameObject, не подкласс Boss/Enemy.
class VampireSpawnMinion : public GameObject, public IAnimatedActor {
public:
    // feminine выбирает одну из двух расцветок пака (Masc./Fem. Vampire Spawn) — чисто визуально, на механику не
    // влияет; вызывающий код (см. SceneFacade.cpp) рандомизирует её на каждый спавн ради разнообразия на арене.
    VampireSpawnMinion(sf::Vector2f position, sf::Vector2f size, float speed, float detectionRadius, bool feminine);

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
