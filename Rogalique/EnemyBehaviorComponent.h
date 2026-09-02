#pragma once
#include "IComponent.h"
#include <string>

class GameObject;

// Конечный автомат поведения бота поверх ChaseComponent: Patrol (ходит рандомно вокруг точки спавна) -> Chase
// (видит цель — прямая видимость через NavGrid, не просто дистанция, — и гонится за ней через обычное поведение
// ChaseComponent, override не ставим) -> Alert (потерял из виду — идёт к последней замеченной точке; не нашёл
// там цель — назад в Patrol). Из Patrol ещё есть Rest — на каждой смене точки патруля шанс REST_CHANCE вместо
// новой точки постоять REST_DURATION секунд, потом снова в Patrol; как и Patrol, прерывается в Chase, стоит
// заметить цель. Сама атака (AttackComponent/RangedAttackComponent) этим не управляется — она и так автоматически
// срабатывает, стоит цели оказаться в радиусе, независимо от состояния.
enum class BotState { Patrol, Chase, Alert, Rest };

class EnemyBehaviorComponent : public IComponent {
public:
    // Конус зрения относительно направления взгляда бота (ChaseComponent::getFacing()): в пределах PRIMARY —
    // видит гарантированно (при условии прямой видимости и дистанции); от PRIMARY до PERIPHERAL — боковое зрение,
    // шанс PERIPHERAL_SPOT_CHANCE на каждую переоценку (см. m_peripheralRerollCooldown в .cpp); за PERIPHERAL —
    // не видит вовсе. Увеличено с 55/90 — было слишком легко просто обойти бота по кругу и
    // остаться в мёртвой зоне сзади; 75+65=140 в каждую сторону — 150°/280° суммарно, мёртвая зона сзади сузилась
    // с 180° до 80°.
    static constexpr float PRIMARY_HALF_ANGLE_DEG = 75.f;
    static constexpr float PERIPHERAL_HALF_ANGLE_DEG = 140.f;
    static constexpr float PERIPHERAL_SPOT_CHANCE = 0.5f;

    // Шанс на Rest вместо новой точки патруля (см. класс-комментарий) и сама длительность отдыха.
    static constexpr float REST_CHANCE = 0.1f;
    static constexpr float REST_DURATION_SECONDS = 5.f;

    // chaseRadius — та же дистанция обнаружения, что передана ChaseComponent (детектит с прямой видимостью,
    // а не просто по дистанции). patrolRadius — насколько далеко от точки спавна бот готов забредать в патруле.
    // label — для лога при смене состояния (Patrol/Chase/Alert), например "Orc"/"Soldier"; пустая строка — не логировать.
    // omnidirectionalVision=false (по умолчанию) — конус зрения как обычно (см. PRIMARY/PERIPHERAL выше). true —
    // конус вообще не проверяется, видит цель по всем сторонам сразу, как только та в радиусе и без стены между
    // (слизь: "видит игрока по всем направлениям").
    EnemyBehaviorComponent(float chaseRadius, float patrolRadius, std::string label = "", bool omnidirectionalVision = false);

    void update(sf::Time dt) override;
    void reset() override;

    BotState getState() const
    {
        return m_state;
    }
    float getChaseRadius() const
    {
        return m_chaseRadius;
    }
    static const char* stateName(BotState state);

private:
    void pickNewPatrolPoint(GameObject* owner);

    float m_chaseRadius;
    float m_patrolRadius;
    std::string m_label;
    bool m_omnidirectionalVision;

    sf::Vector2f m_spawnPosition;
    bool m_hasSpawnPosition = false;

    BotState m_state = BotState::Patrol;

    sf::Vector2f m_patrolTarget;
    sf::Time m_patrolPickCooldown;

    sf::Vector2f m_lastKnownPlayerPosition;
    bool m_hasLastKnown = false;

    // Боковое зрение не переоценивается каждый кадр — иначе бросок монетки мигал бы "вижу"/"не вижу" ежекадрово.
    sf::Time m_peripheralRerollCooldown = sf::Time::Zero;
    bool m_peripheralSpotted = false;

    sf::Time m_restRemaining = sf::Time::Zero;
};
