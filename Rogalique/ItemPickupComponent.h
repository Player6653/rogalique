#pragma once
#include "FocusedInput.h"
#include "IComponent.h"
#include "InputEdge.h"
#include "ItemDefinition.h"
#include <SFML/Window/Keyboard.hpp>

class SpriteComponent;
class PickupGlowComponent;

// Предмет на карте, который можно подобрать — тот же приём, что у ArrowCrateComponent (см. ArrowCrateComponent.h):
// при подборе не уничтожается (GameObject::destroy() необратим), а прячет спрайт и запоминает "уже подобран";
// reset() возвращает его на место, тем же механизмом, что и весь остальной изначальный ростер сцены при рестарте
// уровня (см. object.resetComponents() в SceneFacade).
//
// requiresInteract=false (по умолчанию) — мелкие предметы подбираются просто проходом рядом (см. update()).
// true — нужен ещё и явный E (сундуки/ящики с добычей: чтобы игрок не подбирал их случайно на бегу мимо).
class ItemPickupComponent : public IComponent {
public:
    ItemPickupComponent(
        const ItemDefinition& item, int count, SpriteComponent& icon, PickupGlowComponent& glow, bool requiresInteract = false);

    void update(sf::Time dt) override;
    void reset() override;

    // См. WeaponComponent::resyncInput() — тот же приём для E: без этого держать E зажатой всё время, что мир на
    // паузе (инвентарь, меню паузы...), а отпустить уже после снятия паузы рядом с предметом — засчиталось бы
    // ложным подбором на первом кадре геймплея. Публичный, потому что вызывающий (SceneFacade) не хранит ссылку
    // на конкретный ItemPickupComponent — находит все разом через GameObject::getComponentsInChildren().
    void resyncInteract()
    {
        m_interactEdge.sync(FocusedInput::isKeyPressed(sf::Keyboard::E));
    }

    bool isCollected() const
    {
        return m_collected;
    }
    // Только для восстановления сейва (см. SceneFacade) — не обычный игровой подбор, тот идёт через update().
    // true после resetComponents() (который прячущийся спрайт возвращает) нужен, чтобы предмет, уже лежащий в
    // мешке загруженного сейва, не продублировался на карте ещё и подбираемым заново.
    void setCollected(bool collected);

private:
    const ItemDefinition& m_item;
    int m_count;
    SpriteComponent& m_icon;
    PickupGlowComponent& m_glow;
    bool m_requiresInteract;
    bool m_collected = false;
    KeyEdge m_interactEdge;
};
