#include "pch.h"
#include "ParticleSystemComponent.h"
#include "ParticleSystem.h"

void ParticleSystemComponent::update(sf::Time dt)
{
    ParticleSystem::instance().update(dt);
}

void ParticleSystemComponent::draw(sf::RenderWindow& window) const
{
    ParticleSystem::instance().draw(window);
}
