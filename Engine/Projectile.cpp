#include "pch.h"
#include "Projectile.h"
#include "ProjectileComponent.h"
#include "SpriteComponent.h"
#include <cmath>

namespace
{
    constexpr float RAD_TO_DEG = 180.f / 3.14159265f;
}

Projectile::Projectile(sf::Vector2f position, sf::Vector2f direction, float speed, int damage, float hitRadius, float maxRange,
    const GameObject* ignoreOwner, const std::string& texturePath, sf::Vector2f visualSize, int frameCount,
    sf::Time frameDuration)
    : GameObject(position)
{
    SpriteComponent& sprite = addComponent<SpriteComponent>(visualSize);
    sprite.setPlaceholderColor(sf::Color(200, 200, 60));
    // Пустой путь — намеренно без текстуры (например, у игрока пока нет своего спрайта пули), тогда остаётся
    // плейсхолдер-прямоугольник заданного цвета/размера, без ложного LOG_WARN "не удалось загрузить".
    if (!texturePath.empty()) {
        if (frameCount > 1) {
            sprite.loadAnimation(texturePath, frameCount, frameDuration, /*loop=*/true);
        } else {
            sprite.loadTexture(texturePath);
        }
    }
    // Текстура нарисована указывающей вправо (0°) — тот же ноль, что и у atan2/sf::Sprite::setRotation.
    sprite.setRotation(std::atan2(direction.y, direction.x) * RAD_TO_DEG);

    addComponent<ProjectileComponent>(direction, speed, damage, hitRadius, maxRange, ignoreOwner);
}
