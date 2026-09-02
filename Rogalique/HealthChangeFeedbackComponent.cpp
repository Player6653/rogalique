#include "HealthChangeFeedbackComponent.h"
#include "HealthComponent.h"
#include "CameraComponent.h"
#include "GameObject.h"
#include "ParticleSystem.h"

namespace
{
    constexpr float SHAKE_MAGNITUDE_PIXELS = 10.f;
    const sf::Time SHAKE_DURATION = sf::seconds(0.2f);

    constexpr int DAMAGE_PARTICLE_COUNT = 14;
    const sf::Color DAMAGE_PARTICLE_COLOR(220, 40, 40);

    constexpr int HEAL_PARTICLE_COUNT = 16;
    const sf::Color HEAL_PARTICLE_COLOR(80, 220, 110);

    // Общие для обеих вспышек — небольшой радиус разлёта, короткая жизнь, чтобы не загромождать экран и не
    // накапливаться в ParticleSystem, если урон/лечение случаются часто подряд (бой на арене).
    constexpr float PARTICLE_SPEED_MIN = 40.f;
    constexpr float PARTICLE_SPEED_MAX = 110.f;
    constexpr float PARTICLE_SIZE_MIN = 2.f;
    constexpr float PARTICLE_SIZE_MAX = 5.f;
    const sf::Time PARTICLE_LIFETIME = sf::seconds(0.45f);
}

HealthChangeFeedbackComponent::HealthChangeFeedbackComponent(HealthComponent& target, CameraComponent& camera)
    : m_target(target)
    , m_camera(camera)
    , m_lastHp(target.getHp())
{
}

void HealthChangeFeedbackComponent::update(sf::Time)
{
    int currentHp = m_target.getHp();
    if (currentHp != m_lastHp) {
        GameObject* owner = m_target.getOwner();
        sf::Vector2f position = owner ? owner->getPosition() : sf::Vector2f(0.f, 0.f);
        if (currentHp < m_lastHp) {
            m_camera.shake(SHAKE_MAGNITUDE_PIXELS, SHAKE_DURATION);
            ParticleSystem::instance().spawnBurst(position, DAMAGE_PARTICLE_COUNT, DAMAGE_PARTICLE_COLOR,
                PARTICLE_SPEED_MIN, PARTICLE_SPEED_MAX, PARTICLE_SIZE_MIN, PARTICLE_SIZE_MAX, PARTICLE_LIFETIME);
        } else {
            ParticleSystem::instance().spawnBurst(position, HEAL_PARTICLE_COUNT, HEAL_PARTICLE_COLOR,
                PARTICLE_SPEED_MIN, PARTICLE_SPEED_MAX, PARTICLE_SIZE_MIN, PARTICLE_SIZE_MAX, PARTICLE_LIFETIME);
        }
    }
    m_lastHp = currentHp;
}

void HealthChangeFeedbackComponent::reset()
{
    // См. класс-комментарий в шапке файла (бывший CameraShakeOnDamageComponent::reset) — без этого первый update()
    // после ребута/загрузки мог бы прочитать восстановление HP до максимума как "лечение" и пыхнуть зелёным на
    // ровном месте сразу после возврата в игру.
    m_lastHp = m_target.getHp();
}
