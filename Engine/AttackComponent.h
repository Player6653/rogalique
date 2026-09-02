#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

class HealthComponent;

// Атака — раз в cooldown бьёт по ближайшему подходящему объекту с HealthComponent в радиусе range, наносит
// damage, дальше сама HealthComponent применит броню/неуязвимость.
class ENGINE_API AttackComponent : public IComponent {
public:
    // targetFilter — необязательный отбор годных целей вдобавок к "не мёртв и не сам владелец" (например, у ботов
    // между собой: только тот, у кого есть ChaseTargetComponent, чтобы враги не били друг друга по ошибке).
    // nullptr (по умолчанию) значит "годится любой чужой живой HealthComponent" — как у игрока, тот бьёт кого угодно.
    // requireTarget=true (по умолчанию, как у ботов с autoAttack) — без цели в радиусе tryAttack() вообще не
    // срабатывает (незачем автоботу махать в пустоту каждый кадр). false — нужно ручной атаке игрока: удар
    // засчитывается (кулдаун, анимация через consumeJustStarted()) даже мимо, просто без урона и без цели.
    // omnidirectional=false (по умолчанию) — цель ещё и в переднем конусе ±60° от facing владельца (см.
    // findTarget()), как у Orc/Soldier/игрока. true — конус не проверяется вовсе, бьёт по всем сторонам сразу
    // (слизь: "радиус вокруг себя", а не удар в одну сторону).
    AttackComponent(std::string label, int damage, float range, sf::Time cooldown, bool autoAttack,
        sf::Time hitDelay = sf::Time::Zero, std::function<bool(GameObject*)> targetFilter = nullptr, bool requireTarget = true,
        bool omnidirectional = false);

    void update(sf::Time dt) override;

    // true, если удар реально начался (кулдаун прошёл и цель была в радиусе).
    bool tryAttack();

    bool isOnCooldown() const
    {
        return m_cooldownRemaining > sf::Time::Zero;
    }

    // Разово потребляет флаг "удар начался в этом кадре". Нужен визуальным компонентам вроде EnemyAnimationComponent —
    // они не могут ловить старт удара опросом isOnCooldown() раз в кадр, потому что при автоатаке кулдаун обнуляется и
    // тут же снова взводится внутри одного и того же вызова update() (см. tryAttack()), и внешний наблюдатель эту
    // границу "выключен -> включён" никогда не увидит.
    bool consumeJustStarted()
    {
        bool result = m_justStarted;
        m_justStarted = false;
        return result;
    }

    void reset() override
    {
        m_cooldownRemaining = sf::Time::Zero;
        m_hitDelayRemaining = sf::Time::Zero;
        m_pendingTarget = nullptr;
        m_justStarted = false;
    }

private:
    void resolvePendingHit();

    std::string m_label;
    int m_damage;
    float m_range;
    sf::Time m_cooldown;
    sf::Time m_cooldownRemaining;
    bool m_autoAttack;
    bool m_requireTarget;
    bool m_omnidirectional;
    bool m_justStarted = false;

    sf::Time m_hitDelay;
    sf::Time m_hitDelayRemaining;
    // Невладеющий — цель это HealthComponent живого объекта на сцене. Может быть уничтожен целиком (не просто
    // умереть) за время задержки удара (например, реролл уровня сносит временных ботов волны/детей деления
    // слизи) — resolvePendingHit() перед разыменованием сверяет указатель с GameWorld::getHealthComponents(),
    // а не полагается на то, что этого не может произойти.
    HealthComponent* m_pendingTarget = nullptr;

    std::function<bool(GameObject*)> m_targetFilter;
};
