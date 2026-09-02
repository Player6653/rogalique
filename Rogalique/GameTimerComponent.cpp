#include "GameTimerComponent.h"
#include "GameWorld.h"

void GameTimerComponent::update(sf::Time dt)
{
    if (m_stopped) {
        return;
    }
    if (GameWorld::instance().isVictory()) {
        // Останавливаем ровно на времени победы, а не продолжаем считать, пока висит экран победы — тот сам мир
        // тоже ставит на паузу (см. SceneFacade.cpp), но этот компонент живёт в uiRoot и тикал бы дальше, если бы
        // не эта проверка (см. класс-комментарий в .h).
        m_stopped = true;
        return;
    }
    if (!GameWorld::instance().hasStarted() || GameWorld::instance().isPaused() || GameWorld::instance().isGameOver()) {
        return;
    }
    m_elapsed += dt;
}

void GameTimerComponent::reset()
{
    m_elapsed = sf::Time::Zero;
    m_stopped = false;
}
