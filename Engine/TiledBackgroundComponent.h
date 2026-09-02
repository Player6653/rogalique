#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <functional>
#include <string>

// Полноэкранный фон из зацикленного куска текстуры.
class ENGINE_API TiledBackgroundComponent : public IComponent {
public:
    TiledBackgroundComponent(
        sf::Vector2f windowSize, const std::string& texturePath, sf::IntRect tileRect, std::function<bool()> isVisible);

    void draw(sf::RenderWindow& window) const override;

private:
    std::function<bool()> m_isVisible;
    sf::Texture m_texture;
    bool m_hasTexture = false;
    sf::Sprite m_sprite;
};
