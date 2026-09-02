#include "pch.h"
#include "GameWorld.h"
#include "ColliderComponent.h"
#include <algorithm>

GameWorld& GameWorld::instance()
{
    static GameWorld instance;
    return instance;
}

void GameWorld::registerCollider(ColliderComponent* collider)
{
    m_colliders.push_back(collider);
    if (!collider->isKinematic()) {
        m_dynamicColliders.push_back(collider);
    }
}

void GameWorld::unregisterCollider(ColliderComponent* collider)
{
    m_colliders.erase(std::remove(m_colliders.begin(), m_colliders.end(), collider), m_colliders.end());
    m_dynamicColliders.erase(
        std::remove(m_dynamicColliders.begin(), m_dynamicColliders.end(), collider), m_dynamicColliders.end());
}
