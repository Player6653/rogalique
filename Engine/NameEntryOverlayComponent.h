#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include "InputEdge.h"
#include "OverlayPanelBase.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <string>
#include <vector>

// Экран ввода короткой строки (например, имени для таблицы рекордов) — заголовок + произвольные информационные
// строки (см. show() — "Победа! Время: MM:SS" и т.п., передаются при каждом показе, а не один раз при
// создании — у Rogalique это время конкретного забега, оно каждый раз разное) + редактируемая строка ввода с
// мигающим курсором. Enter подтверждает (не раньше, чем строка непустая) и зовёт onConfirm(name), после чего сам
// скрывается — вызывающий код сам решает, что показать дальше (см. Rogalique/SceneFacade.cpp — экран победы:
// сначала это, потом обычное меню "Играть заново"/"В главное меню"). Символы приходят через TextInputBuffer, не
// через обычный polling-ввод движка (см. NameEntryInput.h — почему).
class ENGINE_API NameEntryOverlayComponent : public IComponent {
public:
    NameEntryOverlayComponent(sf::Vector2f windowSize, const std::string& panelTexturePath, const std::string& fontPath,
        std::string title, std::function<void(const std::string&)> onConfirm);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;

    // Открывает экран с этими info-строками, сбрасывая введённый текст с прошлого раза.
    void show(std::vector<std::string> infoLines);

private:
    void layout();
    void rebuildInfoTexts();
    void rebuildInputText();

    std::string m_title;
    std::vector<std::string> m_infoLines;
    std::function<void(const std::string&)> m_onConfirm;

    bool m_visible = false;
    std::string m_enteredName;
    int m_enteredNameLength = 0; // В символах, не байтах UTF-8 — см. NameEntryInput.h.
    sf::Time m_cursorBlinkElapsed;
    bool m_cursorVisible = true;

    KeyEdge m_enterEdge;

    OverlayPanelBase m_base;
    std::vector<sf::Text> m_infoTexts;
    sf::Text m_inputText;
    sf::Vector2f m_firstLinePosition;
    float m_lineHeight = 0.f;
    float m_inputLineY = 0.f;
};
