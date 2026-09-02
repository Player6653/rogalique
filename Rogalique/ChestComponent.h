#pragma once
#include "FocusedInput.h"
#include "IComponent.h"
#include "InputEdge.h"
#include "ItemDefinition.h"
#include <SFML/Window/Keyboard.hpp>
#include <string>

class SpriteComponent;
class PickupGlowComponent;

// Сундук — открывается по E в радиусе (см. update()), как ItemPickupComponent с requiresInteract=true. В отличие
// от него при открытии НЕ прячет спрайт, а переключает текстуру icon на openTexturePath и остаётся видимым
// навсегда — сундук на карте не исчезает, просто меняет вид на "открыт". reset() возвращает закрытый вид и
// доступность — тот же жизненный цикл, что у ArrowCrateComponent/TrapComponent (resetComponents() при
// "Начать"/"В главное меню"); при "Продолжить"/"Загрузить сохранение" reset() тоже зовётся (общий проход по
// уровню), но следом SceneFacade избирательно возвращает уже открытые по сейву сундуки через
// markOpenedFromSave() — иначе предмет, уже осевший в сохранённом инвентаре, выдавался бы повторно при повторном
// открытии того же сундука (баг-дубликат, найден при аудите сохранений; см. GameMemento::getOpenedChests).
class ChestComponent : public IComponent {
public:
    ChestComponent(const ItemDefinition& item, int count, SpriteComponent& icon, PickupGlowComponent& glow,
        std::string idleTexturePath, std::string openTexturePath);

    void update(sf::Time dt) override;
    void reset() override;

    // Восстанавливает "открыт" из сейва (см. GameMemento::getOpenedChests) — без повторной выдачи предмета (он
    // уже восстановлен в мешок через applyInventoryMemento в SceneFacade) и без звука/тоста, чтобы загрузка не
    // выглядела как только что открытый сундук.
    void markOpenedFromSave();

    // См. ItemPickupComponent::resyncInteract() — тот же приём для E: без этого держать E зажатой всё время, что
    // мир на паузе, а отпустить уже после снятия паузы рядом с сундуком — засчиталось бы ложным открытием на
    // первом кадре геймплея.
    void resyncInteract()
    {
        m_interactEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::E));
    }

    bool isOpened() const
    {
        return m_opened;
    }

private:
    const ItemDefinition& m_item;
    int m_count;
    SpriteComponent& m_icon;
    PickupGlowComponent& m_glow;
    std::string m_idleTexturePath;
    std::string m_openTexturePath;
    bool m_opened = false;
    KeyEdge m_interactEdge;
};
