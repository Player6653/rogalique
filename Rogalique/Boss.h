#pragma once
#include "GameObject.h"
#include <functional>

// Gameplay: босс арены (см. docs/DESIGN_DOC.md, направление Gameplay Developer, средний уровень) — заглушка на
// существующих текстурах с плейсхолдер-цветом (реальный спрайт-лист "Vampire Lord" ещё не куплен, см. диздок,
// раздел 6). Композиция теми же компонентами, что и у Enemy/Soldier/Slime (см. «Компоненты вместо наследования» в
// docs/DOCUMENTATION.md) — не подкласс какого-либо из них, самостоятельный GameObject: HP заметно больше обычного
// врага, бьёт и ближним, и дальним одновременно (как Slime3), периодически зовёт подкрепление (см.
// BossMinionSummonComponent). Без ActorAnimationComponent — плейсхолдеру нечего анимировать, статичный цветной
// прямоугольник крупнее обычных врагов; когда появится настоящий спрайт-лист, сюда добавится тот же
// IAnimatedActor+ActorAnimationConfig, что и у остальных существ.
class Boss : public GameObject {
public:
    // spawnMinion — фабрика подкрепления (см. BossMinionSummonComponent.h), тем же приёмом внешней инъекции, что и
    // spawnEnemy у ArenaWaveComponent: сам Boss не знает, как строить Slime, просто дирижирует, когда звать помощь.
    Boss(sf::Vector2f position, sf::Vector2f size, float speed, float detectionRadius,
        std::function<GameObject*(sf::Vector2f)> spawnMinion);
};
