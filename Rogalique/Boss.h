#pragma once
#include "GameObject.h"
#include "IAnimatedActor.h"
#include <functional>

class SpriteComponent;

// Gameplay: босс арены (см. docs/DESIGN_DOC.md, направление Gameplay Developer, средний уровень). Композиция теми
// же компонентами, что и у Enemy/Soldier/Slime (см. «Компоненты вместо наследования» в docs/DOCUMENTATION.md) — не
// подкласс какого-либо из них, самостоятельный GameObject: HP заметно больше обычного врага, бьёт и ближним, и
// дальним одновременно (как Slime3), периодически зовёт подкрепление (см. BossMinionSummonComponent). Спрайты —
// купленный пак "The Vampire Lord & Spawns" (@ElectricLemon, см. титры) — та же схема IAnimatedActor+
// ActorAnimationComponent, что у Enemy/Soldier/Slime, один и тот же ряд листа (row=0, авто-флип по X через
// ActorAnimationComponent берёт на себя лево/право, отдельного "смотрит вверх/вниз" у этих существ и так нет).
class Boss : public GameObject, public IAnimatedActor {
public:
    // spawnMinion — фабрика подкрепления (см. BossMinionSummonComponent.h), тем же приёмом внешней инъекции, что и
    // spawnEnemy у ArenaWaveComponent: сам Boss не знает, как строить миньона, просто дирижирует, когда звать помощь.
    Boss(sf::Vector2f position, sf::Vector2f size, float speed, float detectionRadius,
        std::function<GameObject*(sf::Vector2f)> spawnMinion);

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
