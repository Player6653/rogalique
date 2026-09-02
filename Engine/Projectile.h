#pragma once
#include "EngineExport.h"
#include "GameObject.h"
#include <string>

// Летящий снаряд статичный спрайт (без анимации), развёрнутый по направлению полёта, плюс ProjectileComponent,
// который его двигает и сам решает, когда себя уничтожить (см. GameObject::destroy).
class ENGINE_API Projectile : public GameObject {
public:
    // frameCount>1 — texturePath лист с анимацией (полоса кадров подряд по горизонтали, см. SpriteComponent::loadAnimation)
    // вместо статичной текстуры целиком; зациклена (снаряд обычно живёт дольше одного цикла).
    Projectile(sf::Vector2f position, sf::Vector2f direction, float speed, int damage, float hitRadius, float maxRange,
        const GameObject* ignoreOwner, const std::string& texturePath, sf::Vector2f visualSize, int frameCount = 1,
        sf::Time frameDuration = sf::Time::Zero);
};
