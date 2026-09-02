#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include "InputEdge.h"
#include "OverlayPanelBase.h"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

// Текстовый оверлей с постраничным листанием (титры, помощь и т.п.) — если страница одна, стрелки/индикатор
// просто не рисуются и не слушают ввод, ведёт себя как раньше.
class ENGINE_API CreditsOverlayComponent : public IComponent {
public:
    // У каждой страницы свой заголовок (панель одна, но текст в шапке меняется вместе со страницей) и свои строки.
    struct Page {
        std::string title;
        std::vector<std::string> lines;
    };

    // arrowLeft/RightTexturePath можно оставить пустыми, если страница всего одна — тогда стрелки/индикатор
    // просто не рисуются и не слушают ввод, ведёт себя как обычный однострочный оверлей.
    CreditsOverlayComponent(sf::Vector2f windowSize, const std::string& panelTexturePath, const std::string& fontPath,
        std::vector<Page> pages, std::string arrowLeftTexturePath = "", std::string arrowRightTexturePath = "",
        std::string moveSoundName = "");

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;

    void show();

    // Меняет содержимое уже созданного оверлея и возвращает на первую страницу — нужно экрану таблицы рекордов
    // (см. Rogalique/SceneFacade.cpp), где список между показами меняется, в отличие от статичных титров/помощи.
    void setPages(std::vector<Page> pages);

private:
    void layout();
    void rebuildCurrentPageTexts();
    void goToPage(int delta);
    // Наводка/клик мышью по стрелкам.
    void updateMouse();
    // Переносит одну строку по словам, если она не влезает в ширину панели — см. класс-комментарий у m_wrappedPages.
    std::vector<std::string> wrapLine(const std::string& line, float maxWidth) const;

    std::vector<Page> m_pages;
    // Строки m_pages, перенесённые по словам под ширину панели (см. wrapLine()) — считаются один раз в
    // конструкторе для ВСЕХ страниц разом (не только текущей), потому что layout() должен знать высоту панели под
    // самую длинную страницу ДО того, как страница вообще открыта. Раньше строки рисовались как есть, без переноса
    // — длинные (например, у "Помощи" на странице предметов) вылезали за края панели и окна.
    std::vector<std::vector<std::string>> m_wrappedPages;
    std::size_t m_currentPage = 0;

    bool m_visible = false;
    KeyEdge m_escapeEdge;
    KeyEdge m_rmbEdge;
    KeyEdge m_leftEdge;
    KeyEdge m_rightEdge;
    KeyEdge m_mouseLeftEdge;

    std::string m_moveSoundName;
    // Пути к текстурам стрелок — сохранены отдельно от m_hasArrowTextures/m_leftArrowTexture, потому что на
    // момент конструктора страниц может быть <=1 (тогда грузить нечего, см. setPages()), а setPages() вызывается
    // и позже, когда страниц уже больше одной (см. класс-комментарий у setPages() — таблица лидеров).
    std::string m_arrowLeftTexturePath;
    std::string m_arrowRightTexturePath;

    OverlayPanelBase m_base;
    std::vector<sf::Text> m_lines;
    // Позиция первой строки и шаг между ними — считаются один раз в layout(), rebuildCurrentPageTexts() их не трогает.
    sf::Vector2f m_firstLinePosition;
    float m_lineHeight = 0.f;
    float m_panelCenterX = 0.f;

    bool m_hasArrowTextures = false;
    sf::Texture m_leftArrowTexture;
    sf::Texture m_rightArrowTexture;
    sf::Sprite m_leftArrowSprite;
    sf::Sprite m_rightArrowSprite;
    sf::Text m_pageIndicatorText;
};
