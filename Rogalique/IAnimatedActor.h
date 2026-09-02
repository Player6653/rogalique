#pragma once

class SpriteComponent;

// Общий интерфейс существ с телом+тенью как отдельными SpriteComponent (Enemy/Soldier/Slime) — ActorAnimationComponent
// достаёт через него нужные спрайты одним dynamic_cast, не завязываясь на конкретный класс.
class IAnimatedActor {
public:
    virtual ~IAnimatedActor() = default;

    virtual SpriteComponent& getBodySprite() const = 0;
    virtual SpriteComponent& getShadowSprite() const = 0;
};
