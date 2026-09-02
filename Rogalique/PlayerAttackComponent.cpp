#include "PlayerAttackComponent.h"
#include "FocusedInput.h"
#include "GameObject.h"
#include "WeaponComponent.h"

void PlayerAttackComponent::update(sf::Time dt)
{
    bool attackKeyHeld = FocusedInput::isKeyPressed(sf::Keyboard::F) || FocusedInput::isKeyPressed(sf::Keyboard::Z)
                         || FocusedInput::isButtonPressed(sf::Mouse::Left);
    bool attackKeyPressedThisFrame = m_attackEdge.poll(attackKeyHeld);

    if (!attackKeyPressedThisFrame) {
        return;
    }

    GameObject* owner = getOwner();
    auto* weapon = owner ? owner->getComponent<WeaponComponent>() : nullptr;
    if (weapon) {
        weapon->tryAttack();
    }
}
