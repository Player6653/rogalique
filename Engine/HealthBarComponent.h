#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <string>
#include <vector>

class HealthComponent;

// HUD-полоска здоровья.
class ENGINE_API HealthBarComponent : public IComponent {
public:
    HealthBarComponent(
        HealthComponent& target, const std::string& texturePath, sf::Vector2f size, std::function<bool()> isVisible = nullptr);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;
    // Origin — левый верхний угол (не центр, как у SpriteComponent) для HUD так удобнее раскладывать по экрану.
    void onOwnerMoved(sf::Vector2f newPosition) override;

private:
    // Пересчитывает m_segments/m_segmentLocalOffsets под текущий m_target.getMaxHp() — вынесено из конструктора,
    // потому что maxHp теперь не константа на весь забег (см. HealthComponent::increaseMaxHp, награда за серию
    // убийств в Rogalique/KillStreakComponent): update() зовёт это заново, стоит maxHp измениться, иначе новый
    // сегмент просто не появился бы на полоске до следующего пересоздания HUD.
    void rebuildSegments();

    HealthComponent& m_target;
    std::function<bool()> m_isVisible;
    sf::Vector2f m_size;
    sf::Vector2f m_ownerPosition;

    sf::Texture m_frameTexture;
    sf::Sprite m_frameSprite;
    bool m_hasTexture = false;
    // Заглушка на случай, если текстура не загрузится (см. LOG_WARN в конструкторе).
    sf::RectangleShape m_fallbackBackground;

    std::vector<sf::RectangleShape> m_segments;
    // Позиция сегмента относительно владельца (левый верхний угол полоски) — пересчитывается в rebuildSegments()
    // вместе с самими сегментами, дальше только сдвигается на текущую позицию владельца в onOwnerMoved.
    std::vector<sf::Vector2f> m_segmentLocalOffsets;
    // maxHp на момент последней сборки m_segments — сравнение в update() решает, нужен ли rebuildSegments().
    int m_lastKnownMaxHp = 0;
};
