#include "WeaponComponent.h"
#include "ArrowCrateComponent.h"
#include "AttackComponent.h"
#include "FocusedInput.h"
#include "GameObject.h"
#include "GameWorld.h"
#include "InputComponent.h"
#include "Log.h"
#include "MouseWheelBuffer.h"
#include "RangedAttackComponent.h"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    const char* weaponName(Weapon weapon)
    {
        switch (weapon) {
        case Weapon::Spear:
            return "копьё";
        case Weapon::Gun:
            return "пистолет";
        default:
            return "ничего (безоружен)";
        }
    }

    // Тот же радиус, на котором Soldier подбирает у ArrowCrate (см. ARROW_PICKUP_DISTANCE в SoldierAmmoComponent.cpp,
    // включая почему именно столько — у ящика коллизия, вплотную ближе физически не подойти).
    constexpr float CRATE_PICKUP_DISTANCE = 42.f;
} // namespace

WeaponComponent::WeaponComponent(
    AttackComponent* spearAttack, RangedAttackComponent* gunAttack, int magazineSize, int reserveAmmo, sf::Time reloadDuration)
    : m_spearAttack(spearAttack),
      m_gunAttack(gunAttack),
      m_magazineSize(magazineSize),
      m_ammo(magazineSize),
      m_reserveAmmoMax(reserveAmmo),
      m_reserveAmmo(reserveAmmo),
      m_reloadDuration(reloadDuration)
{
}

void WeaponComponent::update(sf::Time dt)
{
    // Мёртвому оружие менять незачем — экран поражения уже на экране (GameWorld::isGameOver()), а world при этом
    // не на паузе (см. PlayerDeathComponent), так что без этой проверки Q продолжал бы работать и после смерти.
    if (GameWorld::instance().isGameOver()) {
        m_switchEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Q));
        m_reloadEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::R));
    } else {
        // Колесо мыши — та же смена оружия, что и Q (см. MouseWheelBuffer.h): оружий всего два, направление
        // прокрутки не важно, любое ненулевое значение за кадр считается переключением.
        if (m_switchEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Q)) || MouseWheelBuffer::deltaThisFrame() != 0.f) {
            switchWeapon();
        }
        if (m_reloadEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::R))) {
            tryReload();
        }
    }

    if (m_reloadRemaining > sf::Time::Zero) {
        m_reloadRemaining -= dt;
        if (m_reloadRemaining <= sf::Time::Zero) {
            // Дозаряжаем из резерва, не сразу до полного магазина — если патронов в запасе меньше, чем не хватает
            // в стволе, забираем сколько есть (magazineSize=1 сейчас, так что на практике всегда 0 или 1).
            int refill = std::min(m_magazineSize - m_ammo, m_reserveAmmo);
            m_ammo += refill;
            m_reserveAmmo -= refill;
        }
    }

    // Копьё "коммитит" — на весь свой кулдаун персонаж стоит и бьёт, а не скользит по экрану (у Attack/Spear нет
    // отдельного ролика "бью на ходу", см. InputComponent::setMovementLocked). Пистолет так не запирает — у него
    // как раз есть Walk_while_Shooting/Run_while_shooting, стрелять на бегу можно. А вот бежать (не просто идти)
    // на перезарядке нельзя — ролика Run_while_Reloading в паке нет, есть только Walk_while_Reloading, поэтому на
    // это время глушим сам множитель спринта (см. InputComponent::setSprintSuppressed), а не запрещаем движение.
    GameObject* owner = getOwner();
    auto* input = owner ? owner->getComponent<InputComponent>() : nullptr;
    if (input) {
        bool lockMovement = (m_current == Weapon::Spear) && m_spearAttack && m_spearAttack->isOnCooldown();
        input->setMovementLocked(lockMovement);
        input->setSprintSuppressed(m_current == Weapon::Gun && isReloading());
    }

    refillFromNearbyCrate();
}

void WeaponComponent::refillFromNearbyCrate()
{
    if (m_reserveAmmo >= m_reserveAmmoMax) {
        return;
    }
    GameObject* owner = getOwner();
    if (!owner) {
        return;
    }
    for (ArrowCrateComponent* crate : ArrowCrateComponent::getAll()) {
        GameObject* crateOwner = crate->getOwner();
        if (!crateOwner) {
            continue;
        }
        sf::Vector2f delta = crateOwner->getPosition() - owner->getPosition();
        float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        if (distance <= CRATE_PICKUP_DISTANCE) {
            m_reserveAmmo = m_reserveAmmoMax;
            LOG_INFO("Player: пополнил запас болтов у ящика, снова " + std::to_string(m_reserveAmmo));
            return;
        }
    }
}

void WeaponComponent::switchWeapon()
{
    switch (m_current) {
    case Weapon::Unarmed:
        m_current = Weapon::Spear;
        break;
    case Weapon::Spear:
        m_current = m_gunAvailable ? Weapon::Gun : Weapon::Unarmed;
        break;
    case Weapon::Gun:
        m_current = Weapon::Unarmed;
        break;
    }
    LOG_INFO(std::string("Player: сменил оружие на ") + weaponName(m_current));
}

void WeaponComponent::setGunAvailable(bool available)
{
    m_gunAvailable = available;
    // Арбалет сняли прямо в руках — тут же переключаем на копьё, а не оставляем висеть в руках недоступное
    // оружие (Q следующим нажатием и так пропустил бы Gun, но tryAttack() бил бы в пустоту до этого момента).
    if (!available && m_current == Weapon::Gun) {
        m_current = Weapon::Spear;
        LOG_INFO("Player: арбалет снят, переключаюсь на копьё");
    }
}

void WeaponComponent::tryReload()
{
    // Только у пистолета, только если не перезаряжаемся уже, есть что дозарядить и есть из чего (резерв не пуст).
    if (m_current != Weapon::Gun || isReloading() || m_ammo >= m_magazineSize || m_reserveAmmo <= 0) {
        return;
    }
    m_reloadRemaining = m_reloadDuration;
    LOG_INFO("Player-Gun: перезарядка (R)");
}

void WeaponComponent::tryAttack()
{
    if (m_current == Weapon::Spear) {
        if (m_spearAttack) {
            m_spearAttack->tryAttack();
        }
        return;
    }

    if (m_current == Weapon::Gun) {
        // В процессе перезарядки или без патронов (магазин пуст — ждёт R, см. tryReload()) не стреляем.
        if (isReloading() || m_ammo <= 0 || !m_gunAttack) {
            return;
        }
        if (m_gunAttack->tryShoot()) {
            --m_ammo;
        }
        return;
    }

    // Unarmed — бить нечем.
}

void WeaponComponent::reset()
{
    m_current = Weapon::Spear;
    m_ammo = m_magazineSize;
    m_reserveAmmo = m_reserveAmmoMax;
    m_reloadRemaining = sf::Time::Zero;
    m_switchEdge.sync(false);
    m_reloadEdge.sync(false);

    GameObject* owner = getOwner();
    auto* input = owner ? owner->getComponent<InputComponent>() : nullptr;
    if (input) {
        input->setMovementLocked(false);
        input->setSprintSuppressed(false);
    }
}
