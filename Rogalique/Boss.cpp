#include "Boss.h"
#include "ActorAnimationComponent.h"
#include "ActorSpawnHelpers.h"
#include "AttackComponent.h"
#include "BossIntroComponent.h"
#include "BossMinionSummonComponent.h"
#include "BossSpinBarrageComponent.h"
#include "BossTeleportComponent.h"
#include "ChaseComponent.h"
#include "ChaseTargetComponent.h"
#include "ColliderComponent.h"
#include "DeathParticleComponent.h"
#include "EnemyBehaviorComponent.h"
#include "GameWorld.h"
#include "HealthComponent.h"
#include "HitFlashComponent.h"
#include "Log.h"
#include "MovementComponent.h"
#include "RangedAttackComponent.h"
#include "SpriteComponent.h"
#include "VisualEffect.h"

namespace
{
    // Заметно больше и живучее любого рядового врага (Enemy — 6 HP/2 урона, см. ENEMY_MAX_HP в Enemy.cpp) — босс
    // арены, последняя волна. Числа ориентировочные — баланс имеет смысл донастраивать по факту прохождения.
    constexpr int BOSS_MAX_HP = 50;
    // 0, не больше — HealthComponent::takeDamage вычитает броню из урона ФЛЭТОМ (не в процентах): у игрока и
    // копьё, и арбалет бьют ровно на 2 (см. Player.cpp), при броне 3 это давало max(0, 2-3)=0 — удары не наносили
    // вообще никакого урона (был баг: удары мечом/арбалетом не проходили). Ни у одного другого врага в игре брони нет
    // вовсе (см. Enemy/Soldier/Slime) — сложность боссу и так даёт запас HP, а не броня.
    constexpr int BOSS_ARMOR = 0;
    constexpr float BOSS_PATROL_RADIUS = 40.f;

    // Урон мечом вернули на 3 (было временно 1, пока не завели HealthComponent::setLastStandEnabled — см.
    // Player.cpp) — теперь "нельзя умереть за один неожиданный удар" даёт правило последнего шанса само по себе,
    // независимо от конкретной цифры урона, отдельно душить урон босса под это уже не нужно.
    constexpr int BOSS_MELEE_DAMAGE = 3;
    constexpr float BOSS_MELEE_RANGE = 70.f;
    // 0.6, не 0.9 (буст сложности) — здесь и ниже все кулдауны/интервалы урезаны, чтобы босс
    // ощутимо чаще пускал в ход способности.
    const sf::Time BOSS_MELEE_COOLDOWN = sf::seconds(0.6f);
    const sf::Time BOSS_MELEE_HIT_DELAY = sf::seconds(0.25f);

    // Кольцо дальней атаки начинается за пределами MELEE_RANGE — не стреляет в упор, там уже работает ближний бой.
    constexpr int BOSS_RANGED_DAMAGE = 2;
    constexpr float BOSS_RANGED_MIN_RANGE = BOSS_MELEE_RANGE;
    constexpr float BOSS_RANGED_MAX_RANGE = 280.f;
    const sf::Time BOSS_RANGED_COOLDOWN = sf::seconds(1.4f);
    const sf::Time BOSS_RANGED_SHOT_DELAY = sf::seconds(0.3f);
    constexpr float PROJECTILE_SPEED = 240.f;
    constexpr float PROJECTILE_HIT_RADIUS = 14.f;
    // Родной кадр 6x6 (см. vampire_lord_PROJECTILE.png) — заметно мельче остальных пуль в игре, поэтому визуальный
    // размер задран сильнее обычного множителя, иначе на арене был бы почти не виден.
    const sf::Vector2f PROJECTILE_VISUAL_SIZE(20.f, 20.f);
    const std::string PROJECTILE_TEXTURE
        = "Resources/Characters/The Vampire Lord & Spawns/Vampire Lord/Magical Effects/vampire_lord_PROJECTILE.png";
    constexpr int PROJECTILE_FRAME_COUNT = 4;
    const sf::Time PROJECTILE_FRAME_DURATION = sf::seconds(0.09f);

    // Ниже — четыре новые способности босса, задействующие весь набор роликов купленного пака, включая те, для
    // которых до этого не было ни одной механики (см. docs/DESIGN_DOC.md).
    // Общая папка эффектов ("Magical Effects") — снаряды/вспышки, отдельная от роликов тела ("Vampire Lord
    // Animation States") — та же структура, что уже была у PROJECTILE_TEXTURE выше.
    const std::string BODY_DIR = "Resources/Characters/The Vampire Lord & Spawns/Vampire Lord/Vampire Lord Animation States/";
    const std::string EFFECTS_DIR = "Resources/Characters/The Vampire Lord & Spawns/Vampire Lord/Magical Effects/";

    // BITE — вампирский укус: второй, более редкий канал ближнего боя (тикает независимо от обычного melee выше,
    // тем же приёмом, что у Slime3 совмещающего RangedAttackComponent+AttackComponent, см. класс-комментарий Boss).
    // Слабее по чистому урону, но лечит босса на весь реально нанесённый урон (см. AttackComponent::lifestealFraction) —
    // тематически "укус", а не просто третий взмах мечом.
    constexpr int BOSS_BITE_DAMAGE = 2;
    constexpr float BOSS_BITE_RANGE = BOSS_MELEE_RANGE;
    const sf::Time BOSS_BITE_COOLDOWN = sf::seconds(4.5f);
    // 12 кадров BITE по 0.06с — вспышка клыков ближе к концу ролика, а не в самом начале замаха.
    const sf::Time BOSS_BITE_HIT_DELAY = sf::seconds(0.5f);
    constexpr float BOSS_BITE_LIFESTEAL = 1.f;
    constexpr int BITE_FRAME_COUNT = 12;

    // AOE — размашистый разряд по площади вокруг себя: дальше и медленнее melee/bite, с заметным телеграфом
    // (AOE_CAST на теле + растущее кольцо AOE_EFFECT) перед самим уроном (AOE_DISCHARGE_EFFECT). Строго говоря,
    // движок не умеет бить сразу нескольких — findBestAttackTarget всегда берёт одного ближайшего подходящего
    // (см. TargetFinder.h), но на арене боя с боссом единственная валидная цель — игрок (onlyPlayer-фильтр, как и
    // у остальных атак ниже), так что для этой конкретной арены "большой радиус, телеграфированный заранее" и
    // "настоящий AOE" неотличимы на практике.
    constexpr int BOSS_AOE_DAMAGE = 2;
    constexpr float BOSS_AOE_RANGE = 130.f;
    const sf::Time BOSS_AOE_COOLDOWN = sf::seconds(7.f);
    // 16 кадров AOE_CAST по 0.065с ~ 1.04с — удар почти в конце ролика, чтобы игрок успел увидеть замах и уйти.
    const sf::Time BOSS_AOE_HIT_DELAY = sf::seconds(1.f);
    constexpr int AOE_CAST_FRAME_COUNT = 16;
    constexpr int AOE_RING_FRAME_COUNT = 12;      // AOE_EFFECT_BACK/FRONT, 576x48 -> 48x48
    constexpr int AOE_DISCHARGE_FRAME_COUNT = 9;  // AOE_DISCHARGE_EFFECT (BACK/FRONT), 432x48 -> 48x48
    const sf::Vector2f AOE_RING_VISUAL_SIZE(260.f, 260.f); // ~диаметр BOSS_AOE_RANGE*2
    const sf::Time AOE_RING_FRAME_DURATION = sf::seconds(BOSS_AOE_HIT_DELAY.asSeconds() / static_cast<float>(AOE_RING_FRAME_COUNT));
    const sf::Time AOE_DISCHARGE_FRAME_DURATION = sf::seconds(0.05f);

    // Charged Shot — усиленный вариант дальней атаки: та же RangedAttackComponent, что и у обычного выстрела выше,
    // просто с более редким кулдауном, бОльшим уроном/снарядом и заметно длиннее shotDelay ("время зарядки" — на
    // него и завязан визуальный эффект LARGE_PROJECTILE_CHARGE_UP, см. buildAnimationConfig ниже).
    constexpr int BOSS_CHARGED_SHOT_DAMAGE = 4;
    const sf::Time BOSS_CHARGED_SHOT_COOLDOWN = sf::seconds(9.f);
    const sf::Time BOSS_CHARGED_SHOT_CHARGE_TIME = sf::seconds(0.8f);
    constexpr float BOSS_CHARGED_SHOT_SPEED = 200.f;
    const sf::Vector2f CHARGED_PROJECTILE_VISUAL_SIZE(34.f, 34.f);
    constexpr float CHARGED_PROJECTILE_HIT_RADIUS = 20.f;
    // LARGE_PROJECTILE_CHARGE_UP — сетка 4x3 (не горизонтальная лента, проверено по факту
    // размеров файла), для зарядного свечения используем только первую строку (4 кадра 16x16) через тот же
    // приём row/rowCount, что и у многострочных роликов тела.
    constexpr int CHARGE_GLOW_FRAME_COUNT = 4;
    constexpr int CHARGE_GLOW_ROW_COUNT = 3;
    const sf::Vector2f CHARGE_GLOW_VISUAL_SIZE(32.f, 32.f);
    const sf::Time CHARGE_GLOW_FRAME_DURATION
        = sf::seconds(BOSS_CHARGED_SHOT_CHARGE_TIME.asSeconds() / static_cast<float>(CHARGE_GLOW_FRAME_COUNT));

    // Вспышка попадания снаряда — маленькая версия для обычного выстрела (тот и сам мельче), крупная для
    // Charged Shot (см. ниже, оба вешаются через RangedAttackComponent::onImpact). Barrage (см.
    // BossSpinBarrageComponent) использует IMPACT_SMALL для собственных, ещё более мелких болтов независимо.
    constexpr int PROJECTILE_IMPACT_FRAME_COUNT = 11;       // 528x48 -> 48x48
    constexpr int PROJECTILE_IMPACT_SMALL_FRAME_COUNT = 5;  // 80x16 -> 16x16
    const sf::Vector2f PROJECTILE_IMPACT_VISUAL_SIZE(50.f, 50.f);
    const sf::Vector2f PROJECTILE_IMPACT_SMALL_VISUAL_SIZE(24.f, 24.f);
    const sf::Time PROJECTILE_IMPACT_FRAME_DURATION = sf::seconds(0.04f);

    void spawnSmallImpact(sf::Vector2f position)
    {
        GameWorld::instance().spawnInRoot(std::make_unique<VisualEffect>(position,
            EFFECTS_DIR + "vampire_lord_PROJECTILE_IMPACT_SMALL.png", PROJECTILE_IMPACT_SMALL_VISUAL_SIZE,
            PROJECTILE_IMPACT_SMALL_FRAME_COUNT, PROJECTILE_IMPACT_FRAME_DURATION));
    }

    void spawnBigImpact(sf::Vector2f position)
    {
        GameWorld::instance().spawnInRoot(std::make_unique<VisualEffect>(position,
            EFFECTS_DIR + "vampire_lord_PROJECTILE_IMPACT.png", PROJECTILE_IMPACT_VISUAL_SIZE, PROJECTILE_IMPACT_FRAME_COUNT,
            PROJECTILE_IMPACT_FRAME_DURATION));
    }

    // Spin Barrage — редкая "крутящаяся" атака по всей арене сразу, см. BossSpinBarrageComponent за деталями:
    // раз в интервал два кольца снарядов (внутреннее быстрое + внешнее медленное) во все стороны сразу, не
    // прицельно. Единственный набор из четырёх файлов пака без готового боевого тела-ролика (только снаряды/
    // эффекты) — босс во время неё остаётся в текущей позе (Idle/Walk), отдельного клипа под неё в паке нет.
    const sf::Time BOSS_SPIN_BARRAGE_INTERVAL = sf::seconds(12.f);
    const sf::Time BOSS_SPIN_BARRAGE_CHANNEL = sf::seconds(0.55f);
    constexpr int BOSS_SPIN_SMALL_RING_COUNT = 8;
    constexpr int BOSS_SPIN_SMALL_RING_DAMAGE = 1;
    constexpr float BOSS_SPIN_SMALL_RING_SPEED = 190.f;
    constexpr int BOSS_SPIN_MULTI_RING_COUNT = 10;
    constexpr int BOSS_SPIN_MULTI_RING_DAMAGE = 1;
    constexpr float BOSS_SPIN_MULTI_RING_SPEED = 150.f;

    // Подкрепление: раз в SUMMON_INTERVAL, если живых миньонов меньше SUMMON_MAX_ALIVE — новый (см.
    // BossMinionSummonComponent), появляется в кольце SUMMON_SPAWN_RADIUS вокруг босса.
    const sf::Time SUMMON_INTERVAL = sf::seconds(9.f);
    constexpr int SUMMON_MAX_ALIVE = 2;
    constexpr float SUMMON_SPAWN_RADIUS = 90.f;

    // Больше коллайдера (size, приходит от вызывающего кода — SceneFacade) ради читаемости на арене, как и у
    // остальных существ (см. VISUAL_SIZE в Enemy.cpp) — коллайдер остаётся компактным для честных столкновений.
    const sf::Vector2f VISUAL_SIZE(192.f, 192.f);
    // Своей текстуры тени в купленном паке нет — плоский плейсхолдер-эллипс вместо неё, сдвинутый к ногам через
    // SpriteComponent::setPositionOffset (иначе плейсхолдер на центре GameObject читается как полоса на груди/
    // плечах, а не как тень). Смещение и размер подобраны по факту: непрозрачные пиксели тела в кадре
    // vampire_lord_IDLE.png лежат в y=[14..36] из 48 (низ ног на 12px ниже центра кадра 24), при масштабе VISUAL_SIZE/48=4
    // это 48 мировых пикселей вниз от центра GameObject.
    const sf::Vector2f SHADOW_SIZE(56.f, 18.f);
    constexpr float SHADOW_OFFSET_Y = 46.f;

    // Лист 4-строчный (48x48 на кадр, 4 строки — см. проверку размеров файлов), но, как и у Slime, используем только
    // строку 0: ActorAnimationComponent сам зеркалит спрайт по X через getFacing() (см. класс-комментарий Boss.h),
    // отдельного "смотрит вверх/вниз" у ботов в игре нет вовсе (у Orc/Soldier тоже только один ракурс).
    constexpr int BODY_ROW_COUNT = 4;
    constexpr int IDLE_FRAME_COUNT = 4;
    constexpr int IDLE_READY_FRAME_COUNT = 4;
    constexpr int WALK_FRAME_COUNT = 6;
    constexpr int MELEE_FRAME_COUNT = 8;
    constexpr int CAST_FRAME_COUNT = 8;
    constexpr int GET_READY_FRAME_COUNT = 6;
    // BAT_TELEPORT_EFFECT — единственный клип тела, снятый в 1 ряд (768x48), не 4 — те же кадры на все стороны.
    constexpr int BAT_TELEPORT_FRAME_COUNT = 16;
    // DEATH — тоже без 4 строк (лист 2736x48, один ряд) — боссу для драматичной смерти хватает одного общего
    // ракурса; 57 кадров считаны из реальных размеров файла (2736 / 48).
    constexpr int DEATH_FRAME_COUNT = 57;

    // Оба атакующих ролика (ближний+дальний) тикают независимо (см. ActorAnimationConfig::attacks) — приоритет
    // выше Walk/Idle, побеждает тот, чей visualDuration ещё не истёк.
    ActorAnimationConfig buildAnimationConfig(std::function<bool()> consumeIntroJustTriggered,
        std::function<bool()> consumeTeleportJustTriggered, std::function<bool()> consumeMeleeJustTriggered,
        std::function<bool()> consumeRangedJustTriggered, std::function<bool()> consumeBiteJustTriggered,
        std::function<bool()> consumeAoeJustTriggered, std::function<bool()> isAlert)
    {
        ActorAnimationConfig config;
        config.idle = {BODY_DIR + "vampire_lord_IDLE.png", IDLE_FRAME_COUNT, sf::seconds(0.15f), true, 0, BODY_ROW_COUNT};
        // IDLE_READY — "боевая" стойка вместо обычного покоя, как только босс хоть раз получил урон в этом бою (см.
        // isAlert ниже, Boss::Boss) — раньше не показывалась вовсе, ролик в паке был, а условия для него не было.
        config.alertIdle
            = {BODY_DIR + "vampire_lord_IDLE_READY.png", IDLE_READY_FRAME_COUNT, sf::seconds(0.15f), true, 0, BODY_ROW_COUNT};
        config.isAlert = std::move(isAlert);
        config.walk = {BODY_DIR + "vampire_lord_WALK.png", WALK_FRAME_COUNT, sf::seconds(0.1f), true, 0, BODY_ROW_COUNT};
        // Hurt (стан) у ботов без постхитовой неуязвимости никогда не срабатывает (см. HealthComponent::isStunned,
        // addHealthComponentWithFallback не задаёт postHitInvulnerability) — как и у Enemy/Soldier/Slime, клип
        // реально не показывается, но поле обязано быть валидным (ActorAnimationComponent грузит его безусловно).
        config.hurt = config.idle;
        config.hurtVisualDuration = sf::Time::Zero;
        config.death = {BODY_DIR + "vampire_lord_DEATH.png", DEATH_FRAME_COUNT, sf::seconds(0.04f), false, 0, 1};
        // Своей тени в купленном паке нет (только тело) — оставляем SpriteComponent на плейсхолдере (см. конструктор
        // ниже): пустой путь гарантированно не грузится, SpriteComponent сам откатывается на цветной плейсхолдер.
        config.normalShadow = {"", 1, sf::Time::Zero, true};
        config.deathShadow = {"", 1, sf::Time::Zero, false};

        // Приоритет показа тела (см. класс-комментарий ActorAnimationConfig::attacks выше) — первый ещё активный
        // побеждает. GET_READY/телепорт — редкие и короткие, логично выше по приоритету, чем обычные удары
        // (иначе долгий AOE_CAST мог бы перекрыть только что начавшийся телепорт на один-два кадра).
        ActorAttackAnim introAnim;
        introAnim.consumeJustTriggered = std::move(consumeIntroJustTriggered);
        introAnim.clips = {{BODY_DIR + "vampire_lord_GET_READY.png", GET_READY_FRAME_COUNT, sf::seconds(0.12f), false, 0, BODY_ROW_COUNT}};
        introAnim.visualDuration = sf::seconds(0.12f * GET_READY_FRAME_COUNT);
        config.attacks.push_back(std::move(introAnim));

        ActorAttackAnim teleportAnim;
        teleportAnim.consumeJustTriggered = std::move(consumeTeleportJustTriggered);
        teleportAnim.clips
            = {{BODY_DIR + "vampire_lord_BAT_TELEPORT_EFFECT.png", BAT_TELEPORT_FRAME_COUNT, sf::seconds(0.05f), false, 0, 1}};
        teleportAnim.visualDuration = sf::seconds(0.05f * BAT_TELEPORT_FRAME_COUNT);
        config.attacks.push_back(std::move(teleportAnim));

        ActorAttackAnim meleeAnim;
        meleeAnim.consumeJustTriggered = std::move(consumeMeleeJustTriggered);
        meleeAnim.clips
            = {{BODY_DIR + "vampire_lord_MELEE_ATTACK.png", MELEE_FRAME_COUNT, sf::seconds(0.06f), false, 0, BODY_ROW_COUNT}};
        meleeAnim.visualDuration = sf::seconds(0.06f * MELEE_FRAME_COUNT);
        config.attacks.push_back(std::move(meleeAnim));

        ActorAttackAnim rangedAnim;
        rangedAnim.consumeJustTriggered = std::move(consumeRangedJustTriggered);
        rangedAnim.clips = {{BODY_DIR + "vampire_lord_CAST.png", CAST_FRAME_COUNT, sf::seconds(0.06f), false, 0, BODY_ROW_COUNT}};
        rangedAnim.visualDuration = sf::seconds(0.06f * CAST_FRAME_COUNT);
        config.attacks.push_back(std::move(rangedAnim));

        ActorAttackAnim biteAnim;
        biteAnim.consumeJustTriggered = std::move(consumeBiteJustTriggered);
        biteAnim.clips = {{BODY_DIR + "vampire_lord_BITE.png", BITE_FRAME_COUNT, sf::seconds(0.06f), false, 0, BODY_ROW_COUNT}};
        biteAnim.visualDuration = sf::seconds(0.06f * BITE_FRAME_COUNT);
        config.attacks.push_back(std::move(biteAnim));

        ActorAttackAnim aoeAnim;
        aoeAnim.consumeJustTriggered = std::move(consumeAoeJustTriggered);
        aoeAnim.clips
            = {{BODY_DIR + "vampire_lord_AOE_CAST.png", AOE_CAST_FRAME_COUNT, sf::seconds(0.065f), false, 0, 1}};
        aoeAnim.visualDuration = sf::seconds(0.065f * AOE_CAST_FRAME_COUNT);
        config.attacks.push_back(std::move(aoeAnim));

        return config;
    }
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

    // Тень рисуется первой (значит под телом), тело — вторым поверх неё — та же схема, что у Enemy/Slime.
    m_shadowSprite = &addComponent<SpriteComponent>(SHADOW_SIZE);
    m_shadowSprite->setPlaceholderColor(sf::Color(0, 0, 0, 110));
    m_shadowSprite->setPositionOffset(sf::Vector2f(0.f, SHADOW_OFFSET_Y));

    m_bodySprite = &addComponent<SpriteComponent>(VISUAL_SIZE);
    m_bodySprite->setPlaceholderColor(sf::Color(90, 20, 110));

    addComponent<ColliderComponent>(size, false);

    // Общий приём Enemy/Soldier/Slime (см. ActorSpawnHelpers.h) — ловит GameException на некорректные
    // maxHp/armor и откатывается на безопасные дефолты 1/0.
    HealthComponent& health = addHealthComponentWithFallback(*this, BOSS_MAX_HP, BOSS_ARMOR, "Boss");
    addComponent<DeathParticleComponent>(health);

    // Заведён здесь (а не рядом с ActorAnimationComponent ниже) — нужна ссылка на него ДО того, как строится
    // конфигурация анимации (buildAnimationConfig принимает готовую лямбду-триггер параметром, как и у melee/
    // ranged/bite/aoe, единым приёмом, а не постфактум-правкой уже собранного конфига).
    BossIntroComponent& intro = addComponent<BossIntroComponent>();

    auto onlyPlayer = [](GameObject* target) { return target->getComponent<ChaseTargetComponent>() != nullptr; };

    // Бьёт и ближним, и дальним одновременно (как Slime3), оба независимо следят за своим кулдауном (autoAttack/
    // autoFire=true). omnidirectional=true — как у Slime: без директивного спрайта конус зрения по facing игроку
    // всё равно не виден, честнее бить по всем сторонам сразу.
    AttackComponent& melee = addComponent<AttackComponent>(
        "Boss", BOSS_MELEE_DAMAGE, BOSS_MELEE_RANGE, BOSS_MELEE_COOLDOWN, true, BOSS_MELEE_HIT_DELAY, onlyPlayer, true, true);
    RangedAttackComponent& ranged = addComponent<RangedAttackComponent>("Boss", BOSS_RANGED_DAMAGE, BOSS_RANGED_MIN_RANGE,
        BOSS_RANGED_MAX_RANGE, BOSS_RANGED_COOLDOWN, PROJECTILE_SPEED, PROJECTILE_HIT_RADIUS, PROJECTILE_TEXTURE,
        PROJECTILE_VISUAL_SIZE, BOSS_RANGED_SHOT_DELAY, onlyPlayer, /*autoFire=*/true, /*requireTarget=*/true,
        PROJECTILE_FRAME_COUNT, PROJECTILE_FRAME_DURATION, /*onShotStarted=*/nullptr, /*onImpact=*/&spawnSmallImpact);

    // BITE — вампирский укус (см. константы выше) — лечит босса на весь нанесённый урон.
    AttackComponent& bite = addComponent<AttackComponent>("Boss-Bite", BOSS_BITE_DAMAGE, BOSS_BITE_RANGE, BOSS_BITE_COOLDOWN,
        /*autoAttack=*/true, BOSS_BITE_HIT_DELAY, onlyPlayer, /*requireTarget=*/true, /*omnidirectional=*/true,
        BOSS_BITE_LIFESTEAL);

    // AOE — телеграф (AOE_EFFECT BACK+FRONT, растущее кольцо на весь BOSS_AOE_HIT_DELAY) в момент старта удара,
    // разряд (AOE_DISCHARGE_EFFECT BACK+FRONT) в момент реального попадания — BACK спавнится первым (значит
    // рисуется под FRONT), тот же приём слоёв, что подразумевает сам пак двумя раздельными файлами на слой.
    auto spawnAoeTelegraph = [](sf::Vector2f position) {
        GameWorld::instance().spawnInRoot(std::make_unique<VisualEffect>(
            position, EFFECTS_DIR + "vampire_lord_AOE_EFFECT_BACK.png", AOE_RING_VISUAL_SIZE, AOE_RING_FRAME_COUNT,
            AOE_RING_FRAME_DURATION));
        GameWorld::instance().spawnInRoot(std::make_unique<VisualEffect>(
            position, EFFECTS_DIR + "vampire_lord_AOE_EFFECT_FRONT.png", AOE_RING_VISUAL_SIZE, AOE_RING_FRAME_COUNT,
            AOE_RING_FRAME_DURATION));
    };
    auto spawnAoeDischarge = [](sf::Vector2f position) {
        GameWorld::instance().spawnInRoot(std::make_unique<VisualEffect>(position,
            EFFECTS_DIR + "vampire_lord_AOE_DISCHARGE_EFFECT (BACK).png", AOE_RING_VISUAL_SIZE, AOE_DISCHARGE_FRAME_COUNT,
            AOE_DISCHARGE_FRAME_DURATION));
        GameWorld::instance().spawnInRoot(std::make_unique<VisualEffect>(position,
            EFFECTS_DIR + "vampire_lord_AOE_DISCHARGE_EFFECT (FRONT).png", AOE_RING_VISUAL_SIZE, AOE_DISCHARGE_FRAME_COUNT,
            AOE_DISCHARGE_FRAME_DURATION));
    };
    AttackComponent& aoe = addComponent<AttackComponent>("Boss-AOE", BOSS_AOE_DAMAGE, BOSS_AOE_RANGE, BOSS_AOE_COOLDOWN,
        /*autoAttack=*/true, BOSS_AOE_HIT_DELAY, onlyPlayer, /*requireTarget=*/true, /*omnidirectional=*/true,
        /*lifestealFraction=*/0.f, spawnAoeTelegraph, spawnAoeDischarge);

    // Charged Shot — усиленный дальний выстрел (см. константы выше); charge-glow спавнится в момент старта (та же
    // shotDelay-задержка, что и у самого выстрела — свечение гаснет ровно к появлению снаряда), impact — крупная
    // вспышка (снаряд заметно больше обычного).
    auto spawnChargeGlow = [](sf::Vector2f position) {
        GameWorld::instance().spawnInRoot(std::make_unique<VisualEffect>(position,
            EFFECTS_DIR + "vampire_lord_LARGE_PROJECTILE_CHARGE_UP.png", CHARGE_GLOW_VISUAL_SIZE, CHARGE_GLOW_FRAME_COUNT,
            CHARGE_GLOW_FRAME_DURATION, /*row=*/0, CHARGE_GLOW_ROW_COUNT));
    };
    addComponent<RangedAttackComponent>("Boss-ChargedShot", BOSS_CHARGED_SHOT_DAMAGE, BOSS_RANGED_MIN_RANGE,
        BOSS_RANGED_MAX_RANGE, BOSS_CHARGED_SHOT_COOLDOWN, BOSS_CHARGED_SHOT_SPEED, CHARGED_PROJECTILE_HIT_RADIUS,
        PROJECTILE_TEXTURE, CHARGED_PROJECTILE_VISUAL_SIZE, BOSS_CHARGED_SHOT_CHARGE_TIME, onlyPlayer, /*autoFire=*/true,
        /*requireTarget=*/true, PROJECTILE_FRAME_COUNT, PROJECTILE_FRAME_DURATION, spawnChargeGlow, &spawnBigImpact);

    // Spin Barrage — см. класс-комментарий BossSpinBarrageComponent.h и константы выше.
    addComponent<BossSpinBarrageComponent>(health, BOSS_SPIN_BARRAGE_INTERVAL, BOSS_SPIN_BARRAGE_CHANNEL,
        BOSS_SPIN_SMALL_RING_COUNT, BOSS_SPIN_SMALL_RING_DAMAGE, BOSS_SPIN_SMALL_RING_SPEED, BOSS_SPIN_MULTI_RING_COUNT,
        BOSS_SPIN_MULTI_RING_DAMAGE, BOSS_SPIN_MULTI_RING_SPEED);

    // IDLE_READY вместо обычного IDLE, как только босс хоть раз получил урон в этом бою (см. buildAnimationConfig
    // выше) — "проснулся", по-настоящему готов к бою, а не просто стоит в нейтральной позе. &health безопасна:
    // тот же приём, что уже используют lifestealFraction/onlyPlayer выше — компонент живёт на этом же GameObject
    // ровно столько же, сколько сам Boss.
    auto isAlert = [&health] { return health.getHp() < health.getMaxHp(); };

    addComponent<HitFlashComponent>(*m_bodySprite, sf::seconds(0.3f), sf::seconds(0.06f), sf::Color(255, 60, 60));
    // Телепорт — BossTeleportComponent появляется позже (см. SceneFacade.cpp, добавляется уже после того, как этот
    // конструктор отработает целиком), прямую ссылку сюда захватить не на что — лямбда вместо этого ищет его через
    // getOwner()->getComponent() лениво, при каждом реальном вызове (то есть уже во время апдейтов, когда
    // SceneFacade успеет его добавить), тем же приёмом, каким Trap/Chest/Door ищут игрока через findChaseTarget().
    addComponent<ActorAnimationComponent>(buildAnimationConfig(
        [&intro] { return intro.consumeJustTriggered(); },
        [this] {
            auto* teleport = getComponent<BossTeleportComponent>();
            return teleport && teleport->consumeJustTeleported();
        },
        [&melee] { return melee.consumeJustStarted(); }, [&ranged] { return ranged.consumeJustFired(); },
        [&bite] { return bite.consumeJustStarted(); }, [&aoe] { return aoe.consumeJustStarted(); }, std::move(isAlert)));

    addComponent<BossMinionSummonComponent>(SUMMON_INTERVAL, SUMMON_MAX_ALIVE, SUMMON_SPAWN_RADIUS, std::move(spawnMinion));

    LOG_INFO("Boss создан на позиции (" + std::to_string(position.x) + ", " + std::to_string(position.y)
             + "), радиус обнаружения " + std::to_string(detectionRadius));
}
