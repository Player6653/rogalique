#include "pch.h"
#include "ChaseComponent.h"
#include "ChaseTargetComponent.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "Log.h"
#include "NavGrid.h"
#include <cmath>
#include <random>

namespace
{
    // Как часто пересчитывать путь, пока идём к одной и той же цели — цель (особенно живая, вроде игрока)
    // постоянно немного двигается, пересчитывать на каждый кадр было бы избыточно для сетки такого размера.
    // Увеличено с 0.4с — карта выросла (чанки от игрока, уже 21+ бот разом), суммарная стоимость A* по всем ботам
    // росла вместе с их числом, просадка FPS в бою. GOAL_DRIFT_RECOMPUTE_THRESHOLD ниже всё равно форсирует
    // пересчёт раньше срока, если цель успела заметно сдвинуться — отзывчивость погони страдает не сильно.
    const sf::Time PATH_RECOMPUTE_INTERVAL = sf::seconds(0.7f);
    // Если цель сдвинулась дальше этого — путь безнадёжно устарел, пересчитываем раньше таймера.
    constexpr float GOAL_DRIFT_RECOMPUTE_THRESHOLD = 48.f;
    // Насколько близко нужно подойти к очередной точке пути, чтобы считать её пройденной и перейти к следующей.
    constexpr float WAYPOINT_REACH_DISTANCE = 10.f;
} // namespace

ChaseComponent::ChaseComponent(float detectionRadius, float stopDistance)
    : m_detectionRadius(detectionRadius),
      m_stopDistance(stopDistance)
{
    // См. m_pathRecomputeJitter в .h — своё случайное смещение таймера на весь срок жизни компонента, не только
    // на один пересчёт, иначе боты быстро "снова" сойдутся по фазе.
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> jitterDist(-0.08f, 0.08f);
    m_pathRecomputeJitter = sf::seconds(jitterDist(rng));
}

void ChaseComponent::findTarget()
{
    // Реестр (см. findChaseTarget() в ChaseTargetComponent.h), не обход дерева — тот же приём, что и у остальных
    // потребителей этой метки (Chest/Door/Trap/ItemPickup, Rogalique).
    GameObject* target = findChaseTarget();
    if (target && target != getOwner()) {
        m_target = target;
    }
}

void ChaseComponent::moveTowardGoal(GameObject* owner, sf::Vector2f goal, float stopDistance, sf::Time dt)
{
    if (m_pathRecomputeCooldown > sf::Time::Zero) {
        m_pathRecomputeCooldown -= dt;
    }

    sf::Vector2f toGoal = goal - owner->getPosition();
    float distance = std::sqrt(toGoal.x * toGoal.x + toGoal.y * toGoal.y);
    if (distance > 0.0001f) {
        m_facing = toGoal / distance;
    }

    if (distance <= stopDistance) {
        m_direction = sf::Vector2f(0.f, 0.f);
        m_currentPath.clear();
        return;
    }

    NavGrid* nav = GameWorld::instance().getNavGrid();
    if (!nav) {
        // Сетка ещё не построена (или движок используется без неё) — старое поведение, по прямой.
        m_direction = m_facing;
        return;
    }

    bool needsRecompute = m_currentPath.empty() || m_pathRecomputeCooldown <= sf::Time::Zero;
    if (!needsRecompute) {
        sf::Vector2f goalDrift = goal - m_pathGoal;
        float driftDistance = std::sqrt(goalDrift.x * goalDrift.x + goalDrift.y * goalDrift.y);
        needsRecompute = driftDistance > GOAL_DRIFT_RECOMPUTE_THRESHOLD;
    }

    if (needsRecompute) {
        m_currentPath = nav->findPath(owner->getPosition(), goal);
        m_pathIndex = 0;
        m_pathGoal = goal;
        m_pathRecomputeCooldown = PATH_RECOMPUTE_INTERVAL + m_pathRecomputeJitter;
    }

    if (m_currentPath.empty()) {
        // NavGrid::findPath() пуст либо когда цель в той же клетке (тогда просто идём на неё по прямой), либо
        // когда путь действительно недостижим — прямая видимость отличает одно от другого.
        m_direction = nav->hasLineOfSight(owner->getPosition(), goal) ? m_facing : sf::Vector2f(0.f, 0.f);
        return;
    }

    while (m_pathIndex < m_currentPath.size()) {
        sf::Vector2f toWaypoint = m_currentPath[m_pathIndex] - owner->getPosition();
        float waypointDistance = std::sqrt(toWaypoint.x * toWaypoint.x + toWaypoint.y * toWaypoint.y);
        if (waypointDistance <= WAYPOINT_REACH_DISTANCE) {
            ++m_pathIndex;
            continue;
        }
        m_facing = toWaypoint / waypointDistance;
        m_direction = m_facing;
        return;
    }

    // Дошли до конца построенного пути, но stopDistance почему-то ещё не сработал (например, цель чуть отступила
    // за последнюю точку пути после его построения) — постоим на месте, следующий пересчёт всё поправит.
    m_direction = sf::Vector2f(0.f, 0.f);
}

void ChaseComponent::update(sf::Time dt)
{
    GameObject* owner = getOwner();
    if (!owner) {
        m_direction = sf::Vector2f(0.f, 0.f);
        return;
    }

    // Ищем цель независимо от override — EnemyBehaviorComponent (Patrol/Alert) выставляет override каждый кадр,
    // и если находить цель только в ветке ниже, m_target никогда бы не появился: getTarget() всегда возвращал бы
    // nullptr, canSeePlayer в EnemyBehaviorComponent всегда false, бот навечно застрял бы в Patrol даже стоя
    // вплотную к игроку. Раз найденную цель не теряем, findTarget() после первого успеха больше не дёргаем.
    if (!m_target) {
        findTarget();
    }
    if (!m_target && !m_warnedNoTarget) {
        m_warnedNoTarget = true;
        LOG_WARN("ChaseComponent: на сцене нет ни одного объекта с ChaseTargetComponent, преследовать некого");
    }

    if (m_hasSeekOverride) {
        m_hasSeekOverride = false; // разовое — снимаем сразу же, чтобы без нового вызова вернуться к обычной цели.
        moveTowardGoal(owner, m_seekOverridePosition, m_seekOverrideStopDistance, dt);
        return;
    }

    if (!m_target) {
        m_direction = sf::Vector2f(0.f, 0.f);
        return;
    }

    sf::Vector2f toTarget = m_target->getPosition() - owner->getPosition();
    float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);

    if (distance > m_detectionRadius) {
        // Ещё не заметил цель — не поворачиваемся к ней раньше времени, взгляд остаётся как был.
        m_direction = sf::Vector2f(0.f, 0.f);
        return;
    }

    moveTowardGoal(owner, m_target->getPosition(), m_stopDistance, dt);
}
