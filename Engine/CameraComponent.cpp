#include "pch.h"
#include "CameraComponent.h"
#include "GameWorld.h"
#include <algorithm>
#include <cstdlib>

CameraComponent::CameraComponent(sf::Vector2f viewSize)
    : m_view(sf::FloatRect(0.f, 0.f, viewSize.x, viewSize.y))
{
    GameWorld::instance().registerCamera(this);
}

CameraComponent::~CameraComponent()
{
    GameWorld::instance().unregisterCamera(this);
}

void CameraComponent::onOwnerMoved(sf::Vector2f newPosition)
{
    sf::Vector2f center = newPosition;
    if (m_hasBounds) {
        sf::Vector2f viewSize = m_view.getSize();
        // Клампим только там, где есть куда клампить — уровень шире/выше вида по этой оси. Если уровень уже вида
        // по оси (например, ещё маленький, недостроенный уровень в Tiled — min>max, std::clamp был бы не определён),
        // camera НЕ прибивается к центру уровня, а просто следует за игроком без клампа: у края будет видна
        // пустота за пределами уровня, но это осознанно лучше, чем немая камера, полностью игнорирующая игрока —
        // тем более раз это временное состояние уровня, который ещё достраивается.
        if (m_bounds.width > viewSize.x) {
            float minX = m_bounds.left + viewSize.x / 2.f;
            float maxX = m_bounds.left + m_bounds.width - viewSize.x / 2.f;
            center.x = std::max(minX, std::min(maxX, center.x));
        }
        if (m_bounds.height > viewSize.y) {
            float minY = m_bounds.top + viewSize.y / 2.f;
            float maxY = m_bounds.top + m_bounds.height - viewSize.y / 2.f;
            center.y = std::max(minY, std::min(maxY, center.y));
        }
    }
    m_baseCenter = center;
}

void CameraComponent::update(sf::Time dt)
{
    if (m_shakeRemaining <= sf::Time::Zero) {
        m_shakeOffset = sf::Vector2f(0.f, 0.f);
        return;
    }
    m_shakeRemaining -= dt;
    if (m_shakeRemaining <= sf::Time::Zero) {
        m_shakeOffset = sf::Vector2f(0.f, 0.f);
        return;
    }
    // Линейное затухание амплитуды к концу тряски — дёргает сильнее всего сразу после удара, а не резко
    // обрывается на полной силе на последнем кадре.
    float fraction = m_shakeRemaining.asSeconds() / m_shakeDuration.asSeconds();
    float currentMagnitude = m_shakeMagnitude * fraction;
    // rand()/RAND_MAX даёт [0,1], *2-1 растягивает до [-1,1] — простое дрожание в обе стороны по каждой оси
    // независимо, качество случайности тут не важно (чисто косметический эффект, не игровая логика).
    float offsetX = (static_cast<float>(std::rand()) / RAND_MAX * 2.f - 1.f) * currentMagnitude;
    float offsetY = (static_cast<float>(std::rand()) / RAND_MAX * 2.f - 1.f) * currentMagnitude;
    m_shakeOffset = sf::Vector2f(offsetX, offsetY);
}

void CameraComponent::apply(sf::RenderWindow& window)
{
    m_view.setCenter(m_baseCenter + m_shakeOffset);
    window.setView(m_view);
}

void CameraComponent::setBounds(sf::FloatRect worldBounds)
{
    m_bounds = worldBounds;
    m_hasBounds = true;
}

void CameraComponent::shake(float magnitudePixels, sf::Time duration)
{
    m_shakeMagnitude = magnitudePixels;
    m_shakeDuration = duration;
    m_shakeRemaining = duration;
}
