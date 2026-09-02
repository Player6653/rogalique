#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>

class SpriteComponent;

// Следит за соседним HealthComponent — как только тот переходит в стан, подмешивает flashColor в sprite, мигая
// каждые blinkInterval.
class ENGINE_API HitFlashComponent : public IComponent {
public:
    HitFlashComponent(SpriteComponent& sprite, sf::Time flashDuration, sf::Time blinkInterval, sf::Color flashColor);

    void update(sf::Time dt) override;
    void reset() override;

private:
    SpriteComponent& m_sprite;
    sf::Time m_flashDuration;
    sf::Time m_blinkInterval;
    sf::Color m_flashColor;

    sf::Time m_flashTimeRemaining;
    sf::Time m_blinkTimer;
    bool m_wasStunned = false;
    bool m_flashOn = false;
};
