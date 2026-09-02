#pragma once
#include "FocusedInput.h"
#include "IComponent.h"
#include "InputEdge.h"
#include <SFML/Window/Keyboard.hpp>

class AttackComponent;
class RangedAttackComponent;

enum class Weapon { Unarmed, Spear, Gun };

// Держит текущее оружие игрока (переключается по Q, по кругу Копьё -> Пистолет -> Безоружен -> ...), патроны и
// перезарядку пистолета (по R, вручную — не автоматом при опустевшем магазине). Пистолет в цикле пропускается,
// пока не надет арбалет (см. setGunAvailable/InventoryComponent) — тогда Q крутит только Копьё <-> Безоружен.
// F/Z (PlayerAttackComponent) зовёт tryAttack() сюда, а не напрямую в AttackComponent — этот компонент сам решает,
// какое из двух "боевых" реально сработает.
class WeaponComponent : public IComponent {
public:
    WeaponComponent(AttackComponent* spearAttack, RangedAttackComponent* gunAttack, int magazineSize, int reserveAmmo,
        sf::Time reloadDuration);

    void update(sf::Time dt) override;
    void reset() override;

    // Перечитывает реальное состояние Q/R — нужно звать сразу после снятия паузы (эти клавиши не polled, пока
    // update() не вызывается вовсе, см. GameWorld::isPaused()), иначе клавиша, зажатая всё время, что было
    // приостановлено, на первом кадре геймплея прочиталась бы как "только что нажали" (тот же класс бага, что и
    // у InputComponent::resyncInput() — см. там подробный комментарий).
    void resyncInput()
    {
        m_switchEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Q));
        m_reloadEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::R));
    }

    // Делегирует текущему оружию. У пистолета сам списывает патрон; магазин может опустеть, но сам не перезаряжается — см. R.
    void tryAttack();

    Weapon getCurrent() const
    {
        return m_current;
    }
    bool isReloading() const
    {
        return m_reloadRemaining > sf::Time::Zero;
    }
    int getAmmo() const
    {
        return m_ammo;
    }
    int getMagazineSize() const
    {
        return m_magazineSize;
    }
    // Запасные болты сверх того, что в "стволе" (см. GUN_RESERVE_AMMO в Player.cpp) — R не срабатывает, если
    // резерв пуст, даже когда m_ammo уже 0 (взять перезарядку неоткуда).
    int getReserveAmmo() const
    {
        return m_reserveAmmo;
    }

    // Пистолет (сейчас единственный — это и есть арбалет, см. ItemDefinition.cpp "crossbow") недоступен, пока в
    // экипировке нет соответствующего Weapon-предмета: ни выбрать по Q, ни оставаться на нём, если его сняли.
    // Зовётся из InventoryComponent::recomputeEquipmentEffects() при каждой экипировке/снятии/сбросе.
    void setGunAvailable(bool available);
    bool isGunAvailable() const
    {
        return m_gunAvailable;
    }

private:
    void switchWeapon();
    void tryReload();
    // Пополняет m_reserveAmmo до максимума, если рядом (тот же приём и радиус, что у SoldierAmmoComponent) есть
    // ArrowCrate и резерв не полон — без кнопки, по факту нахождения рядом.
    void refillFromNearbyCrate();

    AttackComponent* m_spearAttack;
    RangedAttackComponent* m_gunAttack;

    Weapon m_current = Weapon::Spear;
    bool m_gunAvailable = false;
    KeyEdge m_switchEdge;
    KeyEdge m_reloadEdge;

    int m_magazineSize;
    int m_ammo;
    int m_reserveAmmoMax;
    int m_reserveAmmo;
    sf::Time m_reloadDuration;
    sf::Time m_reloadRemaining;
};
