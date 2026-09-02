#pragma once
#include "EngineExport.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <string>

// Синглтон единственное окно приложения и точка входа для отрисовки кадра.
class ENGINE_API RenderSystem {
public:
    static RenderSystem& instance();

    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;

    // fullscreen=true игнорирует width/height и берёт текущее разрешение рабочего стола (sf::VideoMode::getDesktopMode())
    // — SFML-полноэкранный режим (sf::Style::Fullscreen) требует валидный видеорежим, а не произвольный размер.
    // iconPath — иконка заголовка окна/панели задач, пустая строка (по умолчанию) — оставить системную иконку по
    // умолчанию. Отдельно от иконки самого .exe-файла (та зашивается в бинарник через .rc-ресурс на этапе сборки,
    // см. Rogalique.rc) — SFML её сама не подхватывает, окну нужно явно передать пиксели через sf::Image.
    void createWindow(unsigned width, unsigned height, const std::string& title, bool fullscreen = false,
        const std::string& iconPath = "");
    sf::RenderWindow& getWindow();

    void beginFrame(); // window.clear()
    void endFrame();   // window.display()

    // 0 — без ограничения.
    void setFramerateLimit(unsigned limit);
    unsigned getFramerateLimit() const
    {
        return m_framerateLimit;
    }

private:
    RenderSystem() = default;

    std::unique_ptr<sf::RenderWindow> m_window;
    unsigned m_framerateLimit = 0;
};
