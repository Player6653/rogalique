#pragma once
#include "FocusedInput.h"
#include "IComponent.h"
#include "InputEdge.h"
#include <SFML/Window/Keyboard.hpp>
#include <functional>
#include <string>
#include <vector>

class SpriteComponent;

// Дверь открывается по E в радиусе (тот же приём, что ChestComponent), но только если в мешке игрока лежат ВСЕ
// ключи из requiredKeyIds (id предметов, см. ItemDefinition.cpp — по умолчанию 4 ключа сторон света, см.
// SceneFacade::run()). Ключи не расходуются: это метка прогресса ("нашёл все"), а не расходник. При успехе зовёт
// onOpened() (если задан, см. setOnOpened) вместо того чтобы решать самой, что происходит дальше — раньше дверь
// сама выставляла GameWorld::setVictory(true), теперь это отдаёт вызывающему коду (SceneFacade), потому что
// открытие двери телепортирует на арену волн, а не сразу победа (та наступает только после всех волн).
// reset() возвращает закрытый вид — тот же жизненный цикл, что у ChestComponent (resetComponents() при "Начать"/
// "В главное меню"/загрузке в SceneFacade).
class DoorComponent : public IComponent {
public:
    DoorComponent(SpriteComponent& icon, std::string closedTexturePath, std::string openTexturePath,
        std::vector<std::string> requiredKeyIds);

    void update(sf::Time dt) override;
    void reset() override;

    // См. ChestComponent::resyncInteract() — тот же приём для E, тот же повод.
    void resyncInteract()
    {
        m_interactEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::E));
    }

    bool isOpen() const
    {
        return m_open;
    }

    // Зовётся ровно один раз, в момент успешного открытия (см. update()) — SceneFacade вешает сюда телепорт на
    // арену волн. Не в конструкторе, потому что колбэку обычно нужны объекты (playerObject и т.п.), которых на
    // момент создания Door в SceneFacade::run() ещё может не быть под рукой в этом месте кода.
    void setOnOpened(std::function<void()> callback)
    {
        m_onOpened = std::move(callback);
    }

private:
    SpriteComponent& m_icon;
    std::string m_closedTexturePath;
    std::string m_openTexturePath;
    std::vector<std::string> m_requiredKeyIds;
    bool m_open = false;
    KeyEdge m_interactEdge;
    std::function<void()> m_onOpened;
};
