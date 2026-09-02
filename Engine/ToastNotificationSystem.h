#pragma once
#include "EngineExport.h"
#include <SFML/System/Time.hpp>
#include <deque>
#include <string>

// Синглтон очереди всплывающих уведомлений ("Подобрано: Зелье лечения" и т.п.) — тот же приём, что у AudioSystem:
// любой код (например, ItemPickupComponent в Rogalique/) зовёт show() напрямую, не имея ссылки на конкретный
// UI-компонент. Сама отрисовка/тайминг — в ToastNotificationComponent (см. тот же каталог), который каждый кадр
// зовёт update() этого синглтона и рисует то, что тот считает активным сейчас. Разделение так же, как у
// RenderSystem/AudioSystem — управляет состоянием, но ничего сам не рисует (для рисования нужен sf::RenderWindow,
// а до него синглтоны Engine-уровня напрямую не должны дотягиваться).
class ENGINE_API ToastNotificationSystem {
public:
    static ToastNotificationSystem& instance();

    // Ставит сообщение в очередь — если сейчас ничего не показывается, станет активным немедленно, иначе покажется
    // после того, как предыдущие в очереди доиграют. Не ограничена по длине очереди — всплывающих сообщений в игре
    // не настолько много за раз, чтобы это стало проблемой.
    void show(std::string text);

    // Продвигает таймер активного сообщения/берёт следующее из очереди — звать раз в кадр из
    // ToastNotificationComponent::update(), не откуда-либо ещё.
    void update(sf::Time dt);

    bool hasActive() const
    {
        return !m_activeText.empty();
    }
    const std::string& getActiveText() const
    {
        return m_activeText;
    }
    // 0..1 — для плавного проявления/исчезновения, а не резкого моргания.
    float getActiveAlpha() const
    {
        return m_activeAlpha;
    }

private:
    ToastNotificationSystem() = default;

    std::deque<std::string> m_queue;
    std::string m_activeText;
    sf::Time m_activeRemaining;
    float m_activeAlpha = 0.f;
};
