#include "pch.h"
#include "ColliderComponent.h"
#include "GameObject.h"
#include "GameWorld.h"

ColliderComponent::ColliderComponent(sf::Vector2f size, bool isKinematic)
    : m_size(size),
      m_isKinematic(isKinematic)
{
    GameWorld::instance().registerCollider(this);
}

ColliderComponent::~ColliderComponent()
{
    GameWorld::instance().unregisterCollider(this);
}

sf::FloatRect ColliderComponent::getBounds() const
{
    GameObject* owner = getOwner();
    sf::Vector2f position = owner ? owner->getPosition() : sf::Vector2f(0.f, 0.f);
    return getBoundsAt(position);
}

sf::FloatRect ColliderComponent::getBoundsAt(sf::Vector2f position) const
{
    return sf::FloatRect(position.x - m_size.x / 2.f, position.y - m_size.y / 2.f, m_size.x, m_size.y);
}
