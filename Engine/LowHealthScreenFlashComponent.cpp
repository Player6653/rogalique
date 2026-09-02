#include "pch.h"
#include "LowHealthScreenFlashComponent.h"
#include "HealthComponent.h"
#include <cmath>

namespace
{
    // Цвет только rgb — альфа выставляется отдельно на внешних вершинах (см. setEdgeAlpha), внутренние всегда 0.
    const sf::Color FLASH_COLOR(200, 20, 20);
    constexpr sf::Uint8 FLASH_MAX_ALPHA = 140;
    // Насколько глубоко красное вбирается внутрь экрана от каждого края — центр остаётся чистым, не мешает обзору.
    constexpr float VIGNETTE_THICKNESS = 130.f;
    constexpr float PI = 3.14159265f;

    // Индексы внешних (глубина 0, у самого края экрана) вершин каждой из 4 полос-градиентов в m_vignette — им
    // одним меняется альфа при пульсации (см. setEdgeAlpha); внутренние (глубина VIGNETTE_THICKNESS) остаются
    // нулевой альфой всегда, заданы один раз в конструкторе.
    constexpr std::size_t OUTER_INDICES[] = {0, 1, 4, 5, 8, 9, 12, 13};
}

LowHealthScreenFlashComponent::LowHealthScreenFlashComponent(
    HealthComponent& health, sf::Vector2f windowSize, int lowHpThreshold, sf::Time pulsePeriod, std::function<bool()> isVisible)
    : m_health(health)
    , m_lowHpThreshold(lowHpThreshold)
    , m_pulsePeriod(pulsePeriod)
    , m_isVisible(std::move(isVisible))
    , m_vignette(sf::Quads, 16)
{
    float w = windowSize.x;
    float h = windowSize.y;
    float t = VIGNETTE_THICKNESS;

    // Top: (0,0)-(w,0)-(w,t)-(0,t).
    m_vignette[0].position = sf::Vector2f(0.f, 0.f);
    m_vignette[1].position = sf::Vector2f(w, 0.f);
    m_vignette[2].position = sf::Vector2f(w, t);
    m_vignette[3].position = sf::Vector2f(0.f, t);
    // Bottom: (0,h)-(w,h)-(w,h-t)-(0,h-t).
    m_vignette[4].position = sf::Vector2f(0.f, h);
    m_vignette[5].position = sf::Vector2f(w, h);
    m_vignette[6].position = sf::Vector2f(w, h - t);
    m_vignette[7].position = sf::Vector2f(0.f, h - t);
    // Left: (0,0)-(0,h)-(t,h)-(t,0).
    m_vignette[8].position = sf::Vector2f(0.f, 0.f);
    m_vignette[9].position = sf::Vector2f(0.f, h);
    m_vignette[10].position = sf::Vector2f(t, h);
    m_vignette[11].position = sf::Vector2f(t, 0.f);
    // Right: (w,0)-(w,h)-(w-t,h)-(w-t,0).
    m_vignette[12].position = sf::Vector2f(w, 0.f);
    m_vignette[13].position = sf::Vector2f(w, h);
    m_vignette[14].position = sf::Vector2f(w - t, h);
    m_vignette[15].position = sf::Vector2f(w - t, 0.f);

    for (std::size_t i = 0; i < m_vignette.getVertexCount(); ++i) {
        m_vignette[i].color = sf::Color(FLASH_COLOR.r, FLASH_COLOR.g, FLASH_COLOR.b, 0);
    }
}

void LowHealthScreenFlashComponent::setEdgeAlpha(sf::Uint8 alpha)
{
    for (std::size_t index : OUTER_INDICES) {
        m_vignette[index].color.a = alpha;
    }
}

void LowHealthScreenFlashComponent::update(sf::Time dt)
{
    bool active = (!m_isVisible || m_isVisible()) && !m_health.isDead() && m_health.getHp() <= m_lowHpThreshold;
    if (!active) {
        m_pulseTimer = sf::Time::Zero;
        setEdgeAlpha(0);
        return;
    }

    // Растёт непрерывно, пока активно — не сбрасывается на полуцикле, как было бы у дискретного мигания: иначе
    // на границе "стало активно" волна дёргалась бы с середины, а не с нуля.
    m_pulseTimer += dt;
    float period = m_pulsePeriod.asSeconds();
    if (period <= 0.f) {
        setEdgeAlpha(FLASH_MAX_ALPHA);
        return;
    }
    float phase = std::fmod(m_pulseTimer.asSeconds(), period) / period; // 0..1 внутри текущего цикла.
    // 1-cos даёт гладкий подъём от 0 до максимума и плавный спад обратно, вместо резкого
    // прыжка между 0 и FLASH_MAX_ALPHA, как было раньше (дискретное мигание каждые pulsePeriod).
    float wave = 0.5f - 0.5f * std::cos(phase * 2.f * PI);
    setEdgeAlpha(static_cast<sf::Uint8>(wave * FLASH_MAX_ALPHA));
}

void LowHealthScreenFlashComponent::draw(sf::RenderWindow& window) const
{
    window.draw(m_vignette);
}

void LowHealthScreenFlashComponent::reset()
{
    m_pulseTimer = sf::Time::Zero;
    setEdgeAlpha(0);
}
