#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include "InputEdge.h"
#include <SFML/Graphics.hpp>
#include <functional>

// Единственная задача слушать Escape и переключать GameWorld::isPaused(). Раньше это было побочной обязанностью MenuOverlayComponent (флаг escapeToggle).
class ENGINE_API PauseToggleComponent : public IComponent {
public:
    // onUnpause зовётся сразу после снятия паузы этим Escape (не после её постановки) — нужно вызывающей стороне,
    // чтобы досинхронизировать состояние игрока (см. InputComponent::resyncInput()): пока мир на паузе, его
    // update() не вызывается, и клавиша, которой сняли паузу здесь тем же нажатием Escape мимо любого меню, на
    // первом кадре геймплея иначе читалась бы неверно, если эта же клавиша ещё и клавиша движения/рывка/прыжка.
    explicit PauseToggleComponent(std::function<void()> onUnpause = nullptr);

    void update(sf::Time dt) override;

private:
    KeyEdge m_escapeEdge;
    std::function<void()> m_onUnpause;
};
