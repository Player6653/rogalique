#include "BossSpinBarrageComponent.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "Projectile.h"
#include "VisualEffect.h"
#include <cmath>

namespace
{
    const std::string EFFECTS_DIR = "Resources/Characters/The Vampire Lord & Spawns/Vampire Lord/Magical Effects/";

    // Родные кадры — все квадратные, ширина листа делится на высоту (16x16), см. комментарий в Boss.cpp о том, как
    // эти размеры вообще были определены (по факту размеров файлов, а не на глаз).
    constexpr int SMALL_ONGOING_FRAME_COUNT = 9;
    constexpr int MULTI_ONGOING_FRAME_COUNT = 3;
    constexpr int SMALL_START_FRAME_COUNT = 3;
    constexpr int MULTI_START_FRAME_COUNT = 9;
    const sf::Vector2f RING_PROJECTILE_VISUAL_SIZE(18.f, 18.f);
    const sf::Vector2f START_VFX_VISUAL_SIZE(40.f, 40.f);
    constexpr float RING_PROJECTILE_HIT_RADIUS = 12.f;
    constexpr float RING_PROJECTILE_MAX_RANGE = 480.f;

    // Угловой сдвиг внешнего кольца относительно внутреннего — иначе оба кольца стреляли бы строго в одни и те же
    // направления, и получалось бы визуально одно кольцо с двумя разными снарядами подряд в каждом луче, а не два
    // разных узора.
    constexpr float MULTI_RING_ANGLE_OFFSET_DEG = 22.5f;

    // Болты кольца мелкие — маленькая вспышка попадания (см. Boss.cpp — та же текстура, что и у обычного выстрела).
    constexpr int IMPACT_FRAME_COUNT = 5; // vampire_lord_PROJECTILE_IMPACT_SMALL.png, 80x16 -> 16x16
    const sf::Vector2f IMPACT_VISUAL_SIZE(24.f, 24.f);
    const sf::Time IMPACT_FRAME_DURATION = sf::seconds(0.04f);

    void spawnImpact(sf::Vector2f position)
    {
        GameWorld::instance().spawnInRoot(std::make_unique<VisualEffect>(
            position, EFFECTS_DIR + "vampire_lord_PROJECTILE_IMPACT_SMALL.png", IMPACT_VISUAL_SIZE, IMPACT_FRAME_COUNT,
            IMPACT_FRAME_DURATION));
    }

    void spawnRing(GameObject& owner, int count, int damage, float speed, const std::string& texturePath, int frameCount,
        sf::Time frameDuration, float angleOffsetDeg)
    {
        if (count <= 0) {
            return;
        }
        sf::Vector2f center = owner.getPosition();
        float angleStep = 360.f / static_cast<float>(count);
        constexpr float DEG_TO_RAD = 3.14159265f / 180.f;
        for (int i = 0; i < count; ++i) {
            float angleDeg = angleOffsetDeg + angleStep * static_cast<float>(i);
            float angleRad = angleDeg * DEG_TO_RAD;
            sf::Vector2f direction(std::cos(angleRad), std::sin(angleRad));
            auto projectile = std::make_unique<Projectile>(center, direction, speed, damage, RING_PROJECTILE_HIT_RADIUS,
                RING_PROJECTILE_MAX_RANGE, &owner, texturePath, RING_PROJECTILE_VISUAL_SIZE, frameCount, frameDuration,
                &spawnImpact);
            GameWorld::instance().spawnInRoot(std::move(projectile));
        }
    }
} // namespace

BossSpinBarrageComponent::BossSpinBarrageComponent(HealthComponent& health, sf::Time interval, sf::Time channelDelay,
    int smallRingCount, int smallRingDamage, float smallRingSpeed, int multiRingCount, int multiRingDamage,
    float multiRingSpeed)
    : m_health(health),
      m_interval(interval),
      m_cooldownRemaining(interval),
      m_channelDelay(channelDelay),
      m_smallRingCount(smallRingCount),
      m_smallRingDamage(smallRingDamage),
      m_smallRingSpeed(smallRingSpeed),
      m_multiRingCount(multiRingCount),
      m_multiRingDamage(multiRingDamage),
      m_multiRingSpeed(multiRingSpeed)
{
}

void BossSpinBarrageComponent::update(sf::Time dt)
{
    if (m_health.isDead()) {
        return;
    }

    if (m_channeling) {
        m_channelRemaining -= dt;
        if (m_channelRemaining <= sf::Time::Zero) {
            releaseBarrage();
        }
        return;
    }

    m_cooldownRemaining -= dt;
    if (m_cooldownRemaining <= sf::Time::Zero) {
        beginChannel();
    }
}

void BossSpinBarrageComponent::beginChannel()
{
    GameObject* owner = getOwner();
    if (!owner) {
        return;
    }
    m_channeling = true;
    m_channelRemaining = m_channelDelay;

    // Обе телеграфные вспышки сразу на позиции босса — не прицельные, просто предупреждают "сейчас будет залп",
    // тем дольше и заметнее из двух роликов (MULTI_START, 9 кадров), сколько и длится сам channelDelay (см. Boss.cpp).
    sf::Vector2f center = owner->getPosition();
    GameWorld::instance().spawnInRoot(std::make_unique<VisualEffect>(
        center, EFFECTS_DIR + "vampire_lord_SMALL_PROJECTILE_SPIN_START.png", START_VFX_VISUAL_SIZE, SMALL_START_FRAME_COUNT,
        sf::seconds(0.08f)));
    GameWorld::instance().spawnInRoot(std::make_unique<VisualEffect>(
        center, EFFECTS_DIR + "vampire_lord_MULTI_PROJECTILE_SPIN_START.png", START_VFX_VISUAL_SIZE, MULTI_START_FRAME_COUNT,
        sf::seconds(0.06f)));
}

void BossSpinBarrageComponent::releaseBarrage()
{
    m_channeling = false;
    m_cooldownRemaining = m_interval;

    GameObject* owner = getOwner();
    if (!owner || m_health.isDead()) {
        return;
    }

    spawnRing(*owner, m_smallRingCount, m_smallRingDamage, m_smallRingSpeed,
        EFFECTS_DIR + "vampire_lord_SMALL_PROJECTILE_SPIN_ONGOING.png", SMALL_ONGOING_FRAME_COUNT, sf::seconds(0.05f), 0.f);
    spawnRing(*owner, m_multiRingCount, m_multiRingDamage, m_multiRingSpeed,
        EFFECTS_DIR + "vampire_lord_MULTI_PROJECTILE_SPIN_ONGOING.png", MULTI_ONGOING_FRAME_COUNT, sf::seconds(0.06f),
        MULTI_RING_ANGLE_OFFSET_DEG);
}

void BossSpinBarrageComponent::reset()
{
    m_channeling = false;
    m_cooldownRemaining = m_interval;
    m_channelRemaining = sf::Time::Zero;
}
