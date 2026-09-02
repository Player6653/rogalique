#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include "IDirectionProvider.h"
#include <SFML/Graphics.hpp>
#include <vector>

class GameObject;

// Стратегия ИИ - сама ищет на сцене объект с меткой ChaseTargetComponent (обычно игрока) и, пока тот ближе
// detectionRadius, идёт к нему (для MovementComponent), иначе отдаёт (0,0) и объект стоит на месте. Движение — не
// по прямой, а по пути через GameWorld::getNavGrid() (A*, обходит стены); если сетка ещё не построена, падает
// обратно на движение по прямой линии (как раньше).
class ENGINE_API ChaseComponent : public IComponent, public IDirectionProvider {
public:
    // stopDistance — на какой дистанции до цели останавливается, не пытаясь подойти вплотную (обычно чуть меньше радиуса атаки, чтобы враг стоял в зоне удара, а не наваливался на цель). 0 — идти вплотную (как раньше).
    explicit ChaseComponent(float detectionRadius, float stopDistance = 0.f);

    void update(sf::Time dt) override;
    // Сбрасывает override/путь/предупреждение к начальному состоянию — нужно рестарту (resetComponents()): без
    // этого протухший override (например, Alert на последнюю точку ИГРОКА ДО рестарта) мог бы отработать один
    // лишний кадр, если новое состояние бота сразу окажется Chase (та ветка override не переустанавливает).
    // m_target нарочно не трогаем — тот же игрок никуда не делся, а findTarget() лишний поиск не помешает, но и не нужен.
    void reset() override
    {
        m_hasSeekOverride = false;
        m_currentPath.clear();
        m_pathIndex = 0;
        m_pathRecomputeCooldown = sf::Time::Zero;
        m_direction = sf::Vector2f(0.f, 0.f);
        m_warnedNoTarget = false;
    }
    sf::Vector2f getMoveDirection() const override
    {
        return m_direction;
    }
    // В отличие от m_direction (обнуляется, как только бот остановился на stopDistance), m_facing продолжает
    // смотреть на цель, пока та в detectionRadius — иначе взгляд (и, например, конус AttackComponent) замирает
    // в последнем направлении движения, и остановившийся вплотную лучник/орк не разворачивается к цели, зашедшей сбоку.
    sf::Vector2f getFacing() const override
    {
        return m_facing;
    }

    // Кого сейчас преследует (см. findTarget) — нужен, например, Soldier'у, который иногда идёт не за игроком
    // (см. setSeekOverride), но должен помнить, куда возвращаться, когда особая надобность отпадёт.
    const GameObject* getTarget() const
    {
        return m_target;
    }

    // Разово переопределяет цель этого кадра на явную точку вместо обычного ChaseTargetComponent — например,
    // Soldier сбегал за ящиком стрел или сближается на бой врукопашную, когда стрелы кончились. Действует только
    // на ближайший update(): не позвали заново — на следующем кадре сам вернётся к обычному поведению.
    void setSeekOverride(sf::Vector2f position, float stopDistance)
    {
        m_hasSeekOverride = true;
        m_seekOverridePosition = position;
        m_seekOverrideStopDistance = stopDistance;
    }

private:
    void findTarget();
    // Общая логика "иди к goal, останавливайся на stopDistance" — используется и обычным преследованием, и
    // seekOverride. Путь строит через NavGrid (если она есть), с периодическим пересчётом, а не на каждый кадр.
    void moveTowardGoal(GameObject* owner, sf::Vector2f goal, float stopDistance, sf::Time dt);

    const GameObject* m_target = nullptr;
    float m_detectionRadius;
    float m_stopDistance;
    sf::Vector2f m_direction;
    sf::Vector2f m_facing;
    // Чтобы не спамить один и тот же WARN каждый кадр, пока цель не появится на сцене.
    bool m_warnedNoTarget = false;

    bool m_hasSeekOverride = false;
    sf::Vector2f m_seekOverridePosition;
    float m_seekOverrideStopDistance = 0.f;

    // Кэш последнего построенного пути (см. moveTowardGoal) — пересчитывается не каждый кадр, а по таймеру или
    // когда цель заметно ушла от той, для которой путь строился.
    std::vector<sf::Vector2f> m_currentPath;
    std::size_t m_pathIndex = 0;
    sf::Vector2f m_pathGoal;
    sf::Time m_pathRecomputeCooldown;
    // Случайная добавка к PATH_RECOMPUTE_INTERVAL, своя для каждого экземпляра (см. конструктор в .cpp) — без неё
    // все боты, у которых reset() сработал в один кадр (например, "Начать"/рестарт после смерти), пересчитывают
    // A*-путь по одному и тому же таймеру синхронно КАЖДЫЙ раз, даже когда цель не сдвинулась: раз в 0.4с все разом
    // прогоняют полный A* по всей (уже немаленькой, после чанков) карте — просадка FPS повторяется каждые 0.4с,
    // а не один раз. Джиттер размазывает эти пересчёты по разным кадрам вместо одного общего пика.
    sf::Time m_pathRecomputeJitter;
};
