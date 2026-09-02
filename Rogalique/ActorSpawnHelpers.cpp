#include "ActorSpawnHelpers.h"
#include "GameException.h"
#include "GameObject.h"
#include "HealthComponent.h"
#include "Log.h"

HealthComponent& addHealthComponentWithFallback(GameObject& owner, int maxHp, int armor, const std::string& classLabel)
{
    try {
        return owner.addComponent<HealthComponent>(maxHp, armor);
    } catch (const GameException& e) {
        LOG_ERROR(classLabel + ": некорректные HP/броня, использую значения по умолчанию (1/0): " + e.what());
        return owner.addComponent<HealthComponent>(1, 0);
    }
}
