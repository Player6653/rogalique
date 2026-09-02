#pragma once
#include "GameObject.h"
#include "IAnimatedActor.h"
#include <string>

class SpriteComponent;

// Настройки одного экземпляра слизи — три расцветки (skin, см. Resources/Characters/Slime/) с разными механиками:
// Slime1 — обычная (ближний бой по кругу), Slime2 — делитель (canSplit), Slime3 — плевок на расстоянии (isRanged).
struct SlimeConfig {
    std::string skin = "Slime1";
    int maxHp = 2;
    // Множитель на VISUAL_SIZE — только для мелких "детей" деления (см. Slime.cpp), у обычных слизей всегда 1.
    float visualScale = 1.f;
    // При смерти распадается на пару мелких копий той же расцветки (см. SlimeSplitComponent); у самих детей
    // всегда false — иначе цепочка расщепления была бы бесконечной.
    bool canSplit = false;
    // Атака — направленный снаряд (RangedAttackComponent), а не удар по кругу вокруг себя (AttackComponent).
    bool isRanged = false;
    // > 0 — слизь умирает сама, выпустив столько снарядов (см. SlimeShotLimitComponent), а не только от полученного
    // урона; 0 (по умолчанию) — не ограничена. Имеет смысл только при isRanged=true.
    int maxShotsBeforeDeath = 0;
    // Куда добавлять детей при делении — обычно &actors из SceneFacade.cpp, тот же Y-sort контейнер, что и у всех
    // остальных актёров (см. GameWorld::spawnIn). Нужен, только если canSplit=true.
    GameObject* childSpawnParent = nullptr;
};

// Третий тип врага: видит игрока по всем направлениям (не только в конусе, см. EnemyBehaviorComponent
// omnidirectionalVision) — маленький, слабый, но застаёт врасплох с любой стороны. Дерётся по-разному в
// зависимости от SlimeConfig (см. выше), а не просто рескин одной и той же механики под три расцветки.
class Slime : public GameObject, public IAnimatedActor {
public:
    Slime(sf::Vector2f position, sf::Vector2f size, float speed, float detectionRadius, SlimeConfig config = {});

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
