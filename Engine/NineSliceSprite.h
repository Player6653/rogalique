#pragma once
#include "EngineExport.h"
#include <SFML/Graphics.hpp>
#include <string>

// Растягивает текстуру-панель на произвольный размер без искажения.
class ENGINE_API NineSliceSprite {
public:
    // cornerSize толщина фиксированной каймы в пикселях исходной текстуры, одинаковая со всех 4 сторон.
    NineSliceSprite(const std::string& texturePath, int cornerSize);

    bool isLoaded() const
    {
        return m_loaded;
    }

    void setRect(sf::Vector2f position, sf::Vector2f size);
    void draw(sf::RenderWindow& window) const;

private:
    sf::Texture m_texture;
    bool m_loaded = false;
    int m_cornerSize;
    sf::Sprite m_pieces[9];
};
