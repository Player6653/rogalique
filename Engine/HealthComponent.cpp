#include "pch.h"
#include "HealthComponent.h"
#include "GameException.h"
#include "GameWorld.h"
#include "Log.h"
#include <algorithm>
#include <cassert>
#include <string>

namespace
{
    // Пара миганий HitFlashComponent укладывается ровно в это окно.
    const sf::Time STUN_DURATION = sf::seconds(0.3f);
} // namespace

HealthComponent::HealthComponent(int maxHp, int armor, sf::Time postHitInvulnerability)
    : m_hp(maxHp),
      m_maxHp(maxHp),
      m_armor(armor),
      m_postHitInvulnerability(postHitInvulnerability)
{
    // assert падаем сразу на этапе разработки (Debug), не доходя до дальнейших багов из-за сломанного объекта.
    assert(maxHp > 0 && "HealthComponent: maxHp must be positive");
    assert(armor >= 0 && "HealthComponent: armor must not be negative");
    assert(postHitInvulnerability >= sf::Time::Zero && "HealthComponent: postHitInvulnerability must not be negative");

    // GameException та же проверка, но актуальна и в Release, где assert выше уже ничего не делает.
    if (maxHp <= 0 || armor < 0) {
        std::string message
            = "HealthComponent: invalid stats (maxHp=" + std::to_string(maxHp) + ", armor=" + std::to_string(armor) + ")";
        LOG_ERROR(message);
        throw GameException(message);
    }

    GameWorld::instance().registerHealth(this);
}

HealthComponent::~HealthComponent()
{
    GameWorld::instance().unregisterHealth(this);
}

void HealthComponent::update(sf::Time dt)
{
    if (m_invulnerableTimeRemaining > sf::Time::Zero) {
        m_invulnerableTimeRemaining -= dt;
    }
    if (m_stunTimeRemaining > sf::Time::Zero) {
        m_stunTimeRemaining -= dt;
    }
}

int HealthComponent::takeDamage(int amount)
{
    assert(amount >= 0 && "HealthComponent::takeDamage: amount must not be negative");
    if (amount < 0) {
        LOG_WARN("HealthComponent::takeDamage вызван с отрицательным уроном (" + std::to_string(amount) + "), игнорирую");
        return 0;
    }

    if (isInvulnerable()) {
        return 0;
    }

    if (m_damageInterceptor) {
        amount = m_damageInterceptor(amount);
    }

    m_stunTimeRemaining = STUN_DURATION;
    if (m_postHitInvulnerability > sf::Time::Zero) {
        m_invulnerableTimeRemaining = m_postHitInvulnerability;
    }

    int effectiveDamage = std::max(0, amount - m_armor);
    int hpBefore = m_hp;
    m_hp = std::max(0, m_hp - effectiveDamage);
    return hpBefore - m_hp;
}

void HealthComponent::setHp(int hp)
{
    m_hp = std::max(0, std::min(m_maxHp, hp));
}

void HealthComponent::setInvulnerable(sf::Time duration)
{
    m_invulnerableTimeRemaining = duration;
}

void HealthComponent::reset()
{
    m_hp = m_maxHp;
    m_invulnerableTimeRemaining = sf::Time::Zero;
    m_stunTimeRemaining = sf::Time::Zero;
}
