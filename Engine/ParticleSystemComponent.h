#pragma once
#include "EngineExport.h"
#include "IComponent.h"

// Тикает/рисует ParticleSystem (см. ParticleSystem.h — там сама логика и очередь). Ровно один экземпляр, добавлен
// в МИРОВОЕ дерево сцены (actorsContainer или root), не в UI — частицы в мировых координатах, должны двигаться
// вместе с камерой при её панорамировании, в отличие от тостов/HUD.
class ENGINE_API ParticleSystemComponent : public IComponent {
public:
    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;
};
