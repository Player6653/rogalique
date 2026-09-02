#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

// HUD-значок: иконка + текущее число из getValue(), зовётся каждый кадр (значение может меняться на лету, например
// суммарная прочность надетой брони — см. InventoryComponent::getTotalDurability() в Rogalique). В отличие от
// HealthBarComponent — не полоска с делениями/долей, а простой статичный бейдж: то же самое число, но без
// визуальной метафоры "заполненности", которую сложно сделать интуитивной для ресурса без фиксированного максимума.
class ENGINE_API ArmorBadgeComponent : public IComponent {
public:
    ArmorBadgeComponent(std::function<int()> getValue, const std::string& iconTexturePath, const std::string& fontPath,
        std::function<bool()> isVisible = nullptr);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;
    // Origin — левый верхний угол, как у HealthBarComponent, для удобной раскладки по HUD.
    void onOwnerMoved(sf::Vector2f newPosition) override;

private:
    std::function<int()> m_getValue;
    std::function<bool()> m_isVisible;

    sf::Texture m_iconTexture;
    sf::Sprite m_iconSprite;
    bool m_hasIcon = false;

    sf::Font m_font;
    bool m_hasFont = false;
    sf::Text m_valueText;
};
