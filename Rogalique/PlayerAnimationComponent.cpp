#include "PlayerAnimationComponent.h"
#include "AttackComponent.h"
#include "HealthComponent.h"
#include "InputComponent.h"
#include "Log.h"
#include "Player.h"
#include "RangedAttackComponent.h"
#include "SpriteComponent.h"
#include "WeaponComponent.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float EPS = 0.0001f;
    // Порог между обычной ходьбой (длина вектора направления == 1) и спринтом (InputComponent умножает на 1.7 при зажатом Shift).
    constexpr float SPRINT_THRESHOLD = 1.3f;
    // Каждая полоса тела/тени/пыли — 384px на 8 кадров по 48px, у всего пака протагониста одинаково.
    constexpr int FRAME_COUNT = 8;

    // Скорость проигрывания роликов Dash/Jump — намеренно не равна физической длительности самих манёвров в InputComponent (0.15-0.3с).
    constexpr float DASH_FRAME_DURATION = 0.06f;
    const sf::Time DASH_VISUAL_DURATION = sf::seconds(DASH_FRAME_DURATION * FRAME_COUNT);
    // И-фреймы рывка — только первая половина проигрывания ролика, а не весь Dash и не Jump.
    const sf::Time DASH_INVULNERABLE_DURATION = DASH_VISUAL_DURATION / 2.f;

    constexpr float JUMP_FRAME_DURATION = 0.06f;
    const sf::Time JUMP_VISUAL_DURATION = sf::seconds(JUMP_FRAME_DURATION * FRAME_COUNT);

    // Совпадает с темпом удара орка/лучника (0.07с/кадр) — было 0.05, ощущалось слишком быстро/дёргано.
    constexpr float MELEE_FRAME_DURATION = 0.07f;
    const sf::Time MELEE_VISUAL_DURATION = sf::seconds(MELEE_FRAME_DURATION * FRAME_COUNT);

    constexpr float SHOOT_FRAME_DURATION = 0.04f;
    const sf::Time SHOOT_VISUAL_DURATION = sf::seconds(SHOOT_FRAME_DURATION * FRAME_COUNT);

    // 8 кадров ровно укладываются в GUN_RELOAD_DURATION (1с, см. Player.cpp) — ролик доигрывает точно к концу перезарядки.
    constexpr float RELOAD_FRAME_DURATION = 0.125f;

    std::string resolveFacing(sf::Vector2f direction, const std::string& previousFacing)
    {
        if (std::abs(direction.x) < EPS && std::abs(direction.y) < EPS) {
            return previousFacing;
        }

        std::string vertical = (direction.y < 0.f) ? "Up" : "Down";
        if (std::abs(direction.x) < EPS) {
            return vertical;
        }
        return (direction.x < 0.f ? "Left_" : "Right_") + vertical;
    }

    std::string toLowerAscii(std::string value)
    {
        std::transform(
            value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    // "" для Unarmed (у Normal нет инфикса в имени файла — просто Idle_Down.png), иначе "Gun"/"Spear".
    std::string weaponInfix(Weapon weapon)
    {
        switch (weapon) {
        case Weapon::Gun:
            return "Gun";
        case Weapon::Spear:
            return "Spear";
        default:
            return "";
        }
    }

    // Подпапка Idle/Walk/Run/Dash/Jump/Death/Death_Shadow под конкретное оружие.
    std::string weaponFolder(Weapon weapon)
    {
        switch (weapon) {
        case Weapon::Gun:
            return "Gun";
        case Weapon::Spear:
            return "Spear";
        default:
            return "Normal";
        }
    }

    // filePrefix + (инфикс + "_", если есть) + facing + ".png" — общий шаблон имени тела для всех локомоционных
    // роликов (Idle/Walk/Run/Dash/Jump/Death), отличаются только baseDir/filePrefix.
    std::string buildBodyPath(
        const std::string& baseDir, const std::string& filePrefix, const std::string& infix, const std::string& facing)
    {
        std::string suffix = infix.empty() ? facing : (infix + "_" + facing);
        return baseDir + filePrefix + suffix + ".png";
    }

    // У неподвижного ролика выстрела (Attack/Gun/Shooting_*) порядок суффиксов не совпадает с остальным паком
    // (Up_Left вместо Left_Up), а файла Shooting_Right_Down.png в паке нет вовсе — раньше это падало на "Down"
    // (анфас-кадр, стрельба выглядела как "вниз" вместо "вправо"). resolveFacing() коллапсирует чистое
    // горизонтальное прицеливание (без W/S) в "Left_Down"/"Right_Down" ("_Down" тут значит "не вверх", а не
    // диагональ-именно-вниз) — берём для обоих чисто горизонтальный Shooting_Left.png/Shooting_Right.png, он
    // в паке есть специально для этого случая.
    std::string resolveStandingShootSuffix(const std::string& facing)
    {
        if (facing == "Left_Up") {
            return "Up_Left";
        }
        if (facing == "Right_Up") {
            return "Up_Right";
        }
        if (facing == "Left_Down") {
            return "Left";
        }
        if (facing == "Right_Down") {
            return "Right";
        }
        return facing; // Up/Down совпадают как есть
    }
} // namespace

void PlayerAnimationComponent::update(sf::Time dt)
{
    auto* player = dynamic_cast<Player*>(getOwner());
    if (!player) {
        return;
    }

    auto* input = player->getComponent<InputComponent>();
    if (!input) {
        return;
    }
    SpriteComponent& body = player->getBodySprite();
    SpriteComponent& shadow = player->getShadowSprite();
    SpriteComponent& dust = player->getDustSprite();

    sf::Vector2f direction = input->getMoveDirection();
    m_facing = resolveFacing(direction, m_facing);

    auto* health = player->getComponent<HealthComponent>();
    auto* weaponComponent = player->getComponent<WeaponComponent>();
    Weapon weapon = weaponComponent ? weaponComponent->getCurrent() : Weapon::Unarmed;
    std::string weapFolder = weaponFolder(weapon);
    std::string infix = weaponInfix(weapon);
    const std::string baseDir = "Resources/Characters/Protogonist/";

    // Смерть необратима и перекрывает всё остальное.
    if (health && health->isDead()) {
        std::string deathClip = "Death_" + weapFolder;
        if (m_currentClip != deathClip) {
            m_currentClip = deathClip;
            LOG_INFO("Player died facing " + m_facing);
            body.loadAnimation(buildBodyPath(baseDir + "Death/" + weapFolder + "/", "death_", infix, m_facing), FRAME_COUNT,
                sf::seconds(0.09f), false);

            std::string shadowInfix = (weapon == Weapon::Unarmed) ? "normal" : infix;
            shadow.loadAnimation(
                baseDir + "Death_Shadow/" + weapFolder + "/death_" + shadowInfix + "_" + toLowerAscii(m_facing) + ".png",
                FRAME_COUNT, sf::seconds(0.09f), false);
        }
        return;
    }

    // Рывок/прыжок — как раньше, не зависят от оружия. Механика в InputComponent короче их роликов — на подъёме
    // флага заводим свой визуальный таймер на полную длину анимации и дальше держим состояние по нему.
    bool dashingNow = input->isDashing();
    bool dashJustStarted = dashingNow && !m_wasDashing;
    if (dashJustStarted) {
        m_dashVisualTimeRemaining = DASH_VISUAL_DURATION;
    }
    m_wasDashing = dashingNow;
    if (m_dashVisualTimeRemaining > sf::Time::Zero) {
        m_dashVisualTimeRemaining -= dt;
    }

    bool jumpingNow = input->isJumping();
    bool jumpJustStarted = jumpingNow && !m_wasJumping;
    if (jumpJustStarted) {
        m_jumpVisualTimeRemaining = JUMP_VISUAL_DURATION;
    }
    m_wasJumping = jumpingNow;
    if (m_jumpVisualTimeRemaining > sf::Time::Zero) {
        m_jumpVisualTimeRemaining -= dt;
    }

    bool isDashingVisual = m_dashVisualTimeRemaining > sf::Time::Zero;
    bool isJumpingVisual = m_jumpVisualTimeRemaining > sf::Time::Zero;

    // Копьё — визуальный импульс на удар (тот же приём, что у Enemy/SoldierAnimationComponent).
    auto* spearAttack = player->getComponent<AttackComponent>();
    bool meleeJustStarted = (weapon == Weapon::Spear) && spearAttack && spearAttack->consumeJustStarted();
    if (meleeJustStarted) {
        m_meleeVisualTimeRemaining = MELEE_VISUAL_DURATION;
    }
    if (m_meleeVisualTimeRemaining > sf::Time::Zero) {
        m_meleeVisualTimeRemaining -= dt;
    }
    bool isMeleeAttacking = m_meleeVisualTimeRemaining > sf::Time::Zero;

    // Пистолет — импульс на выстрел (recoil-поза) отдельно от факта перезарядки.
    auto* gunAttack = player->getComponent<RangedAttackComponent>();
    bool shotJustFired = (weapon == Weapon::Gun) && gunAttack && gunAttack->consumeJustFired();
    if (shotJustFired) {
        m_shootVisualTimeRemaining = SHOOT_VISUAL_DURATION;
    }
    if (m_shootVisualTimeRemaining > sf::Time::Zero) {
        m_shootVisualTimeRemaining -= dt;
    }
    bool isShooting = m_shootVisualTimeRemaining > sf::Time::Zero;
    bool isReloading = weaponComponent && weaponComponent->isReloading();

    float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    bool isMoving = length > EPS;
    bool isSprinting = length > SPRINT_THRESHOLD;

    // Приоритет: Death (выше) > Dash/Jump > копьё > перезарядка > выстрел > бег/ходьба/покой — у пистолета обычная
    // ходьба (не спринт) заменяется на "прицельную" (Walk_while_Aiming), см. комментарий у WalkAim ниже.
    std::string state;
    if (isDashingVisual) {
        state = "Dash";
    } else if (isJumpingVisual) {
        state = "Jump";
    } else if (isMeleeAttacking) {
        state = "MeleeAttack";
    } else if (isReloading) {
        state = isMoving ? "WalkReload" : "Reload";
    } else if (isShooting) {
        state = isSprinting ? "RunShoot" : (isMoving ? "WalkShoot" : "Shoot");
    } else if (isSprinting) {
        state = "Run";
    } else if (isMoving) {
        state = (weapon == Weapon::Gun) ? "WalkAim" : "Walk";
    } else {
        state = "Idle";
    }

    std::string clip = state + "_" + weapFolder + "_" + m_facing;
    // Форсируем перезагрузку на границах событий, даже если строка клипа не поменялась (второй удар/выстрел подряд).
    bool forceReload = dashJustStarted || jumpJustStarted || meleeJustStarted || shotJustFired;
    if (clip == m_currentClip && !forceReload) {
        return;
    }
    m_currentClip = clip;

    std::string bodyPath;
    std::string shadowPath;
    sf::Time frameDuration;
    bool loop = true;
    bool reloadShadow = false;

    if (state == "MeleeAttack") {
        bodyPath = baseDir + "Attack/Spear/Attack_Spear_" + m_facing + ".png";
        frameDuration = sf::seconds(MELEE_FRAME_DURATION);
        loop = false;
    } else if (state == "Shoot") {
        bodyPath = baseDir + "Attack/Gun/Shooting_" + resolveStandingShootSuffix(m_facing) + ".png";
        frameDuration = sf::seconds(SHOOT_FRAME_DURATION);
        loop = false;
    } else if (state == "WalkShoot") {
        bodyPath = baseDir + "Walk_while_Shooting/walk_Shooting_" + m_facing + ".png";
        frameDuration = sf::seconds(SHOOT_FRAME_DURATION);
        loop = false;
    } else if (state == "RunShoot") {
        bodyPath = baseDir + "Run_while_shooting/Run_while_shooting_" + m_facing + ".png";
        frameDuration = sf::seconds(SHOOT_FRAME_DURATION);
        loop = false;
    } else if (state == "Reload") {
        bodyPath = baseDir + "Reloading/Reloading_" + m_facing + ".png";
        frameDuration = sf::seconds(RELOAD_FRAME_DURATION);
        loop = false;
    } else if (state == "WalkReload") {
        bodyPath = baseDir + "Walk_while_Reloading/walk_reloading_" + m_facing + ".png";
        frameDuration = sf::seconds(RELOAD_FRAME_DURATION);
        loop = false;
    } else if (state == "WalkAim") {
        // Обычная ходьба с пистолетом наготове — прицельная стойка. Спринт (state=="Run") прицелиться не даёт,
        // тогда используется обычный Run/Gun ниже.
        bodyPath = baseDir + "Walk_while_Aiming/walk_aiming_" + m_facing + ".png";
        frameDuration = sf::seconds(0.09f);
    } else if (state == "Dash") {
        bodyPath = buildBodyPath(baseDir + "Dash/" + weapFolder + "/", "Dash_", infix, m_facing);
        shadowPath = baseDir + "Dash/Dash_Shadow.png";
        frameDuration = sf::seconds(DASH_FRAME_DURATION);
        loop = false;
        reloadShadow = true;
        // И-фреймы на первую половину проигрывания ролика Dash — единственный источник неуязвимости в игре, Jump её не даёт.
        if (health) {
            health->setInvulnerable(DASH_INVULNERABLE_DURATION);
        }
    } else if (state == "Jump") {
        bodyPath = buildBodyPath(baseDir + "Jump/" + weapFolder + "/", "Jump_", infix, m_facing);
        shadowPath = baseDir + "Jump/Jump_Shadow.png";
        frameDuration = sf::seconds(JUMP_FRAME_DURATION);
        loop = false;
        reloadShadow = true;
    } else if (state == "Run") {
        bodyPath = buildBodyPath(baseDir + "Run/" + weapFolder + "/", "Run_", infix, m_facing);
        shadowPath = baseDir + "Shadow.png";
        frameDuration = sf::seconds(0.07f);
        reloadShadow = true;
    } else if (state == "Walk") {
        bodyPath = buildBodyPath(baseDir + "Walk/" + weapFolder + "/", "walk_", infix, m_facing);
        shadowPath = baseDir + "Shadow.png";
        frameDuration = sf::seconds(0.09f);
        reloadShadow = true;
    } else { // Idle
        bodyPath = buildBodyPath(baseDir + "Idle/" + weapFolder + "/", "Idle_", infix, m_facing);
        shadowPath = baseDir + "Shadow.png";
        frameDuration = sf::seconds(0.18f);
        reloadShadow = true;
    }

    body.loadAnimation(bodyPath, FRAME_COUNT, frameDuration, loop);

    // Тень трогаем только у локомоционных роликов — у Attack/Shoot/Reload/WalkAim нет своей, остаётся та, что
    // уже загружена статичным Shadow.png (или пропущена, всё равно перекрывается следующим Idle/Walk/Run/Dash/Jump).
    if (reloadShadow) {
        bool shadowIsStrip = (state == "Dash" || state == "Jump");
        shadow.loadAnimation(shadowPath, shadowIsStrip ? FRAME_COUNT : 1, shadowIsStrip ? frameDuration : sf::Time::Zero, loop);
    }

    // Пыль — отдельный слой, включаем ровно на время рывка/прыжка (см. Player.cpp — SpriteComponent всегда что-то
    // рисует, прячем через альфу 0, а не отдельным флагом видимости).
    if (state == "Dash" || state == "Jump") {
        if (dashJustStarted || jumpJustStarted) {
            std::string dustPath = (state == "Dash") ? baseDir + "Dash/Dust/Dash_Dust_" + m_facing + ".png"
                                                     : baseDir + "Jump/Dust/Jump_Dust_" + m_facing + ".png";
            dust.loadAnimation(dustPath, FRAME_COUNT, frameDuration, false);
            dust.clearColor();
            m_dustVisible = true;
        }
    } else if (m_dustVisible) {
        dust.setColor(sf::Color(255, 255, 255, 0));
        m_dustVisible = false;
    }
}
