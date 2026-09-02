#pragma once
#include "FocusedInput.h"
#include "IComponent.h"
#include "InputEdge.h"
#include <SFML/Window/Keyboard.hpp>

// По нажатию F или Z пытается атаковать через AttackComponent.
class PlayerAttackComponent : public IComponent {
public:
    void update(sf::Time dt) override;

    // См. WeaponComponent::resyncInput() — та же причина, тот же приём, отдельная клавиша (F/Z).
    void resyncInput()
    {
        m_attackEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::F) || FocusedInput::isKeyPressed(sf::Keyboard::Z));
    }

private:
    KeyEdge m_attackEdge;
};
