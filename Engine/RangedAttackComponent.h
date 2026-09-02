#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

// Раз в cooldown стреляет снарядом (Projectile) по ближайшему подходящему объекту с HealthComponent в кольце
// [minRange, maxRange], в переднем конусе обзора (как у AttackComponent — иначе игрок мог бы отвернуться от врага
// и всё равно попадать в него автоприцелом) и с прямой видимостью через NavGrid (иначе стрелял бы сквозь стены
// по игроку, спрятавшемуся за углом).
// minRange нужен, чтобы не стрелять в упор, если у владельца есть ещё и отдельная ближняя атака (AttackComponent) —
// тогда в упор работает она, а не эта. Если цели нет и requireTarget=false — направление берётся из facing
// владельца (см. IDirectionProvider), выстрел "от бедра".

class ENGINE_API RangedAttackComponent : public IComponent {
public:
    // shotDelay — задержка между стартом кулдауна (и визуальным "выстрел начался") и реальным появлением снаряда,
    // как hitDelay у AttackComponent: чтобы стрела вылетала не раньше, чем на экране доиграет натягивание лука.
    // targetFilter — см. AttackComponent: необязательный отбор годных целей (кого можно ПРИЦЕЛИВАТЬ), не влияет на
    // то, кого снаряд может случайно задеть уже в полёте (см. ProjectileComponent — тот бьёт любого на своём пути).
    // autoFire=false — как autoAttack у AttackComponent: сам не стреляет из update(), только по вызову tryShoot()
    // извне (игрок, WeaponComponent), но кулдаун/задержку всё равно считает и снаряды спавнит как обычно.
    // requireTarget=true (по умолчанию, автоботы) — без цели в кольце вообще не стреляет. false — нужно ручному
    // выстрелу игрока: без цели снаряд всё равно летит, просто в направлении facing владельца (см. IDirectionProvider),
    // а не прицельно; если и facing нулевой (совсем без направления) — тогда правда не стреляет, стрелять некуда.
    // projectileFrameCount>1 — снаряд анимированный (см. Projectile), projectileFrameDuration задаёт скорость этой
    // анимации; по умолчанию 1 кадр — статичная текстура (как у стрелы лучника, поведение не меняется).
    RangedAttackComponent(std::string label, int damage, float minRange, float maxRange, sf::Time cooldown, float projectileSpeed,
        float projectileHitRadius, std::string projectileTexturePath, sf::Vector2f projectileVisualSize,
        sf::Time shotDelay = sf::Time::Zero, std::function<bool(GameObject*)> targetFilter = nullptr, bool autoFire = true,
        bool requireTarget = true, int projectileFrameCount = 1, sf::Time projectileFrameDuration = sf::Time::Zero);

    void update(sf::Time dt) override;

    // true, если выстрел реально начался (кулдаун прошёл и цель была в кольце). Публичный — нужен ручному вызову
    // (WeaponComponent у игрока), автоботы им не пользуются напрямую, у них update() сам зовёт при autoFire=true.
    bool tryShoot();

    bool isOnCooldown() const
    {
        return m_cooldownRemaining > sf::Time::Zero;
    }

    // Тот же одноразовый флаг-событие, что и у AttackComponent::consumeJustStarted() — см. там комментарий,
    // почему опрос isOnCooldown() раз в кадр эту границу может не увидеть.
    bool consumeJustFired()
    {
        bool result = m_justFired;
        m_justFired = false;
        return result;
    }

    void reset() override
    {
        m_cooldownRemaining = sf::Time::Zero;
        m_shotDelayRemaining = sf::Time::Zero;
        m_hasPendingShot = false;
        m_justFired = false;
    }

private:
    void resolvePendingShot();
    // Ближайший подходящий (см. класс-комментарий) в кольце и в конусе facing, направление на него в outDirection;
    // false, если никого нет.
    bool findTargetDirection(GameObject* owner, sf::Vector2f facing, float facingLength, sf::Vector2f& outDirection) const;

    std::string m_label;
    int m_damage;
    float m_minRange;
    float m_maxRange;
    sf::Time m_cooldown;
    sf::Time m_cooldownRemaining;

    float m_projectileSpeed;
    float m_projectileHitRadius;
    std::string m_projectileTexturePath;
    sf::Vector2f m_projectileVisualSize;
    int m_projectileFrameCount;
    sf::Time m_projectileFrameDuration;

    sf::Time m_shotDelay;
    sf::Time m_shotDelayRemaining;
    bool m_hasPendingShot = false;
    sf::Vector2f m_pendingDirection;

    std::function<bool(GameObject*)> m_targetFilter;
    bool m_autoFire;
    bool m_requireTarget;

    bool m_justFired = false;
};
