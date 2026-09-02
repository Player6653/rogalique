#include "pch.h"
#include "Engine.h"
#include "AudioSystem.h"
#include "CameraComponent.h"
#include "GameWorld.h"
#include "MouseWheelBuffer.h"
#include "RenderSystem.h"
#include "TextInputBuffer.h"

Engine& Engine::instance()
{
    static Engine instance;
    return instance;
}

void Engine::run()
{
    sf::RenderWindow& window = RenderSystem::instance().getWindow();
    GameWorld& world = GameWorld::instance();

    sf::Clock clock;
    m_running = true;

    while (m_running && window.isOpen()) {
        // Очищаем ДО опроса новых событий этого кадра — компоненты (см. TextInputBuffer.h/MouseWheelBuffer.h)
        // читают их уже после, из своего update() ниже, так что должны видеть только события именно этого кадра.
        TextInputBuffer::clear();
        MouseWheelBuffer::clear();
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                m_running = false;
            } else if (event.type == sf::Event::TextEntered) {
                TextInputBuffer::pushChar(event.text.unicode);
            } else if (event.type == sf::Event::MouseWheelScrolled) {
                MouseWheelBuffer::pushDelta(event.mouseWheelScroll.delta);
            }
        }

        if (!m_running) {
            break;
        }

        sf::Time dt = clock.restart();
        // Независимо от паузы мира — музыка и так играет вне GameWorld::isPaused() (см. AudioSystem::playMusic),
        // кроссфейду между треками тоже незачем замирать вместе с игровым временем.
        AudioSystem::instance().update(dt);
        world.update(dt);

        RenderSystem::instance().beginFrame();

        // Активная камера (если есть) применяется один раз до отрисовки сцены — через реестр GameWorld (см.
        // GameWorld::registerCamera), не полным обходом дерева каждый кадр (getComponentsInChildren<CameraComponent>()
        // — так было раньше: с разросшимся деревом декора это была лишняя постоянная нагрузка на КАЖДЫЙ кадр).
        if (CameraComponent* camera = world.getActiveCamera()) {
            camera->apply(window);
        }

        world.draw(window);

        // UI/HUD рисуется поверх мира со стандартным (не камерным) view его объекты позиционируются в экранных пикселях, а не в мировых координатах.
        window.setView(window.getDefaultView());
        world.drawUI(window);

        RenderSystem::instance().endFrame();
    }

    // Сцену чистим, пока GL-контекст окна ещё жив, и только потом закрываем окно.
    world.clear();

    if (window.isOpen()) {
        window.close();
    }
}

void Engine::stop()
{
    m_running = false;
}
