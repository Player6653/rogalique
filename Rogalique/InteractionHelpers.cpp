#include "InteractionHelpers.h"
#include "ChaseTargetComponent.h"
#include "FocusedInput.h"
#include "GameObject.h"
#include "InputEdge.h"
#include <cmath>

bool isPlayerInRangeAndInteractPressed(GameObject& owner, float interactDistance, KeyEdge& interactEdge, GameObject** outPlayer)
{
    GameObject* player = findChaseTarget();
    if (!player) {
        interactEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::E));
        return false;
    }
    if (outPlayer) {
        *outPlayer = player;
    }

    sf::Vector2f delta = player->getPosition() - owner.getPosition();
    float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    bool inRange = distance <= interactDistance;
    // poll() зовём каждый кадр, а не только пока в радиусе — см. класс-комментарий в InteractionHelpers.h.
    bool interactPressed = interactEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::E));
    return inRange && interactPressed;
}
