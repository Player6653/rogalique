#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <functional>

// Простая HUD-полоска "доля от 0 до 1" — тёмный фон + цветная заливка, ширина заливки = getFraction() * size.x.
// В отличие от HealthBarComponent не привязана к HealthComponent и не рисует сегментами по числу HP — подходит
// для любого непрерывного ресурса (например, стамина, см. InputComponent::getStamina()). getFraction сам должен
// вернуть уже поделенное на максимум значение [0..1] — компонент ничего не знает о том, что именно измеряет.
class ENGINE_API FractionBarComponent : public IComponent {
public:
    FractionBarComponent(std::function<float()> getFraction, sf::Vector2f size, sf::Color backgroundColor,
        sf::Color fillColor, std::function<bool()> isVisible = nullptr);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;
    // Origin — левый верхний угол (как у HealthBarComponent), не центр — для HUD так удобнее раскладывать по экрану.
    void onOwnerMoved(sf::Vector2f newPosition) override;

private:
    std::function<float()> m_getFraction;
    std::function<bool()> m_isVisible;
    sf::Vector2f m_size;
    sf::RectangleShape m_background;
    sf::RectangleShape m_fill;
};
