#include "EnemyBehaviorComponent.h"
#include "ChaseComponent.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "Log.h"
#include "NavGrid.h"
#include <cmath>
#include <random>

namespace
{
    // Насколько близко считается "дошёл" до точки патруля/тревоги — не обязательно вплотную.
    constexpr float PATROL_STOP_DISTANCE = 8.f;
    constexpr float ALERT_STOP_DISTANCE = 10.f;
    constexpr float MIN_PATROL_INTERVAL = 2.5f;
    constexpr float MAX_PATROL_INTERVAL = 5.f;

    // Как часто переоценивать бросок монетки бокового зрения, пока цель остаётся в боковой зоне — переоценка
    // каждый кадр мигала бы "вижу"/"не вижу" ежекадрово, это будет заметно и будет читаться как баг.
    const sf::Time PERIPHERAL_REROLL_INTERVAL = sf::seconds(0.5f);

    float angleBetweenDeg(sf::Vector2f a, sf::Vector2f b)
    {
        float lenA = std::sqrt(a.x * a.x + a.y * a.y);
        float lenB = std::sqrt(b.x * b.x + b.y * b.y);
        if (lenA < 0.0001f || lenB < 0.0001f) {
            return 0.f;
        }
        float cosAngle = (a.x * b.x + a.y * b.y) / (lenA * lenB);
        cosAngle = std::max(-1.f, std::min(1.f, cosAngle));
        return std::acos(cosAngle) * 180.f / 3.14159265f;
    }

    bool rollChance(float probability)
    {
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist(0.f, 1.f);
        return dist(rng) < probability;
    }
} // namespace

const char* EnemyBehaviorComponent::stateName(BotState state)
{
    switch (state) {
    case BotState::Patrol:
        return "Patrol";
    case BotState::Chase:
        return "Chase";
    case BotState::Alert:
        return "Alert";
    case BotState::Rest:
        return "Rest";
    }
    return "?";
}

EnemyBehaviorComponent::EnemyBehaviorComponent(
    float chaseRadius, float patrolRadius, std::string label, bool omnidirectionalVision)
    : m_chaseRadius(chaseRadius),
      m_patrolRadius(patrolRadius),
      m_label(std::move(label)),
      m_omnidirectionalVision(omnidirectionalVision)
{
}

namespace
{
    void logStateChange(const std::string& label, BotState from, BotState to)
    {
        if (label.empty() || from == to) {
            return;
        }
        LOG_INFO(label + ": " + EnemyBehaviorComponent::stateName(from) + " -> " + EnemyBehaviorComponent::stateName(to));
    }
} // namespace

void EnemyBehaviorComponent::pickNewPatrolPoint(GameObject* owner)
{
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> angleDist(0.f, 6.2831853f);
    std::uniform_real_distribution<float> radiusDist(0.f, m_patrolRadius);
    std::uniform_real_distribution<float> intervalDist(MIN_PATROL_INTERVAL, MAX_PATROL_INTERVAL);

    NavGrid* nav = GameWorld::instance().getNavGrid();
    for (int attempt = 0; attempt < 8; ++attempt) {
        float angle = angleDist(rng);
        float radius = radiusDist(rng);
        sf::Vector2f candidate = m_spawnPosition + sf::Vector2f(std::cos(angle) * radius, std::sin(angle) * radius);
        if (!nav || nav->isWalkableWorld(candidate)) {
            m_patrolTarget = candidate;
            m_patrolPickCooldown = sf::seconds(intervalDist(rng));
            return;
        }
    }
    // 8 попыток не нашли проходимую точку (маловероятно, но патрульный радиус мог упереться в стену) — постоим
    // на точке спавна, кулдаун всё равно взводим, чтобы не пытаться на каждом кадре.
    m_patrolTarget = m_spawnPosition;
    m_patrolPickCooldown = sf::seconds(intervalDist(rng));
}

void EnemyBehaviorComponent::update(sf::Time dt)
{
    GameObject* owner = getOwner();
    if (!owner) {
        return;
    }

    auto* health = owner->getComponent<HealthComponent>();
    if (health && health->isDead()) {
        return;
    }

    auto* chase = owner->getComponent<ChaseComponent>();
    if (!chase) {
        return;
    }

    if (!m_hasSpawnPosition) {
        m_spawnPosition = owner->getPosition();
        m_patrolTarget = m_spawnPosition;
        m_hasSpawnPosition = true;
    }

    if (m_peripheralRerollCooldown > sf::Time::Zero) {
        m_peripheralRerollCooldown -= dt;
    }

    // Chase: видит ли цель прямо сейчас — не просто "в радиусе", а ещё и без стены между, и в конусе зрения.
    // Мёртвого игрока в счёт не берём — иначе бот стоял бы над трупом в Chase бесконечно, вместо того чтобы уйти
    // искать (Alert -> Patrol).
    const GameObject* player = chase->getTarget();
    bool canSeePlayer = false;
    if (player) {
        auto* playerHealth = const_cast<GameObject*>(player)->getComponent<HealthComponent>();
        bool playerAlive = !playerHealth || !playerHealth->isDead();
        if (playerAlive) {
            sf::Vector2f delta = player->getPosition() - owner->getPosition();
            float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            if (distance <= m_chaseRadius) {
                NavGrid* nav = GameWorld::instance().getNavGrid();
                bool losClear = !nav || nav->hasLineOfSight(owner->getPosition(), player->getPosition());
                if (losClear && m_omnidirectionalVision) {
                    // Видит по всем сторонам (слизь) — конус вообще не считаем.
                    canSeePlayer = true;
                    m_peripheralRerollCooldown = sf::Time::Zero;
                } else if (losClear && m_state == BotState::Chase) {
                    // Уже преследуем — конус зрения тут не сужаем: бот, стоящий вплотную к границе боковой зоны,
                    // иначе терял бы и снова находил цель раз в PERIPHERAL_REROLL_INTERVAL (Chase<->Alert мигание,
                    // из-за которого преследование выглядело дёрганым). Конус решает только САМО обнаружение —
                    // сумеет ли бот вообще заметить ещё не замеченную цель, а не удержание уже начатой погони.
                    canSeePlayer = true;
                    m_peripheralRerollCooldown = sf::Time::Zero;
                } else if (losClear) {
                    float angleDeg = angleBetweenDeg(chase->getFacing(), delta);
                    if (angleDeg <= PRIMARY_HALF_ANGLE_DEG) {
                        // В основном конусе — видит гарантированно, следующий заход в боковую зону кинет монетку заново.
                        canSeePlayer = true;
                        m_peripheralRerollCooldown = sf::Time::Zero;
                    } else if (angleDeg <= PERIPHERAL_HALF_ANGLE_DEG) {
                        // Боковое зрение — шанс на переоценку, не на кадр, см. PERIPHERAL_REROLL_INTERVAL.
                        if (m_peripheralRerollCooldown <= sf::Time::Zero) {
                            m_peripheralSpotted = rollChance(PERIPHERAL_SPOT_CHANCE);
                            m_peripheralRerollCooldown = PERIPHERAL_REROLL_INTERVAL;
                        }
                        canSeePlayer = m_peripheralSpotted;
                    } else {
                        // За спиной — вне поля зрения совсем, следующий заход в боковую зону снова кинет монетку.
                        m_peripheralRerollCooldown = sf::Time::Zero;
                    }
                }
            }
        }
    }

    if (canSeePlayer) {
        logStateChange(m_label, m_state, BotState::Chase);
        m_state = BotState::Chase;
        m_lastKnownPlayerPosition = player->getPosition();
        m_hasLastKnown = true;
        // Override не ставим — обычное поведение ChaseComponent (по m_target) само погонится за игроком, теперь
        // через NavGrid/A*, а не по прямой.
        return;
    }

    if (m_state == BotState::Chase) {
        // Только что потеряли из виду — переходим к последней замеченной точке.
        logStateChange(m_label, m_state, BotState::Alert);
        m_state = BotState::Alert;
    }

    if (m_state == BotState::Alert) {
        if (m_hasLastKnown) {
            sf::Vector2f delta = m_lastKnownPlayerPosition - owner->getPosition();
            float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
            chase->setSeekOverride(m_lastKnownPlayerPosition, ALERT_STOP_DISTANCE);
            if (distance <= ALERT_STOP_DISTANCE) {
                // Дошли до последней точки, а цели нет — сдаёмся, назад в патруль.
                m_hasLastKnown = false;
                logStateChange(m_label, m_state, BotState::Patrol);
                m_state = BotState::Patrol;
            }
            return;
        }
        logStateChange(m_label, m_state, BotState::Patrol);
        m_state = BotState::Patrol;
    }

    if (m_state == BotState::Rest) {
        // Стоим на месте (override на собственную позицию с stopDistance=0) — иначе ChaseComponent без override
        // сам решал бы, идти ли к игроку по одной дистанции, не зная про стены (LOS уже проверен выше, canSeePlayer
        // false, но ChaseComponent об этом не знает).
        chase->setSeekOverride(owner->getPosition(), 0.f);
        m_restRemaining -= dt;
        if (m_restRemaining > sf::Time::Zero) {
            return;
        }
        logStateChange(m_label, m_state, BotState::Patrol);
        m_state = BotState::Patrol;
    }

    // Patrol: бродим вокруг точки спавна.
    if (m_patrolPickCooldown > sf::Time::Zero) {
        m_patrolPickCooldown -= dt;
    }
    sf::Vector2f toPatrol = m_patrolTarget - owner->getPosition();
    float patrolDistance = std::sqrt(toPatrol.x * toPatrol.x + toPatrol.y * toPatrol.y);
    if (m_patrolPickCooldown <= sf::Time::Zero || patrolDistance <= PATROL_STOP_DISTANCE) {
        // Вместо новой точки патруля — шанс постоять и отдохнуть (см. класс-комментарий).
        if (rollChance(REST_CHANCE)) {
            logStateChange(m_label, m_state, BotState::Rest);
            m_state = BotState::Rest;
            m_restRemaining = sf::seconds(REST_DURATION_SECONDS);
            chase->setSeekOverride(owner->getPosition(), 0.f);
            return;
        }
        pickNewPatrolPoint(owner);
    }
    chase->setSeekOverride(m_patrolTarget, PATROL_STOP_DISTANCE);
}

void EnemyBehaviorComponent::reset()
{
    m_state = BotState::Patrol;
    m_hasLastKnown = false;
    m_patrolPickCooldown = sf::Time::Zero;
    m_peripheralRerollCooldown = sf::Time::Zero;
    m_peripheralSpotted = false;
    m_restRemaining = sf::Time::Zero;
    if (m_hasSpawnPosition) {
        m_patrolTarget = m_spawnPosition;
    }
}
