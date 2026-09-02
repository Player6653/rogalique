#include "VampireSpawnMinion.h"
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
    constexpr float PATROL_RADIUS = 80.f;

    // Слабее рядового Enemy (6 HP/2 урона, см. Enemy.cpp) — расходное подкрепление босса, не самостоятельная угроза.
    constexpr int MAX_HP = 3;
    constexpr int ARMOR = 0;
    constexpr int MELEE_DAMAGE = 1;
    constexpr float MELEE_RANGE = 40.f;
    const sf::Time MELEE_COOLDOWN = sf::seconds(0.9f);
    const sf::Time MELEE_HIT_DELAY = sf::seconds(0.18f);

    const sf::Vector2f VISUAL_SIZE(72.f, 72.f);
    // См. Boss.cpp — тот же приём (SpriteComponent::setPositionOffset), те же измерения по факту: непрозрачные
    // пиксели в кадре vampire_spawn_*_IDLE.png лежат в y=[2..15] из 16, низ ног на 7px ниже центра кадра (8), при
    // масштабе VISUAL_SIZE/16=4.5 это ~32 мировых пикселя вниз от центра GameObject.
    const sf::Vector2f SHADOW_SIZE(24.f, 8.f);
    constexpr float SHADOW_OFFSET_Y = 32.f;

    const std::string BASE_DIR = "Resources/Characters/The Vampire Lord & Spawns/Vampire Spawns/";
    // Лист 4-строчный (16x16 на кадр) — как и у Boss/Slime, используем только строку 0, авто-флип по X через
    // ActorAnimationComponent берёт на себя лево/право.
    constexpr int BODY_ROW_COUNT = 4;
    constexpr int IDLE_FRAME_COUNT = 4;
    constexpr int MOVE_FRAME_COUNT = 4;
    constexpr int MELEE_FRAME_COUNT = 4;
    // DEATH — лист 80x32 (не 64), у него всего 2 строки, не 4 — единственный ролик без общих BODY_ROW_COUNT.
    constexpr int DEATH_FRAME_COUNT = 5;
    constexpr int DEATH_ROW_COUNT = 2;

    // Папки в паке — "Fem. Vampire Spawn"/"Masc. Vampire Spawn" (с заглавной и точкой), а не просто "fem"/"masc" —
    // это только префикс имени файла внутри неё (vampire_spawn_fem_IDLE.png и т.п.), два разных именования.
    std::string folderName(const std::string& skin)
    {
        return skin == "fem" ? "Fem. Vampire Spawn" : "Masc. Vampire Spawn";
    }

    ActorAnimationConfig buildAnimationConfig(std::function<bool()> consumeMeleeJustTriggered, const std::string& skin)
    {
        std::string dir = BASE_DIR + folderName(skin) + "/";
        std::string prefix = "vampire_spawn_" + skin + "_";

        ActorAnimationConfig config;
        config.idle = {dir + prefix + "IDLE.png", IDLE_FRAME_COUNT, sf::seconds(0.15f), true, 0, BODY_ROW_COUNT};
        config.walk = {dir + prefix + "MOVE.png", MOVE_FRAME_COUNT, sf::seconds(0.1f), true, 0, BODY_ROW_COUNT};
        // См. Boss.cpp — Hurt у ботов без постхитовой неуязвимости никогда реально не показывается.
        config.hurt = config.idle;
        config.hurtVisualDuration = sf::Time::Zero;
        config.death = {dir + prefix + "DEATH.png", DEATH_FRAME_COUNT, sf::seconds(0.1f), false, 0, DEATH_ROW_COUNT};
        // Своей тени в паке нет — пустой путь откатывается на цветной плейсхолдер SpriteComponent (см. конструктор).
        config.normalShadow = {"", 1, sf::Time::Zero, true};
        config.deathShadow = {"", 1, sf::Time::Zero, false};

        ActorAttackAnim meleeAnim;
        meleeAnim.consumeJustTriggered = std::move(consumeMeleeJustTriggered);
        meleeAnim.clips = {{dir + prefix + "MELEE.png", MELEE_FRAME_COUNT, sf::seconds(0.07f), false, 0, BODY_ROW_COUNT}};
        meleeAnim.visualDuration = sf::seconds(0.07f * MELEE_FRAME_COUNT);
        config.attacks.push_back(std::move(meleeAnim));

        return config;
    }
} // namespace

VampireSpawnMinion::VampireSpawnMinion(sf::Vector2f position, sf::Vector2f size, float speed, float detectionRadius, bool feminine)
    : GameObject(position)
{
    std::string skin = feminine ? "fem" : "masc";

    addComponent<EnemyBehaviorComponent>(detectionRadius, PATROL_RADIUS, "VampireSpawn", true);
    addComponent<ChaseComponent>(detectionRadius, MELEE_RANGE - 10.f);
    addComponent<MovementComponent>(speed);

    m_shadowSprite = &addComponent<SpriteComponent>(SHADOW_SIZE);
    m_shadowSprite->setPlaceholderColor(sf::Color(0, 0, 0, 90));
    m_shadowSprite->setPositionOffset(sf::Vector2f(0.f, SHADOW_OFFSET_Y));

    m_bodySprite = &addComponent<SpriteComponent>(VISUAL_SIZE);
    m_bodySprite->setPlaceholderColor(sf::Color(120, 40, 90));

    addComponent<ColliderComponent>(size, false);

    HealthComponent& health = addHealthComponentWithFallback(*this, MAX_HP, ARMOR, "VampireSpawnMinion");
    addComponent<DeathParticleComponent>(health);

    auto onlyPlayer = [](GameObject* target) { return target->getComponent<ChaseTargetComponent>() != nullptr; };
    AttackComponent& attack = addComponent<AttackComponent>(
        "VampireSpawn", MELEE_DAMAGE, MELEE_RANGE, MELEE_COOLDOWN, true, MELEE_HIT_DELAY, onlyPlayer, true, true);

    addComponent<HitFlashComponent>(*m_bodySprite, sf::seconds(0.3f), sf::seconds(0.06f), sf::Color(255, 60, 60));
    addComponent<ActorAnimationComponent>(buildAnimationConfig([&attack] { return attack.consumeJustStarted(); }, skin));

    LOG_INFO("VampireSpawnMinion (" + skin + ") создан на позиции (" + std::to_string(position.x) + ", "
             + std::to_string(position.y) + ")");
}
