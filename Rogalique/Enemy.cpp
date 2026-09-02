#include "Enemy.h"
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
#include "SpriteComponent.h"

namespace
{
    // Насколько далеко от точки спавна орк готов забредать в патруле (см. EnemyBehaviorComponent).
    constexpr float ENEMY_PATROL_RADIUS = 150.f;

    // Игрок бьёт на 2 (см. PLAYER_ATTACK_DAMAGE в Player.cpp), брони у орка нет — 6 HP это ровно 3 удара до смерти.
    constexpr int ENEMY_MAX_HP = 6;
    constexpr int ENEMY_ARMOR = 0;

    constexpr int ENEMY_ATTACK_DAMAGE = 2;
    constexpr float ENEMY_ATTACK_RANGE = 40.f;
    // Чуть меньше радиуса атаки — останавливается, уже стоя в зоне удара, а не наваливаясь на цель вплотную.
    constexpr float ENEMY_CHASE_STOP_DISTANCE = ENEMY_ATTACK_RANGE - 10.f;
    const sf::Time ENEMY_ATTACK_COOLDOWN = sf::seconds(0.8f);
    // Урон применяется не мгновенно, а на кадре 3 (считая с 0) ролика Orc-Attack01 — там на кадрах видна дуга
    // топора, доходящая до цели, кадры 0-2 это замах. Тот же кадр длится 0.07с.
    const sf::Time ENEMY_ATTACK_HIT_DELAY = sf::seconds(0.07f * 3);

    // Кадр пака Orc — 100x100, квадратный (у протагониста 48x64). 256px — вдвое больше высоты игрока (128px, см. VISUAL_SIZE в Player.cpp).
    const sf::Vector2f VISUAL_SIZE(256.f, 256.f);

    constexpr int IDLE_FRAME_COUNT = 6;
    constexpr int WALK_FRAME_COUNT = 8;
    constexpr int ATTACK_FRAME_COUNT = 6;
    constexpr int HURT_FRAME_COUNT = 4;
    constexpr int DEATH_FRAME_COUNT = 4;
    constexpr float ATTACK_FRAME_DURATION = 0.07f;
    constexpr float HURT_FRAME_DURATION = 0.08f;

    const std::string BASE_DIR = "Resources/Characters/Orc/Orc/";

    // Анимации Idle/Walk/Attack/Hurt/Death по состоянию ChaseComponent/HealthComponent/AttackComponent (см.
    // ActorAnimationComponent) — тут собирается только конфиг, вся логика приоритета состояний общая для всех
    // существ этого вида (Enemy/Soldier/Slime).
    ActorAnimationConfig buildAnimationConfig(AttackComponent& attack)
    {
        ActorAnimationConfig config;
        config.idle = {BASE_DIR + "Orc-Idle.png", IDLE_FRAME_COUNT, sf::seconds(0.15f), true};
        config.walk = {BASE_DIR + "Orc-Walk.png", WALK_FRAME_COUNT, sf::seconds(0.1f), true};
        config.hurt = {BASE_DIR + "Orc-Hurt.png", HURT_FRAME_COUNT, sf::seconds(HURT_FRAME_DURATION), false};
        config.hurtVisualDuration = sf::seconds(HURT_FRAME_DURATION * HURT_FRAME_COUNT);
        config.death = {BASE_DIR + "Orc-Death.png", DEATH_FRAME_COUNT, sf::seconds(0.15f), false};
        config.normalShadow = {BASE_DIR + "Shadow sprites/Orc-shadow.png", 1, sf::Time::Zero, true};
        config.deathShadow = {BASE_DIR + "Shadow sprites/Orc-shadow_death.png", DEATH_FRAME_COUNT, sf::seconds(0.15f), false};

        ActorAttackAnim meleeAnim;
        meleeAnim.consumeJustTriggered = [&attack] { return attack.consumeJustStarted(); };
        // Два клипа — чередуются между последовательными ударами ради разнообразия, как у Soldier. Attack02 бьёт с
        // другой стороны, поэтому у него отдельная (тоже анимированная, 6 кадров) тень — Attack01 обходится обычной
        // статичной normalShadow (своей анимированной тени для него в паке нет).
        meleeAnim.clips = {
            {BASE_DIR + "Orc-Attack01.png", ATTACK_FRAME_COUNT, sf::seconds(ATTACK_FRAME_DURATION), false},
            {BASE_DIR + "Orc-Attack02.png", ATTACK_FRAME_COUNT, sf::seconds(ATTACK_FRAME_DURATION), false},
        };
        meleeAnim.shadowOverrides = {
            {},
            {BASE_DIR + "Shadow sprites/Orc-shadow_attack02.png", ATTACK_FRAME_COUNT, sf::seconds(ATTACK_FRAME_DURATION), false},
        };
        meleeAnim.visualDuration = sf::seconds(ATTACK_FRAME_DURATION * ATTACK_FRAME_COUNT);
        config.attacks.push_back(std::move(meleeAnim));

        return config;
    }
} // namespace

Enemy::Enemy(sf::Vector2f position, sf::Vector2f size, float speed, float detectionRadius)
    : GameObject(position)
{
    // Добавлен раньше ChaseComponent специально: его update() должен успеть позвать ChaseComponent::setSeekOverride()
    // (патруль/тревога) ДО того, как в этом же кадре отработает сам ChaseComponent — порядок обновления компонентов
    // на объекте это порядок addComponent() (см. GameObject::update()).
    addComponent<EnemyBehaviorComponent>(detectionRadius, ENEMY_PATROL_RADIUS, "Orc");
    addComponent<ChaseComponent>(detectionRadius, ENEMY_CHASE_STOP_DISTANCE);
    addComponent<MovementComponent>(speed);

    // Тень рисуется первой (значит под телом), тело — вторым поверх неё — та же схема, что у Player.
    m_shadowSprite = &addComponent<SpriteComponent>(VISUAL_SIZE);
    m_shadowSprite->setPlaceholderColor(sf::Color(0, 0, 0, 90));
    m_shadowSprite->loadAnimation(BASE_DIR + "Shadow sprites/Orc-shadow.png", 1, sf::Time::Zero, true);
    m_shadowSprite->setColor(sf::Color(0, 0, 0, 140));

    m_bodySprite = &addComponent<SpriteComponent>(VISUAL_SIZE);
    m_bodySprite->setPlaceholderColor(sf::Color::Red);
    m_bodySprite->loadAnimation(BASE_DIR + "Orc-Idle.png", IDLE_FRAME_COUNT, sf::seconds(0.15f), true);

    addComponent<ColliderComponent>(size, false);

    // Общий приём Enemy/Soldier/Slime (см. ActorSpawnHelpers.h, почему вынесен) — ловит GameException на
    // некорректные maxHp/armor и откатывается на безопасные дефолты 1/0.
    HealthComponent& health = addHealthComponentWithFallback(*this, ENEMY_MAX_HP, ENEMY_ARMOR, "Enemy");
    addComponent<DeathParticleComponent>(health);

    // autoAttack=true — бьёт игрока сам, как только тот окажется в радиусе атаки и кулдаун пройдёт. hitDelay синхронизирует момент урона с кадром замаха анимации.
    // targetFilter ограничивает цель игроком (ChaseTargetComponent), чтобы орк не задел другого бота (Soldier), который может оказаться в радиусе удара.
    AttackComponent& attack
        = addComponent<AttackComponent>("Enemy", ENEMY_ATTACK_DAMAGE, ENEMY_ATTACK_RANGE, ENEMY_ATTACK_COOLDOWN, true,
            ENEMY_ATTACK_HIT_DELAY, [](GameObject* target) { return target->getComponent<ChaseTargetComponent>() != nullptr; });
    addComponent<HitFlashComponent>(*m_bodySprite, sf::seconds(0.3f), sf::seconds(0.06f), sf::Color(255, 60, 60));
    addComponent<ActorAnimationComponent>(buildAnimationConfig(attack));

    LOG_INFO("Enemy создан на позиции (" + std::to_string(position.x) + ", " + std::to_string(position.y)
             + "), радиус обнаружения " + std::to_string(detectionRadius));
}
