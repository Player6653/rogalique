#include "Boss.h"
#include "ActorSpawnHelpers.h"
#include "AttackComponent.h"
#include "BossMinionSummonComponent.h"
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
#include "SpriteComponent.h"

namespace
{
    // Заметно больше и живучее любого рядового врага (Enemy — 6 HP/2 урона, см. ENEMY_MAX_HP в Enemy.cpp) — босс
    // арены, последняя волна. Пока заглушка: реальный баланс имеет смысл подбирать только на настоящем спрайт-листе
    // "Vampire Lord" (см. класс-комментарий Boss.h), сейчас числа ориентировочные.
    constexpr int BOSS_MAX_HP = 50;
    constexpr int BOSS_ARMOR = 3;
    constexpr float BOSS_PATROL_RADIUS = 40.f;

    constexpr int BOSS_MELEE_DAMAGE = 5;
    constexpr float BOSS_MELEE_RANGE = 70.f;
    const sf::Time BOSS_MELEE_COOLDOWN = sf::seconds(0.9f);
    const sf::Time BOSS_MELEE_HIT_DELAY = sf::seconds(0.25f);

    // Кольцо дальней атаки начинается за пределами MELEE_RANGE — не стреляет в упор, там уже работает ближний бой.
    constexpr int BOSS_RANGED_DAMAGE = 3;
    constexpr float BOSS_RANGED_MIN_RANGE = BOSS_MELEE_RANGE;
    constexpr float BOSS_RANGED_MAX_RANGE = 280.f;
    const sf::Time BOSS_RANGED_COOLDOWN = sf::seconds(2.f);
    const sf::Time BOSS_RANGED_SHOT_DELAY = sf::seconds(0.3f);
    constexpr float PROJECTILE_SPEED = 240.f;
    constexpr float PROJECTILE_HIT_RADIUS = 18.f;
    const sf::Vector2f PROJECTILE_VISUAL_SIZE(32.f, 32.f);
    // Переиспользуем уже готовый "плевок" Slime3 (см. PROJECTILE_TEXTURE в Slime.cpp) — тот же приём placeholder'а,
    // что и у самого босса: не тратить время на новую нарезку ради заглушки, которую заменит купленный пак.
    const std::string PROJECTILE_TEXTURE = "Resources/Characters/Slime/Slime3/Spit/Slime3_Spit_Bullet.png";
    constexpr int PROJECTILE_FRAME_COUNT = 4;
    const sf::Time PROJECTILE_FRAME_DURATION = sf::seconds(0.09f);

    // Подкрепление: раз в SUMMON_INTERVAL, если живых миньонов меньше SUMMON_MAX_ALIVE — новый (см.
    // BossMinionSummonComponent), появляется в кольце SUMMON_SPAWN_RADIUS вокруг босса.
    const sf::Time SUMMON_INTERVAL = sf::seconds(12.f);
    constexpr int SUMMON_MAX_ALIVE = 2;
    constexpr float SUMMON_SPAWN_RADIUS = 90.f;
} // namespace

Boss::Boss(sf::Vector2f position, sf::Vector2f size, float speed, float detectionRadius,
    std::function<GameObject*(sf::Vector2f)> spawnMinion)
    : GameObject(position)
{
    // Порядок как у Enemy/Soldier/Slime: EnemyBehaviorComponent должен успеть позвать
    // ChaseComponent::setSeekOverride() до того, как в этом же кадре отработает сам ChaseComponent.
    addComponent<EnemyBehaviorComponent>(detectionRadius, BOSS_PATROL_RADIUS, "Boss", true);
    addComponent<ChaseComponent>(detectionRadius, BOSS_RANGED_MIN_RANGE - 10.f);
    addComponent<MovementComponent>(speed);

    // Тень под телом, как у остальных существ — оба чисто цветные плейсхолдеры (setPlaceholderColor), loadAnimation
    // не зовём: настоящей текстуры для босса ещё нет (см. класс-комментарий Boss.h), рисовать нечего, кроме
    // сплошного прямоугольника. SpriteComponent::draw() сам переключается на m_placeholder, пока m_hasTexture=false.
    SpriteComponent& shadowSprite = addComponent<SpriteComponent>(size);
    shadowSprite.setPlaceholderColor(sf::Color(0, 0, 0, 120));

    SpriteComponent& bodySprite = addComponent<SpriteComponent>(size);
    bodySprite.setPlaceholderColor(sf::Color(90, 20, 110));

    addComponent<ColliderComponent>(size, false);

    // Общий приём Enemy/Soldier/Slime (см. ActorSpawnHelpers.h) — ловит GameException на некорректные
    // maxHp/armor и откатывается на безопасные дефолты 1/0.
    HealthComponent& health = addHealthComponentWithFallback(*this, BOSS_MAX_HP, BOSS_ARMOR, "Boss");
    addComponent<DeathParticleComponent>(health);

    auto onlyPlayer = [](GameObject* target) { return target->getComponent<ChaseTargetComponent>() != nullptr; };

    // Бьёт и ближним, и дальним одновременно (как Slime3), только без "burst"-синхронизации с анимацией — у
    // заглушки нет клипа атаки, синхронизировать не с чем, оба компонента просто сами следят за своим кулдауном
    // (autoAttack/autoFire=true). omnidirectional=true — как у Slime: без директивного спрайта конус зрения по
    // facing игроку всё равно не виден, честнее бить по всем сторонам сразу.
    addComponent<AttackComponent>(
        "Boss", BOSS_MELEE_DAMAGE, BOSS_MELEE_RANGE, BOSS_MELEE_COOLDOWN, true, BOSS_MELEE_HIT_DELAY, onlyPlayer, true, true);
    addComponent<RangedAttackComponent>("Boss", BOSS_RANGED_DAMAGE, BOSS_RANGED_MIN_RANGE, BOSS_RANGED_MAX_RANGE,
        BOSS_RANGED_COOLDOWN, PROJECTILE_SPEED, PROJECTILE_HIT_RADIUS, PROJECTILE_TEXTURE, PROJECTILE_VISUAL_SIZE,
        BOSS_RANGED_SHOT_DELAY, onlyPlayer, /*autoFire=*/true, /*requireTarget=*/true, PROJECTILE_FRAME_COUNT,
        PROJECTILE_FRAME_DURATION);

    addComponent<HitFlashComponent>(bodySprite, sf::seconds(0.3f), sf::seconds(0.06f), sf::Color(255, 200, 60));
    addComponent<BossMinionSummonComponent>(SUMMON_INTERVAL, SUMMON_MAX_ALIVE, SUMMON_SPAWN_RADIUS, std::move(spawnMinion));

    LOG_INFO("Boss создан на позиции (" + std::to_string(position.x) + ", " + std::to_string(position.y)
             + "), радиус обнаружения " + std::to_string(detectionRadius));
}
