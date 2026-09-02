#pragma once
#include "EngineExport.h"
#include "GameObject.h"
#include <functional>
#include <string>

// Летящий снаряд статичный спрайт (без анимации), развёрнутый по направлению полёта, плюс ProjectileComponent,
// который его двигает и сам решает, когда себя уничтожить (см. GameObject::destroy).
class ENGINE_API Projectile : public GameObject {
public:
    // frameCount>1 — texturePath лист с анимацией (полоса кадров подряд по горизонтали, см. SpriteComponent::loadAnimation)
    // вместо статичной текстуры целиком; зациклена (снаряд обычно живёт дольше одного цикла).
    // onImpact — необязательный колбэк (nullptr по умолчанию, как раньше), зовётся с мировой позицией снаряда в
    // момент РЕАЛЬНОГО попадания (в стену или в цель), не на угасании по maxRange — см. ProjectileComponent. Нужен
    // визуальным эффектам вроде спрайта взрыва на попадании (см. Boss.cpp), сам Projectile ничего не рисует, кроме
    // своего тела.
    Projectile(sf::Vector2f position, sf::Vector2f direction, float speed, int damage, float hitRadius, float maxRange,
        const GameObject* ignoreOwner, const std::string& texturePath, sf::Vector2f visualSize, int frameCount = 1,
        sf::Time frameDuration = sf::Time::Zero, std::function<void(sf::Vector2f)> onImpact = nullptr);
};
