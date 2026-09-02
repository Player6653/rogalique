#pragma once
#include "IComponent.h"
#include <string>

// Смотрит на направление движения игрока (InputComponent), здоровье (HealthComponent) и текущее оружие
// (WeaponComponent) — набор роликов (Idle/Walk/Run/Dash/Jump/Death/Attack/Shooting/Reloading) целиком зависит
// от того, что сейчас в руках (Normal/Spear/Gun), см. .cpp.
class PlayerAnimationComponent : public IComponent {
public:
    void update(sf::Time dt) override;
    // m_currentClip нарочно не сбрасываем: очередной update() сам увидит несовпадение желаемого клипа с текущим ("Death") и перезагрузит анимацию.
    void reset() override
    {
        m_wasDashing = false;
        m_dashVisualTimeRemaining = sf::Time::Zero;
        m_wasJumping = false;
        m_jumpVisualTimeRemaining = sf::Time::Zero;
        m_meleeVisualTimeRemaining = sf::Time::Zero;
        m_shootVisualTimeRemaining = sf::Time::Zero;
        m_dustVisible = false;
    }

private:
    std::string m_facing = "Down";
    std::string m_currentClip;

    bool m_wasDashing = false;
    sf::Time m_dashVisualTimeRemaining;

    bool m_wasJumping = false;
    sf::Time m_jumpVisualTimeRemaining;

    // Копьё — визуальный импульс на удар, копия того же приёма, что у EnemyAnimationComponent/SoldierAnimationComponent.
    sf::Time m_meleeVisualTimeRemaining;

    // Пистолет — визуальный импульс на выстрел (recoil-поза), отдельно от факта перезарядки.
    sf::Time m_shootVisualTimeRemaining;

    // Пыль под рывок/прыжок — отдельный слой (Player::getDustSprite()), прячем через альфу, пока не активен.
    bool m_dustVisible = false;
};
