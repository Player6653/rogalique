#pragma once
#include "FocusedInput.h"
#include "IComponent.h"
#include "InputEdge.h"
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

// По нажатию F, Z или ЛКМ зовёт WeaponComponent::tryAttack() — тот сам решает, копьё это или пистолет.
class PlayerAttackComponent : public IComponent {
public:
    void update(sf::Time dt) override;

    // См. WeaponComponent::resyncInput() — та же причина, тот же приём, отдельные клавиши (F/Z/ЛКМ).
    void resyncInput()
    {
        m_attackEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::F) || FocusedInput::isKeyPressed(sf::Keyboard::Z)
                           || FocusedInput::isButtonPressed(sf::Mouse::Left));
    }

private:
    KeyEdge m_attackEdge;
};
