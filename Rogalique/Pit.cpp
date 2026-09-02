#include "Pit.h"
#include "ColliderComponent.h"
#include "PitComponent.h"
#include "SpriteComponent.h"

Pit::Pit(sf::Vector2f position, sf::Vector2f size, const std::string& texturePath)
    : GameObject(position)
{
    // Пустой texturePath (по умолчанию) — совсем без SpriteComponent, никакого визуала поверх пола/декора из
    // Tiled под ямой (и даже не заглушка-плейсхолдер, см. SpriteComponent::draw() — та рисуется, только если
    // addComponent<SpriteComponent> вообще был вызван).
    if (!texturePath.empty()) {
        SpriteComponent& sprite = addComponent<SpriteComponent>(size);
        sprite.setPlaceholderColor(sf::Color(120, 30, 20));
        // Статичная картинка (не полоса кадров) — грузится как один кадр и растягивается под size, как и любая
        // другая текстура (см. SpriteComponent::loadAnimation): исходные пропорции файла значения не имеют.
        sprite.loadTexture(texturePath);
    }

    // Кинематический — блокирует, как стена, но помечен PitComponent, а не просто ColliderComponent сам по себе:
    // MovementComponent отдельно ищет эту метку, чтобы во время прыжка пропустить сквозь именно яму/лаву, а не
    // сквозь любое препятствие подряд.
    addComponent<ColliderComponent>(size, true);
    addComponent<PitComponent>();
}
