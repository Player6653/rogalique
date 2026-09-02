#include "pch.h"
#include "SpawnFadeComponent.h"
#include "SpriteComponent.h"
#include <algorithm>

SpawnFadeComponent::SpawnFadeComponent(SpriteComponent& sprite, sf::Time duration)
    : m_sprite(sprite),
      m_duration(duration)
{
    // Невидим с самого первого кадра — иначе между конструктором и первым update() (тот же кадр, но уже после
    // отрисовки, если компонент добавлен поздно) существо мелькнуло бы полностью видимым один раз.
    m_sprite.setColor(sf::Color(255, 255, 255, 0));
}

void SpawnFadeComponent::update(sf::Time dt)
{
    if (m_finished) {
        return;
    }
    m_elapsed += dt;
    if (m_elapsed >= m_duration) {
        m_finished = true;
        // Обычный белый (без модуляции), а не setColor(..., 255) — тем же приёмом, что и HitFlashComponent,
        // чтобы не мешать HitFlashComponent/другому коду, который тоже трогает цвет этого спрайта, наводить свой
        // тон уже после того, как проявление закончилось.
        m_sprite.clearColor();
        return;
    }
    float t = m_duration.asSeconds() > 0.f ? m_elapsed.asSeconds() / m_duration.asSeconds() : 1.f;
    t = std::min(std::max(t, 0.f), 1.f);
    auto alpha = static_cast<sf::Uint8>(t * 255.f);
    m_sprite.setColor(sf::Color(255, 255, 255, alpha));
}

void SpawnFadeComponent::reset()
{
    // Сейчас не звучит на практике — все существа со SpawnFadeComponent помечены TransientComponent (см.
    // SceneFacade.cpp) и на ребуте уровня уничтожаются целиком, а не переиспользуются через reset(). Оставлен для
    // консистентности с остальными компонентами на таймере (см. HitFlashComponent::reset()) — на случай, если
    // этот приём когда-нибудь применят и к нетранзиентному существу.
    m_elapsed = sf::Time::Zero;
    m_finished = false;
    m_sprite.setColor(sf::Color(255, 255, 255, 0));
}
