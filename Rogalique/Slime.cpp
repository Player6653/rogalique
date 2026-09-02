#include "Slime.h"
#include "ActorAnimationComponent.h"
#include "ActorSpawnHelpers.h"
#include "AttackComponent.h"
#include "ChaseComponent.h"
#include "ChaseTargetComponent.h"
#include "ColliderComponent.h"
#include "DeathParticleComponent.h"
#include "EnemyBehaviorComponent.h"
#include "HealthComponent.h"
#include "HitFlashComponent.h"
#include "Log.h"
#include "MovementComponent.h"
#include "RangedAttackComponent.h"
#include "SlimeShotLimitComponent.h"
#include "SlimeSplitComponent.h"
#include "SpriteComponent.h"
#include <functional>

namespace
{
    // Насколько далеко от точки спавна слизь готова забредать в патруле (см. EnemyBehaviorComponent) — меньше,
    // чем у Orc/Soldier (150), маленькое и слабое существо далеко от дома не уходит.
    constexpr float SLIME_PATROL_RADIUS = 120.f;

    constexpr int SLIME_ARMOR = 0;

    // Плоской брони у игрока больше нет (PLAYER_ARMOR=0 в Player.cpp) — вся защита идёт через надетую экипировку
    // (см. InventoryComponent::absorbHit). Урон ниже, чем у орка (см. ENEMY_ATTACK_DAMAGE в Enemy.cpp) — орк
    // единственный "тяжёлый" противник в игре; и ближний, и дальний урон слизи — одно и то же значение.
    constexpr int SLIME_DAMAGE = 1;
    constexpr float ATTACK_FRAME_DURATION = 0.07f;

    // Ближний бой (Slime1 и не умеющие делиться дети Slime2) — радиус вокруг себя в размере одной клетки,
    // TILE_SIZE из SceneFacade.cpp (48).
    constexpr float MELEE_RANGE = 48.f;
    constexpr float MELEE_CHASE_STOP_DISTANCE = MELEE_RANGE - 10.f;
    const sf::Time MELEE_COOLDOWN = sf::seconds(1.f);
    // Урон на 6-м кадре (где всплеск шипов максимальный, см. Slime*_Attack_body.png) — общий для ближнего и
    // дальнего боя, раскадровка до этого кадра совпадает у всех трёх расцветок, расходится только в хвосте.
    const sf::Time HIT_OR_SHOT_DELAY = sf::seconds(ATTACK_FRAME_DURATION * 6);

    // Дальний бой (Slime3 — "плевок"): держит дистанцию, атака направленная (не по кругу, как у ближнего боя,
    // см. SlimeConfig::isRanged), поэтому AttackComponent ей не подходит — RangedAttackComponent сам не стреляет
    // в упор (distance<=0.0001 внутри findTargetDirection), так что вплотную она беззащитна.
    constexpr float RANGED_MIN_RANGE = 0.f;
    constexpr float RANGED_MAX_RANGE = 200.f;
    const sf::Time RANGED_COOLDOWN = sf::seconds(1.3f);
    constexpr float RANGED_CHASE_STOP_DISTANCE = 150.f;
    constexpr float PROJECTILE_SPEED = 260.f;
    constexpr float PROJECTILE_HIT_RADIUS = 16.f;
    const sf::Vector2f PROJECTILE_VISUAL_SIZE(24.f, 24.f);
    // Обрезка из скачанного пака эффектов (Resources/Effect/Fire Effect and Bullet 16x16.png) — пульсирующий
    // огненный шар, 4 кадра 16x16 подряд по горизонтали (см. SpriteComponent::loadAnimation) — "плевок" слизи
    // огненный, а не ядовитый/кислотный, цвет должен читаться как атака, а не как декоративный зелёный.
    const std::string PROJECTILE_TEXTURE = "Resources/Characters/Slime/Slime3/Spit/Slime3_Spit_Bullet.png";
    constexpr int PROJECTILE_FRAME_COUNT = 4;
    const sf::Time PROJECTILE_FRAME_DURATION = sf::seconds(0.09f);

    // Деление (Slime2, см. SlimeSplitComponent): дети — та же расцветка, слабее и меньше, сами не делятся.
    constexpr int SPLIT_CHILD_COUNT = 2;
    constexpr int SPLIT_CHILD_MAX_HP = 1;
    constexpr float SPLIT_CHILD_VISUAL_SCALE = 0.65f;
    constexpr float SPLIT_SPREAD_RADIUS = 26.f;
    // Примерно длительность ролика смерти (DEATH_FRAME_COUNT=10 * 0.08с ниже) — дети появляются, когда труп уже
    // доиграл, а не мгновенно поверх ещё идущей анимации.
    const sf::Time SPLIT_DELAY = sf::seconds(0.8f);

    // Кадр пака Slime — 64x64, лист поделён на 4 строки (см. SpriteComponent::loadAnimation row/rowCount); строку
    // берём всегда 0-ю (с мордочкой) — у круглой слизи нет смысла честно поворачивать спрайт по направлению.
    constexpr int SLIME_ROW = 0;
    constexpr int SLIME_ROW_COUNT = 4;

    const sf::Vector2f VISUAL_SIZE(96.f, 96.f);

    // Idle/Walk/Hurt/Death совпадают по числу кадров у всех трёх расцветок — только Attack отличается (10/11/9,
    // см. attackFrameCount), поэтому его вынесли отдельной функцией, а не константой.
    constexpr int IDLE_FRAME_COUNT = 6;
    constexpr int WALK_FRAME_COUNT = 8;
    constexpr int HURT_FRAME_COUNT = 5;
    constexpr int DEATH_FRAME_COUNT = 10;
    constexpr float HURT_FRAME_DURATION = 0.08f;

    // Проверено по факту (см. размеры файлов Slime{1,2,3}_Attack_body.png, 64px/кадр): 640/704/576px.
    int attackFrameCount(const std::string& skin)
    {
        if (skin == "Slime2") {
            return 11;
        }
        if (skin == "Slime3") {
            return 9;
        }
        return 10; // Slime1 и любая ещё не заведённая расцветка.
    }

    ActorAnimationConfig buildAnimationConfig(std::function<bool()> consumeJustTriggered, const std::string& skin)
    {
        std::string dir = "Resources/Characters/Slime/" + skin + "/";
        std::string prefix = skin + "_";
        int attackFrames = attackFrameCount(skin);

        ActorAnimationConfig config;
        config.idle
            = {dir + "Idle/" + prefix + "Idle_body.png", IDLE_FRAME_COUNT, sf::seconds(0.15f), true, SLIME_ROW, SLIME_ROW_COUNT};
        config.walk
            = {dir + "Walk/" + prefix + "Walk_body.png", WALK_FRAME_COUNT, sf::seconds(0.1f), true, SLIME_ROW, SLIME_ROW_COUNT};
        config.hurt = {dir + "Hurt/" + prefix + "Hurt_body.png", HURT_FRAME_COUNT, sf::seconds(HURT_FRAME_DURATION), false,
            SLIME_ROW, SLIME_ROW_COUNT};
        config.hurtVisualDuration = sf::seconds(HURT_FRAME_DURATION * HURT_FRAME_COUNT);
        config.death = {
            dir + "Death/" + prefix + "Death_body.png", DEATH_FRAME_COUNT, sf::seconds(0.08f), false, SLIME_ROW, SLIME_ROW_COUNT};
        config.normalShadow
            = {dir + "Idle/" + prefix + "Idle_shadow.png", IDLE_FRAME_COUNT, sf::Time::Zero, true, SLIME_ROW, SLIME_ROW_COUNT};
        config.deathShadow = {dir + "Death/" + prefix + "Death_shadow.png", DEATH_FRAME_COUNT, sf::seconds(0.08f), false,
            SLIME_ROW, SLIME_ROW_COUNT};

        ActorAttackAnim attackAnim;
        attackAnim.consumeJustTriggered = std::move(consumeJustTriggered);
        attackAnim.clips = {{dir + "Attack/" + prefix + "Attack_body.png", attackFrames, sf::seconds(ATTACK_FRAME_DURATION),
            false, SLIME_ROW, SLIME_ROW_COUNT}};
        attackAnim.visualDuration = sf::seconds(ATTACK_FRAME_DURATION * attackFrames);
        config.attacks.push_back(std::move(attackAnim));

        return config;
    }
} // namespace

Slime::Slime(sf::Vector2f position, sf::Vector2f size, float speed, float detectionRadius, SlimeConfig config)
    : GameObject(position)
{
    float chaseStopDistance = config.isRanged ? RANGED_CHASE_STOP_DISTANCE : MELEE_CHASE_STOP_DISTANCE;

    // Порядок как у Enemy/Soldier: EnemyBehaviorComponent (Patrol/Chase/Alert/Rest, тут ещё omnidirectionalVision=true
    // — видит игрока по всем сторонам, не только в конусе, независимо от типа атаки) должен успеть позвать
    // ChaseComponent::setSeekOverride() до того, как в этом же кадре отработает сам ChaseComponent.
    addComponent<EnemyBehaviorComponent>(detectionRadius, SLIME_PATROL_RADIUS, config.skin, true);
    addComponent<ChaseComponent>(detectionRadius, chaseStopDistance);
    addComponent<MovementComponent>(speed);

    std::string dir = "Resources/Characters/Slime/" + config.skin + "/";
    std::string prefix = config.skin + "_";
    sf::Vector2f visualSize = VISUAL_SIZE * config.visualScale;

    m_shadowSprite = &addComponent<SpriteComponent>(visualSize);
    m_shadowSprite->setPlaceholderColor(sf::Color(0, 0, 0, 90));
    m_shadowSprite->loadAnimation(
        dir + "Idle/" + prefix + "Idle_shadow.png", IDLE_FRAME_COUNT, sf::Time::Zero, true, SLIME_ROW, SLIME_ROW_COUNT);
    m_shadowSprite->setColor(sf::Color(0, 0, 0, 140));

    m_bodySprite = &addComponent<SpriteComponent>(visualSize);
    m_bodySprite->setPlaceholderColor(sf::Color(120, 220, 160));
    m_bodySprite->loadAnimation(
        dir + "Idle/" + prefix + "Idle_body.png", IDLE_FRAME_COUNT, sf::seconds(0.15f), true, SLIME_ROW, SLIME_ROW_COUNT);

    addComponent<ColliderComponent>(size, false);

    // Общий приём Enemy/Soldier/Slime (см. ActorSpawnHelpers.h) — ловит GameException на некорректные
    // maxHp/armor и откатывается на безопасные дефолты 1/0.
    HealthComponent& health = addHealthComponentWithFallback(*this, config.maxHp, SLIME_ARMOR, "Slime");
    addComponent<DeathParticleComponent>(health);

    // targetFilter ограничивает цель игроком, чтобы слизь не задела другого бота, случайно оказавшегося рядом.
    auto onlyPlayer = [](GameObject* target) { return target->getComponent<ChaseTargetComponent>() != nullptr; };

    std::function<bool()> consumeJustTriggered;
    if (config.isRanged) {
        // "Плевок" (Slime3) — направленный снаряд, держит дистанцию (см. RANGED_CHASE_STOP_DISTANCE выше).
        RangedAttackComponent& ranged = addComponent<RangedAttackComponent>(config.skin, SLIME_DAMAGE, RANGED_MIN_RANGE,
            RANGED_MAX_RANGE, RANGED_COOLDOWN, PROJECTILE_SPEED, PROJECTILE_HIT_RADIUS, PROJECTILE_TEXTURE,
            PROJECTILE_VISUAL_SIZE, HIT_OR_SHOT_DELAY, onlyPlayer, /*autoFire=*/true,
            /*requireTarget=*/true, PROJECTILE_FRAME_COUNT, PROJECTILE_FRAME_DURATION);
        // Ролик выстрела — тот же общий "всплеск шипов" вокруг себя, что и у ближнего боя (см. Attack_body.png),
        // так что помимо самого снаряда в тот же момент ещё и бьёт по кругу вокруг себя (иначе всплеск выглядел
        // бы чисто декоративным, без урона, странно). autoAttack=false — сама по кулдауну не запускается, её
        // дёргает та же лямбда, что двигает анимацию, синхронно с моментом выстрела, а не отдельным таймером.
        AttackComponent& burst = addComponent<AttackComponent>(
            config.skin + "-Burst", SLIME_DAMAGE, MELEE_RANGE, sf::Time::Zero, false, sf::Time::Zero, onlyPlayer, true, true);
        // Считаем выстрелы тут же, в этой же lambda — SlimeShotLimitComponent сам ranged.consumeJustFired() не
        // читает, тот "съедает" флаг за один вызов (см. класс-комментарий SlimeShotLimitComponent.h).
        SlimeShotLimitComponent* shotLimit
            = config.maxShotsBeforeDeath > 0 ? &addComponent<SlimeShotLimitComponent>(config.maxShotsBeforeDeath) : nullptr;
        consumeJustTriggered = [&ranged, &burst, shotLimit] {
            bool justFired = ranged.consumeJustFired();
            if (justFired) {
                burst.tryAttack();
                if (shotLimit) {
                    shotLimit->notifyShotFired();
                }
            }
            return justFired;
        };
    } else {
        // Ближний бой по кругу (Slime1, дети Slime2) — omnidirectional=true, конус facing не проверяется.
        AttackComponent& attack = addComponent<AttackComponent>(
            config.skin, SLIME_DAMAGE, MELEE_RANGE, MELEE_COOLDOWN, true, HIT_OR_SHOT_DELAY, onlyPlayer, true, true);
        consumeJustTriggered = [&attack] { return attack.consumeJustStarted(); };
    }

    addComponent<HitFlashComponent>(*m_bodySprite, sf::seconds(0.3f), sf::seconds(0.06f), sf::Color(255, 60, 60));
    addComponent<ActorAnimationComponent>(buildAnimationConfig(std::move(consumeJustTriggered), config.skin));

    if (config.canSplit) {
        addComponent<SlimeSplitComponent>(config.skin, SPLIT_CHILD_MAX_HP, SPLIT_CHILD_VISUAL_SCALE, speed, detectionRadius,
            *config.childSpawnParent, SPLIT_CHILD_COUNT, SPLIT_SPREAD_RADIUS, SPLIT_DELAY);
    }

    LOG_INFO(config.skin + " создан на позиции (" + std::to_string(position.x) + ", " + std::to_string(position.y)
             + "), радиус обнаружения " + std::to_string(detectionRadius));
}
