#include "PlayerDeathComponent.h"
#include "AudioSystem.h"
#include "GameWorld.h"
#include "HealthComponent.h"

PlayerDeathComponent::PlayerDeathComponent(HealthComponent* playerHealth)
    : m_playerHealth(playerHealth)
{
}

void PlayerDeathComponent::update(sf::Time)
{
    // isVictory() тоже гасит смерть — иначе игрок, открывший дверь на грани HP, мог словить удар ботом парой кадров
    // позже и получить экран поражения ПОВЕРХ уже показанного экрана победы (оба не ставят мир на паузу сами, см.
    // комментарии у GameWorld::isGameOver/isVictory, поэтому оба могут гипотетически сработать в один и тот же забег).
    if (!GameWorld::instance().hasStarted() || GameWorld::instance().isGameOver() || GameWorld::instance().isVictory()) {
        return;
    }

    if (m_playerHealth && m_playerHealth->isDead()) {
        // Гарантированно один раз — та же проверка isGameOver() выше не даёт этой ветке сработать повторно, пока
        // экран поражения не закрыт (gameover.wav раньше лежал неиспользуемым).
        AudioSystem::instance().playMusic("Resources/Sounds/gameover.wav", false);
        GameWorld::instance().setGameOver(true);
    }
}
