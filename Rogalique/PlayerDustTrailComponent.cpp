#include "PlayerDustTrailComponent.h"
#include "GameObject.h"
#include "InputComponent.h"
#include "ParticleSystem.h"
#include <cmath>

namespace
{
    // Совпадает с PlayerAnimationComponent::SPRINT_THRESHOLD — InputComponent умножает вектор направления на
    // SPRINT_MULTIPLIER (>1), пока зажат Shift, обычная ходьба даёт длину ровно 1.
    constexpr float SPRINT_THRESHOLD = 1.3f;
    const sf::Time SPAWN_INTERVAL = sf::seconds(0.12f);
    constexpr int PARTICLES_PER_PUFF = 4;
    const sf::Color DUST_COLOR(190, 165, 120);
}

PlayerDustTrailComponent::PlayerDustTrailComponent(InputComponent& input, float feetOffsetY)
    : m_input(input)
    , m_feetOffsetY(feetOffsetY)
    , m_timer(sf::Time::Zero)
{
}

void PlayerDustTrailComponent::update(sf::Time dt)
{
    // Рывок и прыжок — свой спрайтовый пылевой эффект из настоящих кадров пака (см. Player::getDustSprite() и
    // PlayerAnimationComponent) — оба манёвра дают direction длиннее спринтового порога (DASH/JUMP_MULTIPLIER
    // больше SPRINT_MULTIPLIER, см. InputComponent.cpp), так что без этой проверки процедурная пыль под ногами
    // накладывалась бы поверх уже готового эффекта рывка/прыжка.
    if (m_input.isDashing() || m_input.isJumping()) {
        m_timer = sf::Time::Zero;
        return;
    }

    sf::Vector2f direction = m_input.getMoveDirection();
    float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= SPRINT_THRESHOLD) {
        m_timer = sf::Time::Zero; // Следующий пыхтящий след начинается сразу с первого кадра нового спринта.
        return;
    }

    m_timer += dt;
    if (m_timer < SPAWN_INTERVAL) {
        return;
    }
    m_timer = sf::Time::Zero;

    sf::Vector2f feetPosition = getOwner()->getPosition() + sf::Vector2f(0.f, m_feetOffsetY);
    ParticleSystem::instance().spawnBurst(feetPosition, PARTICLES_PER_PUFF, DUST_COLOR, 10.f, 35.f, 2.f, 4.f, sf::seconds(0.35f));
}

void PlayerDustTrailComponent::reset()
{
    m_timer = sf::Time::Zero;
}
