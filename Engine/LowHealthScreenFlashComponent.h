#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <functional>

class HealthComponent;

// Пока HP владельца не выше lowHpThreshold — красное виньетирование (сгущается к краям экрана, центр чистый)
// плавно пульсирует поверх HUD (синусоида, не резкое мигание вкл/выкл) с периодом pulsePeriod.
// Дублирует сигнал LowHealthPulseComponent (тот красит только тело игрока в мировых координатах) — предупреждение
// должно быть заметно, даже если взгляд не на персонаже, но не закрывать обзор в центре экрана целиком, как это
// делал бы сплошной прямоугольник.
class ENGINE_API LowHealthScreenFlashComponent : public IComponent {
public:
    LowHealthScreenFlashComponent(
        HealthComponent& health, sf::Vector2f windowSize, int lowHpThreshold, sf::Time pulsePeriod, std::function<bool()> isVisible = nullptr);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;
    void reset() override;

private:
    // Выставляет альфу внешних (глубина 0 — самый край экрана) вершин каждой из 4 полос-градиентов; внутренние
    // (глубина thickness) остаются нулевой альфой всегда — задаются один раз в конструкторе.
    void setEdgeAlpha(sf::Uint8 alpha);

    HealthComponent& m_health;
    int m_lowHpThreshold;
    sf::Time m_pulsePeriod;
    std::function<bool()> m_isVisible;

    sf::VertexArray m_vignette;
    // Фаза синусоиды — растёт непрерывно, пока активно (не сбрасывается на полпериода, как было бы у дискретного
    // мигания); см. update().
    sf::Time m_pulseTimer;
};
