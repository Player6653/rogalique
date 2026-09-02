#include "Soldier.h"
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
#include "SoldierAmmoComponent.h"
#include "SpriteComponent.h"

namespace
{
    // Насколько далеко от точки спавна Soldier готов забредать в патруле (см. EnemyBehaviorComponent).
    constexpr float SOLDIER_PATROL_RADIUS = 150.f;

    // 4 HP без брони — 2 удара игрока (тот бьёт на 2, см. PLAYER_ATTACK_DAMAGE в Player.cpp).
    constexpr int SOLDIER_MAX_HP = 4;
    constexpr int SOLDIER_ARMOR = 0;

    // Ближний бой — вплотную, как у Enemy: тот же радиус и та же логика AttackComponent. Урон намеренно ниже, чем
    // у орка (см. ENEMY_ATTACK_DAMAGE в Enemy.cpp) — орк единственный "тяжёлый" противник в игре, баланс урона.
    constexpr int SOLDIER_MELEE_DAMAGE = 1;
    constexpr float SOLDIER_MELEE_RANGE = 40.f;
    const sf::Time SOLDIER_MELEE_COOLDOWN = sf::seconds(0.8f);
    // Кадр удара мечом (Soldier-Attack01, 6 кадров по 0.07с) — урон на кадре 3, как у Enemy/Orc-Attack01.
    const sf::Time SOLDIER_MELEE_HIT_DELAY = sf::seconds(0.07f * 3);

    // Дальний бой — стрела летит, пока не попадёт или не пролетит maxRange. Тот же баланс, что у ближнего боя выше.
    constexpr int SOLDIER_RANGED_DAMAGE = 1;
    // Не стреляет в упор — на такой дистанции работает меч (см. SOLDIER_MELEE_RANGE выше).
    constexpr float SOLDIER_RANGED_MIN_RANGE = SOLDIER_MELEE_RANGE;
    constexpr float SOLDIER_RANGED_MAX_RANGE = 260.f;
    const sf::Time SOLDIER_RANGED_COOLDOWN = sf::seconds(1.4f);
    // Стрела вылетает не сразу, а на кадре 6 из 9 ролика Soldier-Attack03 (0.09с/кадр) — там лук уже натянут до конца.
    const sf::Time SOLDIER_RANGED_SHOT_DELAY = sf::seconds(0.09f * 6);
    // Стрелы не бесконечные — см. SoldierAmmoComponent: кончились, идёт за ArrowCrate поблизости, а если такого
    // нет — сближается и бьётся мечом.
    constexpr int SOLDIER_MAX_ARROWS = 15;
    constexpr float PROJECTILE_SPEED = 380.f;
    constexpr float PROJECTILE_HIT_RADIUS = 20.f;
    const sf::Vector2f PROJECTILE_VISUAL_SIZE(40.f, 40.f);
    const char* PROJECTILE_TEXTURE_PATH = "Resources/Characters/Soldier/Arrow(projectile)/Arrow01(32x32).png";

    // Останавливается на дистанции стрельбы, вплотную идёт только если цель сама подойдёт близко.
    constexpr float SOLDIER_CHASE_STOP_DISTANCE = 200.f;

    // Кадр пака Soldier — тоже 100x100, как у Orc.
    const sf::Vector2f VISUAL_SIZE(256.f, 256.f);

    constexpr int IDLE_FRAME_COUNT = 6;
    constexpr int WALK_FRAME_COUNT = 8;
    constexpr int MELEE_FRAME_COUNT = 6;
    constexpr int RANGED_FRAME_COUNT = 9;
    constexpr int HURT_FRAME_COUNT = 4;
    constexpr int DEATH_FRAME_COUNT = 4;
    constexpr float MELEE_FRAME_DURATION = 0.07f;
    // Совпадает с SOLDIER_RANGED_SHOT_DELAY выше — там стрела вылетает на кадре 6 из 9, тут ролик доигрывает до конца.
    constexpr float RANGED_FRAME_DURATION = 0.09f;
    constexpr float HURT_FRAME_DURATION = 0.08f;

    const std::string BASE_DIR = "Resources/Characters/Soldier/Soldier/";

    // Приоритет (см. ActorAnimationComponent): Death > Hurt > ближний бой (чередует Attack01/02) > дальний бой > Walk > Idle —
    // порядок в config.attacks ниже задаёт именно этот приоритет между двумя видами атаки.
    ActorAnimationConfig buildAnimationConfig(AttackComponent& melee, RangedAttackComponent& ranged)
    {
        ActorAnimationConfig config;
        config.idle = {BASE_DIR + "Soldier-Idle.png", IDLE_FRAME_COUNT, sf::seconds(0.15f), true};
        config.walk = {BASE_DIR + "Soldier-Walk.png", WALK_FRAME_COUNT, sf::seconds(0.1f), true};
        config.hurt = {BASE_DIR + "Soldier-Hurt.png", HURT_FRAME_COUNT, sf::seconds(HURT_FRAME_DURATION), false};
        config.hurtVisualDuration = sf::seconds(HURT_FRAME_DURATION * HURT_FRAME_COUNT);
        config.death = {BASE_DIR + "Soldier-Death.png", DEATH_FRAME_COUNT, sf::seconds(0.15f), false};
        config.normalShadow = {BASE_DIR + "Shadow sprites/Soldier-Shadow.png", 1, sf::Time::Zero, true};
        config.deathShadow = {BASE_DIR + "Shadow sprites/Soldier-Shadow_death.png", DEATH_FRAME_COUNT, sf::seconds(0.15f), false};

        ActorAttackAnim meleeAnim;
        meleeAnim.consumeJustTriggered = [&melee] { return melee.consumeJustStarted(); };
        // Два клипа — чередуются между последовательными ударами ради разнообразия (см. ActorAnimationComponent).
        meleeAnim.clips = {
            {BASE_DIR + "Soldier-Attack01.png", MELEE_FRAME_COUNT, sf::seconds(MELEE_FRAME_DURATION), false},
            {BASE_DIR + "Soldier-Attack02.png", MELEE_FRAME_COUNT, sf::seconds(MELEE_FRAME_DURATION), false},
        };
        meleeAnim.visualDuration = sf::seconds(MELEE_FRAME_DURATION * MELEE_FRAME_COUNT);
        config.attacks.push_back(std::move(meleeAnim));

        ActorAttackAnim rangedAnim;
        rangedAnim.consumeJustTriggered = [&ranged] { return ranged.consumeJustFired(); };
        rangedAnim.clips = {{BASE_DIR + "Soldier-Attack03.png", RANGED_FRAME_COUNT, sf::seconds(RANGED_FRAME_DURATION), false}};
        rangedAnim.visualDuration = sf::seconds(RANGED_FRAME_DURATION * RANGED_FRAME_COUNT);
        config.attacks.push_back(std::move(rangedAnim));

        return config;
    }
} // namespace

Soldier::Soldier(sf::Vector2f position, sf::Vector2f size, float speed, float detectionRadius)
    : GameObject(position)
{
    // Порядок важен (см. GameObject::update() — компоненты обновляются в порядке addComponent()): сперва
    // EnemyBehaviorComponent решает Patrol/Chase/Alert, затем SoldierAmmoComponent может ПЕРЕБИТЬ это решение
    // (кончились стрелы — идти за ящиком/врукопашную важнее патруля), и оба должны успеть позвать
    // ChaseComponent::setSeekOverride() до того, как в этом же кадре отработает сам ChaseComponent.
    addComponent<EnemyBehaviorComponent>(detectionRadius, SOLDIER_PATROL_RADIUS, "Soldier");
    addComponent<SoldierAmmoComponent>(SOLDIER_MAX_ARROWS);
    addComponent<ChaseComponent>(detectionRadius, SOLDIER_CHASE_STOP_DISTANCE);
    addComponent<MovementComponent>(speed);

    m_shadowSprite = &addComponent<SpriteComponent>(VISUAL_SIZE);
    m_shadowSprite->setPlaceholderColor(sf::Color(0, 0, 0, 90));
    m_shadowSprite->loadAnimation(BASE_DIR + "Shadow sprites/Soldier-Shadow.png", 1, sf::Time::Zero, true);
    m_shadowSprite->setColor(sf::Color(0, 0, 0, 140));

    m_bodySprite = &addComponent<SpriteComponent>(VISUAL_SIZE);
    m_bodySprite->setPlaceholderColor(sf::Color(80, 120, 220));
    m_bodySprite->loadAnimation(BASE_DIR + "Soldier-Idle.png", IDLE_FRAME_COUNT, sf::seconds(0.15f), true);

    addComponent<ColliderComponent>(size, false);

    // Общий приём Enemy/Soldier/Slime (см. ActorSpawnHelpers.h) — ловит GameException на некорректные
    // maxHp/armor и откатывается на безопасные дефолты 1/0.
    HealthComponent& health = addHealthComponentWithFallback(*this, SOLDIER_MAX_HP, SOLDIER_ARMOR, "Soldier");
    addComponent<DeathParticleComponent>(health);

    // targetFilter у обеих атак ограничивает цель игроком, чтобы Soldier не бил орка, если тот случайно окажется рядом.
    auto onlyPlayer = [](GameObject* target) { return target->getComponent<ChaseTargetComponent>() != nullptr; };
    AttackComponent& melee = addComponent<AttackComponent>("Soldier-Melee", SOLDIER_MELEE_DAMAGE, SOLDIER_MELEE_RANGE,
        SOLDIER_MELEE_COOLDOWN, true, SOLDIER_MELEE_HIT_DELAY, onlyPlayer);
    // autoFire=false — стрелять по ним теперь заведует SoldierAmmoComponent (тратит стрелы), а не сам update().
    RangedAttackComponent& ranged = addComponent<RangedAttackComponent>("Soldier-Ranged", SOLDIER_RANGED_DAMAGE,
        SOLDIER_RANGED_MIN_RANGE, SOLDIER_RANGED_MAX_RANGE, SOLDIER_RANGED_COOLDOWN, PROJECTILE_SPEED, PROJECTILE_HIT_RADIUS,
        PROJECTILE_TEXTURE_PATH, PROJECTILE_VISUAL_SIZE, SOLDIER_RANGED_SHOT_DELAY, onlyPlayer, false);
    addComponent<HitFlashComponent>(*m_bodySprite, sf::seconds(0.3f), sf::seconds(0.06f), sf::Color(255, 60, 60));
    addComponent<ActorAnimationComponent>(buildAnimationConfig(melee, ranged));

    LOG_INFO("Soldier создан на позиции (" + std::to_string(position.x) + ", " + std::to_string(position.y)
             + "), радиус обнаружения " + std::to_string(detectionRadius));
}
