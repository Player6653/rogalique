#include "SlimeSplitComponent.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "Log.h"
#include "Slime.h"
#include "TransientComponent.h"
#include <cmath>
#include <memory>
#include <random>

namespace
{
    constexpr float TAU = 6.2831853f;
    // Коллайдер и HP для взрослой слизи заданы отдельно (см. Slime.cpp SLIME_MAX_HP/size) — детям нужен свой,
    // поменьше, независимо от размера родителя.
    const sf::Vector2f CHILD_COLLIDER_SIZE(18.f, 18.f);
} // namespace

SlimeSplitComponent::SlimeSplitComponent(std::string childSkin, int childMaxHp, float childVisualScale, float childSpeed,
    float childDetectionRadius, GameObject& spawnParent, int childCount, float spreadRadius, sf::Time splitDelay)
    : m_childSkin(std::move(childSkin)),
      m_childMaxHp(childMaxHp),
      m_childVisualScale(childVisualScale),
      m_childSpeed(childSpeed),
      m_childDetectionRadius(childDetectionRadius),
      m_spawnParent(spawnParent),
      m_childCount(childCount),
      m_spreadRadius(spreadRadius),
      m_splitDelay(splitDelay)
{
}

void SlimeSplitComponent::update(sf::Time dt)
{
    if (m_hasSplit) {
        return;
    }

    GameObject* owner = getOwner();
    auto* health = owner ? owner->getComponent<HealthComponent>() : nullptr;
    if (!health) {
        return;
    }

    if (!health->isDead()) {
        m_wasDead = false;
        return;
    }

    if (!m_wasDead) {
        m_wasDead = true;
        m_delayRemaining = m_splitDelay;
    }
    if (m_delayRemaining > sf::Time::Zero) {
        m_delayRemaining -= dt;
        return;
    }

    m_hasSplit = true;

    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> angleDist(0.f, TAU);

    SlimeConfig childConfig;
    childConfig.skin = m_childSkin;
    childConfig.maxHp = m_childMaxHp;
    childConfig.visualScale = m_childVisualScale;
    childConfig.canSplit = false; // дети сами не делятся — иначе расщепление было бы бесконечным.

    sf::Vector2f deathPosition = owner->getPosition();
    for (int i = 0; i < m_childCount; ++i) {
        float angle = angleDist(rng);
        sf::Vector2f offset(std::cos(angle) * m_spreadRadius, std::sin(angle) * m_spreadRadius);
        auto child = std::make_unique<Slime>(
            deathPosition + offset, CHILD_COLLIDER_SIZE, m_childSpeed, m_childDetectionRadius, childConfig);
        // Заспавнена динамически во время игры, не изначальным ростером SceneFacade — без этой метки полный
        // ребут уровня (GameObject::destroyTransientChildren()) не знал бы, что её нужно убрать.
        child->addComponent<TransientComponent>();
        GameWorld::instance().spawnIn(m_spawnParent, std::move(child));
    }
    LOG_INFO(
        m_childSkin + ": разделилась на " + std::to_string(m_childCount) + " (" + std::to_string(m_childMaxHp) + " HP каждая)");
}

void SlimeSplitComponent::reset()
{
    m_wasDead = false;
    m_hasSplit = false;
    m_delayRemaining = sf::Time::Zero;
}
