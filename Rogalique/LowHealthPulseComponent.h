#pragma once
#include "IComponent.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Time.hpp>

class HealthComponent;
class SpriteComponent;

// Пока HP владельца не выше lowHpThreshold — тело мигает между обычным цветом и красным с периодом blinkInterval,
// предупреждая "мало здоровья" не дожидаясь следующего удара — в отличие от HitFlashComponent, тот реагирует
// только на сам момент попадания/стана и коротким импульсом, а не постоянно, пока HP низкое. Оба независимо зовут
// SpriteComponent::setColor() на одном и том же спрайте — если оба состояния совпали (низкое HP и только что
// оглушён), кадр-два поборются за цвет, это не баг, просто нижний приоритет неважного визуального нюанса.
class LowHealthPulseComponent : public IComponent {
public:
    LowHealthPulseComponent(HealthComponent& health, SpriteComponent& sprite, int lowHpThreshold, sf::Time blinkInterval);

    void update(sf::Time dt) override;
    void reset() override;

private:
    HealthComponent& m_health;
    SpriteComponent& m_sprite;
    int m_lowHpThreshold;
    sf::Time m_blinkInterval;
    sf::Time m_blinkTimer;
    bool m_flashOn = false;
};
