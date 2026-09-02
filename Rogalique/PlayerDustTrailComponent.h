#pragma once
#include "IComponent.h"
#include <SFML/System/Time.hpp>

class InputComponent;

// Пока игрок бежит (спринт, см. SPRINT_THRESHOLD в .cpp — тот же порог, что и PlayerAnimationComponent использует
// для ролика Run), раз в интервал поднимает под ногами пару частиц пыли (см. ParticleSystem). Не заменяет готовый
// спрайтовый пылевой эффект рывка/прыжка (см. Player::getDustSprite()/PlayerAnimationComponent) — тот рисуется по
// настоящим кадрам из пака, здесь же пыли на обычный бег в паке вообще нет, поэтому процедурные частицы.
class PlayerDustTrailComponent : public IComponent {
public:
    // feetOffsetY — на сколько пикселей ниже центра владельца спавнить частицы (спрайт игрока крупнее коллайдера
    // и заякорен по центру, ноги — это низ спрайта, см. Player.cpp VISUAL_SIZE).
    PlayerDustTrailComponent(InputComponent& input, float feetOffsetY);

    void update(sf::Time dt) override;
    void reset() override;

private:
    InputComponent& m_input;
    float m_feetOffsetY;
    sf::Time m_timer;
};
