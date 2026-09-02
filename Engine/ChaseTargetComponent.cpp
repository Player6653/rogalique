#include "pch.h"
#include "ChaseTargetComponent.h"
#include "GameObject.h"

namespace
{
    // Единственный экземпляр на практике (метка вешается только на игрока) — простой указатель, не вектор, тем же
    // приёмом, что и у GameWorld::registerCamera (см. .h почему).
    ChaseTargetComponent* g_activeTarget = nullptr;
} // namespace

ChaseTargetComponent::ChaseTargetComponent()
{
    g_activeTarget = this;
}

ChaseTargetComponent::~ChaseTargetComponent()
{
    if (g_activeTarget == this) {
        g_activeTarget = nullptr;
    }
}

GameObject* findChaseTarget()
{
    return g_activeTarget ? g_activeTarget->getOwner() : nullptr;
}
