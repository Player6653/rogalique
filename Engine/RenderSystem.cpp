#include "pch.h"
#include "RenderSystem.h"
#include "GameException.h"
#include "Log.h"

RenderSystem& RenderSystem::instance()
{
    static RenderSystem instance;
    return instance;
}

void RenderSystem::createWindow(unsigned width, unsigned height, const std::string& title, bool fullscreen,
    const std::string& iconPath)
{
    sf::VideoMode mode = fullscreen ? sf::VideoMode::getDesktopMode() : sf::VideoMode(width, height);
    sf::Uint32 style = fullscreen ? sf::Style::Fullscreen : sf::Style::Default;
    m_window = std::make_unique<sf::RenderWindow>(mode, title, style);
    if (!m_window->isOpen()) {
        std::string message
            = "RenderSystem: не удалось создать окно " + std::to_string(mode.width) + "x" + std::to_string(mode.height);
        LOG_ERROR(message);
        throw GameException(message);
    }
    LOG_INFO("RenderSystem: окно \"" + title + "\" создано (" + std::to_string(mode.width) + "x" + std::to_string(mode.height)
             + (fullscreen ? ", fullscreen)" : ")"));

    if (!iconPath.empty()) {
        sf::Image icon;
        // SFML умеет грузить PNG (через stb_image), но не .ico — та же иконка, что зашита в .exe-ресурс, здесь
        // должна лежать отдельным PNG-файлом рядом в Resources.
        if (icon.loadFromFile(iconPath)) {
            m_window->setIcon(icon.getSize().x, icon.getSize().y, icon.getPixelsPtr());
        } else {
            LOG_WARN("RenderSystem: не удалось загрузить иконку окна \"" + iconPath + "\"");
        }
    }
}

sf::RenderWindow& RenderSystem::getWindow()
{
    return *m_window;
}

void RenderSystem::beginFrame()
{
    m_window->clear();
}

void RenderSystem::endFrame()
{
    m_window->display();
}

void RenderSystem::setFramerateLimit(unsigned limit)
{
    m_framerateLimit = limit;
    m_window->setFramerateLimit(limit);
}
