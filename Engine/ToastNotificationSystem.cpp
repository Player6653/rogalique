#include "pch.h"
#include "ToastNotificationSystem.h"
#include <algorithm>

namespace
{
    const sf::Time DISPLAY_DURATION = sf::seconds(2.2f);
    // Одна и та же длительность на фейд-ин и фейд-аут — резкое появление/исчезновение текста в углу экрана
    // отвлекает больше, чем плавное.
    const sf::Time FADE_DURATION = sf::seconds(0.25f);
}

ToastNotificationSystem& ToastNotificationSystem::instance()
{
    static ToastNotificationSystem instance;
    return instance;
}

void ToastNotificationSystem::show(std::string text)
{
    m_queue.push_back(std::move(text));
}

void ToastNotificationSystem::update(sf::Time dt)
{
    if (m_activeText.empty()) {
        if (m_queue.empty()) {
            return;
        }
        m_activeText = std::move(m_queue.front());
        m_queue.pop_front();
        m_activeRemaining = DISPLAY_DURATION;
    }

    m_activeRemaining -= dt;
    if (m_activeRemaining <= sf::Time::Zero) {
        // Не сразу следующее из очереди этим же кадром — короткая пауза между сообщениями (следующий update()
        // подхватит) читается лучше, чем сообщения, впритык сменяющие друг друга без единого кадра паузы.
        m_activeText.clear();
        m_activeAlpha = 0.f;
        return;
    }

    sf::Time elapsed = DISPLAY_DURATION - m_activeRemaining;
    if (elapsed < FADE_DURATION) {
        m_activeAlpha = elapsed.asSeconds() / FADE_DURATION.asSeconds();
    } else if (m_activeRemaining < FADE_DURATION) {
        m_activeAlpha = m_activeRemaining.asSeconds() / FADE_DURATION.asSeconds();
    } else {
        m_activeAlpha = 1.f;
    }
    m_activeAlpha = std::max(0.f, std::min(1.f, m_activeAlpha));
}
