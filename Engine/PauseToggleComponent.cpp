#include "pch.h"
#include "PauseToggleComponent.h"
#include "FocusedInput.h"
#include "GameWorld.h"

PauseToggleComponent::PauseToggleComponent(std::function<void()> onUnpause)
    : m_onUnpause(std::move(onUnpause))
{
}

void PauseToggleComponent::update(sf::Time dt)
{
    if (!GameWorld::instance().hasStarted() || GameWorld::instance().isModalOpen() || GameWorld::instance().isGameOver()) {
        m_escapeEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Escape));
        return;
    }

    bool escapePressed = m_escapeEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Escape));
    if (escapePressed) {
        bool nowPaused = !GameWorld::instance().isPaused();
        GameWorld::instance().setPaused(nowPaused);
        if (!nowPaused && m_onUnpause) {
            m_onUnpause();
        }
    }
}
