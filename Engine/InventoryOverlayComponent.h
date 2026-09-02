#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include "InputEdge.h"
#include "OverlayPanelBase.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <map>
#include <string>
#include <vector>

// Данные одного слота (мешка или экипировки) на момент кадра — не хранятся, а запрашиваются заново каждый
// update() через callback (см. конструктор), поэтому сам компонент ничего не знает про ItemDefinition/
// InventoryComponent/ItemCategory — это игровые типы, живут в Rogalique.exe (см. класс-комментарий ниже почему
// это принципиально).
struct InventorySlotView {
    std::string iconPath;   // Пусто — слот пуст, рисуется только рамка.
    int iconFrameCount = 1; // Спрайт-лист иконки (кадры по ширине); показывается всегда 0-й кадр, без анимации.
    int count = 0;          // Счётчик стека рисуется, только если > 1.
};

// Оверлей инвентаря (мешок N слотов + экипировка). ЦЕЛИКОМ engine-side — раньше жил в Rogalique.exe и хранил
// sf::Sprite/Texture/Font/Text как поля прямо там; это ломало рендер всего окна намертво (чёрный экран, ни
// одного исключения, при этом мир и AI продолжали тикать нормально), как только конструировался хоть один такой
// объект. Причина, подтверждённая отладчиком (Visual Studio + EnvDTE, бисекция по конструктору): Engine.dll и
// Rogalique.exe оба линкуют SFML статически (SFML_STATIC) — это две независимые копии рантайма SFML в одном
// процессе, и построение GL-текстуры кодом из "чужой" копии путает shared GL-контекст, которым в реальности
// владеет Engine.dll (там же RenderSystem/окно). Все ДРУГИЕ оверлеи (Menu/Settings/Credits/Help) работают именно
// потому что целиком живут в Engine.dll. Отсюда и генерик-интерфейс ниже: класс ничего не знает о конкретных
// предметах игры, только про generic-слоты (InventorySlotView) и коллбэки в/из вызывающего кода.
class ENGINE_API InventoryOverlayComponent : public IComponent {
public:
    // equipFrameTexturePaths — по одной текстуре рамки на слот экипировки, в порядке, важном только вызывающей
    // стороне (она же расшифровывает индекс слота обратно в свою категорию предмета в onEquipSlotClicked).
    // getBagSlot/getEquipSlot зовутся каждый кадр, пока оверлей открыт, — актуальное состояние инвентаря без
    // необходимости этому классу его хранить. onClose нужен, чтобы вызывающая сторона перечитала реальное
    // состояние клавиш игрока (тот же класс бага, что чинили у меню паузы, см. PauseToggleComponent).
    InventoryOverlayComponent(sf::Vector2f windowSize, const std::string& panelTexturePath,
        const std::string& emptySlotTexturePath, const std::string& fontPath, std::string title, std::string equipLabel,
        std::string bagLabel, int bagSize, std::vector<std::string> equipFrameTexturePaths,
        std::function<InventorySlotView(int)> getBagSlot, std::function<InventorySlotView(int)> getEquipSlot,
        std::function<void(int)> onBagSlotClicked, std::function<void(int)> onEquipSlotClicked, std::function<void()> onClose);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;

private:
    struct SlotView {
        sf::Sprite frame;
        sf::Sprite icon;
        sf::Text count;
        bool hasIcon = false;
    };

    void layout();
    void layoutSlot(SlotView& view, sf::Vector2f position);
    void open();
    void close();
    void updateMouse();
    void refreshVisuals();
    void applyIcon(SlotView& view, const InventorySlotView& data);
    const sf::Texture* iconTexture(const std::string& path);

    int m_bagSize;
    int m_bagCols;
    int m_bagRows;
    int m_equipCols;
    int m_equipRows;

    std::function<InventorySlotView(int)> m_getBagSlot;
    std::function<InventorySlotView(int)> m_getEquipSlot;
    std::function<void(int)> m_onBagSlotClicked;
    std::function<void(int)> m_onEquipSlotClicked;
    std::function<void()> m_onClose;

    sf::Vector2f m_windowSize;
    bool m_visible = false;

    KeyEdge m_toggleEdge;
    KeyEdge m_escapeEdge;
    KeyEdge m_rmbEdge;
    KeyEdge m_mouseLeftEdge;

    OverlayPanelBase m_base;

    sf::Texture m_emptySlotTexture;
    bool m_hasEmptySlotTexture = false;

    std::map<std::string, sf::Texture> m_iconTextures;

    std::vector<sf::Texture> m_equipFrameTextures;
    std::vector<SlotView> m_equipSlots;
    std::vector<SlotView> m_bagSlots;

    sf::Text m_equipLabel;
    sf::Text m_bagLabel;
};
