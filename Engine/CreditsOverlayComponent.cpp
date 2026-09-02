#include "pch.h"
#include "CreditsOverlayComponent.h"
#include "AudioSystem.h"
#include "FocusedInput.h"
#include "GameWorld.h"
#include "Log.h"
#include "RenderSystem.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float PANEL_WIDTH = 560.f;
    constexpr float LINE_HEIGHT = 28.f;
    constexpr float TITLE_HEIGHT = 50.f;
    constexpr float PANEL_PADDING = 28.f;
    constexpr unsigned LINE_CHARACTER_SIZE = 15;
    // Место под ряд "< 1/2 >" под текстом — только если страниц больше одной.
    constexpr float NAV_ROW_HEIGHT = 40.f;
    constexpr float NAV_ICON_SIZE = 28.f;
    constexpr float NAV_ICON_GAP = 16.f;
} // namespace

CreditsOverlayComponent::CreditsOverlayComponent(sf::Vector2f windowSize, const std::string& panelTexturePath,
    const std::string& fontPath, std::vector<Page> pages, std::string arrowLeftTexturePath, std::string arrowRightTexturePath,
    std::string moveSoundName)
    // Заголовок первой страницы читаем из pages ДО того, как ниже в теле конструктора её переместят в m_pages —
    // OverlayPanelBase нужен хоть какой-то заголовок уже на этапе конструирования, а не после layout()/rebuildCurrentPageTexts().
    : m_moveSoundName(std::move(moveSoundName)),
      m_arrowLeftTexturePath(std::move(arrowLeftTexturePath)),
      m_arrowRightTexturePath(std::move(arrowRightTexturePath)),
      m_base(windowSize, panelTexturePath, fontPath, pages.empty() ? std::string() : pages.front().title, 26,
          sf::Color(255, 215, 90), true, 190)
{
    // Сама загрузка текстур стрелок происходит в setPages() ниже, а не здесь — раньше решение "грузить или нет"
    // принималось один раз тут же, по количеству страниц НА МОМЕНТ КОНСТРУКТОРА (баг: "в помощи пропали кнопки
    // лево-право", когда m_pages ещё пустая), а следующий баг того же класса — если конструктор строит компонент
    // с <=1 страницей (тогда грузить действительно нечего), а setPages() позже, во время работы игры, даёт ему
    // больше одной (см. таблицу лидеров) — стрелки должны загрузиться именно тогда, а не остаться незагруженными
    // навсегда. setPages() ниже сама решает, когда грузить, по актуальному m_pages.size().
    if (m_base.hasFont()) {
        m_pageIndicatorText.setFont(m_base.getFont());
        m_pageIndicatorText.setCharacterSize(16);
        m_pageIndicatorText.setFillColor(sf::Color(220, 220, 220));
    }

    setPages(std::move(pages));
}

void CreditsOverlayComponent::setPages(std::vector<Page> pages)
{
    // Публичный — нужен экрану таблицы рекордов (см. Rogalique/SceneFacade.cpp): в отличие от титров/помощи,
    // содержимое там меняется между показами (новый рекорд добавился), а конструктор зовётся только один раз за
    // весь процесс. Пересчитывает всё, что раньше делал только конструктор — перенос по словам, layout(),
    // тексты текущей страницы — и возвращает на первую страницу.
    m_pages = std::move(pages);
    if (m_pages.empty()) {
        m_pages.emplace_back();
    }
    m_currentPage = 0;

    // Грузим текстуры стрелок здесь, а не в конструкторе (см. класс-комментарий там) — только когда они реально
    // нужны (страниц больше одной) И ещё не загружены (не перезагружаем файл заново при каждом setPages(), это
    // может звать таблица рекордов при каждом открытии).
    if (m_pages.size() > 1 && !m_hasArrowTextures && !m_arrowLeftTexturePath.empty() && !m_arrowRightTexturePath.empty()) {
        bool leftOk = m_leftArrowTexture.loadFromFile(m_arrowLeftTexturePath);
        bool rightOk = m_rightArrowTexture.loadFromFile(m_arrowRightTexturePath);
        m_hasArrowTextures = leftOk && rightOk;
        if (!m_hasArrowTextures) {
            LOG_WARN("CreditsOverlayComponent: не удалось загрузить стрелки листания страниц");
        } else {
            m_leftArrowSprite.setTexture(m_leftArrowTexture);
            m_rightArrowSprite.setTexture(m_rightArrowTexture);
        }
    }

    // Перенос по словам — один раз для всех страниц сразу, ДО layout() (та должна знать итоговую высоту панели
    // под самую длинную страницу, а перенесённых строк может быть больше, чем исходных, см. m_wrappedPages).
    float maxTextWidth = PANEL_WIDTH - PANEL_PADDING * 2.f;
    m_wrappedPages.clear();
    m_wrappedPages.reserve(m_pages.size());
    for (const Page& page : m_pages) {
        std::vector<std::string> wrapped;
        for (const std::string& line : page.lines) {
            std::vector<std::string> pieces = wrapLine(line, maxTextWidth);
            wrapped.insert(wrapped.end(), pieces.begin(), pieces.end());
        }
        m_wrappedPages.push_back(std::move(wrapped));
    }

    layout();
    rebuildCurrentPageTexts();
}

std::vector<std::string> CreditsOverlayComponent::wrapLine(const std::string& line, float maxWidth) const
{
    if (!m_base.hasFont() || line.empty()) {
        return {line};
    }

    // Разбиение по ASCII-пробелу — UTF-8-безопасно: пробел (0x20) не встречается ни в одном байте многобайтовой
    // кириллической последовательности, так что строка не может разрезаться посреди символа.
    std::vector<std::string> words;
    std::string word;
    for (char c : line) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    sf::Text probe;
    probe.setFont(m_base.getFont());
    probe.setCharacterSize(LINE_CHARACTER_SIZE);

    std::vector<std::string> result;
    std::string current;
    for (const std::string& w : words) {
        std::string candidate = current.empty() ? w : current + " " + w;
        probe.setString(sf::String::fromUtf8(candidate.begin(), candidate.end()));
        if (probe.getLocalBounds().width > maxWidth && !current.empty()) {
            result.push_back(current);
            current = w;
        } else {
            current = candidate;
        }
    }
    result.push_back(current); // Пустая строка (спейсер, см. итоги вызовов выше) тоже должна остаться одной строкой.
    return result;
}

void CreditsOverlayComponent::layout()
{
    std::size_t maxLines = 0;
    for (const auto& wrapped : m_wrappedPages) {
        maxLines = std::max(maxLines, wrapped.size());
    }

    float panelHeight = TITLE_HEIGHT + maxLines * LINE_HEIGHT + PANEL_PADDING * 2.f;
    if (m_pages.size() > 1) {
        panelHeight += NAV_ROW_HEIGHT;
    }
    sf::Vector2f panelSize(PANEL_WIDTH, panelHeight);
    sf::Vector2f panelPos = m_base.computePanelPosition(panelSize);
    m_base.layout(panelPos, panelSize, panelPos.y + PANEL_PADDING + TITLE_HEIGHT / 2.f);

    m_panelCenterX = panelPos.x + panelSize.x / 2.f;
    m_firstLinePosition = sf::Vector2f(m_panelCenterX, panelPos.y + PANEL_PADDING + TITLE_HEIGHT);
    m_lineHeight = LINE_HEIGHT;

    if (m_pages.size() > 1) {
        float navY = panelPos.y + panelSize.y - PANEL_PADDING - NAV_ROW_HEIGHT / 2.f;
        if (m_hasArrowTextures) {
            sf::Vector2u textureSize = m_leftArrowTexture.getSize();
            int frameWidth = static_cast<int>(textureSize.x) / 4;
            float scale = NAV_ICON_SIZE / static_cast<float>(textureSize.y);
            m_leftArrowSprite.setTextureRect(sf::IntRect(0, 0, frameWidth, textureSize.y));
            m_rightArrowSprite.setTextureRect(sf::IntRect(0, 0, frameWidth, textureSize.y));
            m_leftArrowSprite.setScale(scale, scale);
            m_rightArrowSprite.setScale(scale, scale);
            float halfIcon = frameWidth * scale / 2.f;
            m_leftArrowSprite.setOrigin(frameWidth / 2.f, textureSize.y / 2.f);
            m_rightArrowSprite.setOrigin(frameWidth / 2.f, textureSize.y / 2.f);
            m_leftArrowSprite.setPosition(std::round(m_panelCenterX - NAV_ICON_GAP - halfIcon), std::round(navY));
            m_rightArrowSprite.setPosition(std::round(m_panelCenterX + NAV_ICON_GAP + halfIcon), std::round(navY));
        }
        m_pageIndicatorText.setPosition(std::round(m_panelCenterX), std::round(navY));
    }
}

void CreditsOverlayComponent::rebuildCurrentPageTexts()
{
    m_base.setTitle(m_pages[m_currentPage].title);

    const std::vector<std::string>& lines = m_wrappedPages[m_currentPage];
    m_lines.clear();
    m_lines.reserve(lines.size());

    for (std::size_t i = 0; i < lines.size(); ++i) {
        sf::Text text;
        if (m_base.hasFont()) {
            text.setFont(m_base.getFont());
            text.setString(sf::String::fromUtf8(lines[i].begin(), lines[i].end()));
            text.setCharacterSize(LINE_CHARACTER_SIZE);
            text.setFillColor(sf::Color::White);
            sf::FloatRect bounds = text.getLocalBounds();
            text.setOrigin(std::round(bounds.left + bounds.width / 2.f), std::round(bounds.top + bounds.height / 2.f));
            text.setPosition(
                std::round(m_firstLinePosition.x), std::round(m_firstLinePosition.y + i * m_lineHeight + m_lineHeight / 2.f));
        }
        m_lines.push_back(text);
    }

    if (m_pages.size() > 1 && m_base.hasFont()) {
        std::string indicator = std::to_string(m_currentPage + 1) + "/" + std::to_string(m_pages.size());
        m_pageIndicatorText.setString(indicator);
        sf::FloatRect bounds = m_pageIndicatorText.getLocalBounds();
        m_pageIndicatorText.setOrigin(std::round(bounds.left + bounds.width / 2.f), std::round(bounds.top + bounds.height / 2.f));
    }
}

void CreditsOverlayComponent::goToPage(int delta)
{
    if (m_pages.size() <= 1) {
        return;
    }
    int next = static_cast<int>(m_currentPage) + delta;
    next = std::max(0, std::min(static_cast<int>(m_pages.size()) - 1, next));
    if (static_cast<std::size_t>(next) == m_currentPage) {
        return;
    }
    m_currentPage = static_cast<std::size_t>(next);
    rebuildCurrentPageTexts();
    if (!m_moveSoundName.empty()) {
        AudioSystem::instance().playSound(m_moveSoundName);
    }
}

void CreditsOverlayComponent::show()
{
    m_visible = true;
    m_currentPage = 0;
    rebuildCurrentPageTexts();
    GameWorld::instance().setModalOpen(true);
    // Пересчитываем было ли зажато по факту, а не сбрасываем в false.
    m_escapeEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Escape));
    m_rmbEdge.sync(FocusedInput::isButtonPressed(sf::Mouse::Right));
    m_leftEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Left));
    m_rightEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Right));
    m_mouseLeftEdge.sync(FocusedInput::isButtonPressed(sf::Mouse::Left));
}

void CreditsOverlayComponent::update(sf::Time dt)
{
    if (!m_visible) {
        m_escapeEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Escape));
        m_rmbEdge.sync(FocusedInput::isButtonPressed(sf::Mouse::Right));
        m_leftEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Left));
        m_rightEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::Right));
        m_mouseLeftEdge.sync(FocusedInput::isButtonPressed(sf::Mouse::Left));
        return;
    }

    bool escapePressed = m_escapeEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Escape));
    // ПКМ в любом месте экрана — тот же назад, что и Escape.
    bool rmbPressed = m_rmbEdge.poll(FocusedInput::isButtonPressed(sf::Mouse::Right));

    if (escapePressed || rmbPressed) {
        m_visible = false;
        GameWorld::instance().setModalOpen(false);
        return;
    }

    if (m_leftEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Left))) {
        goToPage(-1);
    }
    if (m_rightEdge.poll(FocusedInput::isKeyPressed(sf::Keyboard::Right))) {
        goToPage(1);
    }

    updateMouse();
}

void CreditsOverlayComponent::updateMouse()
{
    if (!m_hasArrowTextures || m_pages.size() <= 1) {
        return;
    }

    sf::RenderWindow& window = RenderSystem::instance().getWindow();
    sf::Vector2f mousePos(sf::Mouse::getPosition(window));

    bool overLeft = m_leftArrowSprite.getGlobalBounds().contains(mousePos);
    bool overRight = m_rightArrowSprite.getGlobalBounds().contains(mousePos);

    sf::Vector2u textureSize = m_leftArrowTexture.getSize();
    int frameWidth = static_cast<int>(textureSize.x) / 4;
    m_leftArrowSprite.setTextureRect(sf::IntRect((overLeft ? 1 : 0) * frameWidth, 0, frameWidth, textureSize.y));
    m_rightArrowSprite.setTextureRect(sf::IntRect((overRight ? 1 : 0) * frameWidth, 0, frameWidth, textureSize.y));

    if (!m_mouseLeftEdge.poll(FocusedInput::isButtonPressed(sf::Mouse::Left))) {
        return;
    }

    if (overLeft) {
        goToPage(-1);
    } else if (overRight) {
        goToPage(1);
    }
}

void CreditsOverlayComponent::draw(sf::RenderWindow& window) const
{
    if (!m_visible) {
        return;
    }

    m_base.draw(window);
    if (m_base.hasFont()) {
        for (const sf::Text& line : m_lines) {
            window.draw(line);
        }
    }

    if (m_pages.size() > 1) {
        if (m_hasArrowTextures) {
            window.draw(m_leftArrowSprite);
            window.draw(m_rightArrowSprite);
        }
        if (m_base.hasFont()) {
            window.draw(m_pageIndicatorText);
        }
    }
}
