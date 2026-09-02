#pragma once
#include "EngineExport.h"
#include "IComponent.h"

// Пульсирующее пятно света под иконкой предмета/сундука — чисто визуальный маркер "это можно подобрать",
// добавляется ПЕРЕД SpriteComponent самой иконки (см. Rogalique/ItemPickup.cpp/Chest.cpp), чтобы рисоваться под
// ней, а не поверх (порядок компонентов на объекте — это и порядок отрисовки, см. GameObject::draw()). Декор
// (бочки/ящики/кости/мебель — Rogalique/SceneFacade.cpp) такого маркера не получает вовсе, поэтому на глаз сразу
// видно разницу между "с этим можно что-то сделать" и "это просто фон".
//
// Живёт в Engine, а не в Rogalique.exe, хотя используется только там — компонент со своей отрисовкой (window.draw()
// сырых SFML-примитивов) обязан быть скомпилирован в Engine.dll: Engine.dll и Rogalique.exe линкуют SFML_STATIC
// независимо друг от друга, и window.draw() вызов, скомпилированный в EXE, ломает рендер всего окна намертво из-за
// рассинхронизации этих двух статических копий SFML (тот же баг уже был найден и исправлен на InventoryOverlayComponent
// — см. комментарий в SceneFacade.cpp — и на нём же спотыкались попытки подписей над головами ботов).
class ENGINE_API PickupGlowComponent : public IComponent {
public:
    explicit PickupGlowComponent(float baseRadius);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;
    void onOwnerMoved(sf::Vector2f newPosition) override;
    // Возвращает видимость — ItemPickupComponent/ChestComponent сами зовут setVisible(false) при подборе/открытии,
    // reset() здесь только на случай, если что-то ещё когда-нибудь дёрнет resetComponents() без них.
    void reset() override
    {
        m_visible = true;
    }

    // Подобранный предмет/открытый сундук больше не "можно подобрать" — гасим маркер вместе с иконкой
    // (см. ItemPickupComponent/ChestComponent, зовут это там же, где прячут/меняют саму иконку).
    void setVisible(bool visible)
    {
        m_visible = visible;
    }

private:
    float m_baseRadius;
    sf::Time m_elapsed;
    sf::Vector2f m_position;
    bool m_visible = true;
};
