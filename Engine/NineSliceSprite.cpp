#include "pch.h"
#include "NineSliceSprite.h"
#include "Log.h"
#include <algorithm>

NineSliceSprite::NineSliceSprite(const std::string& texturePath, int cornerSize)
    : m_cornerSize(cornerSize)
{
    m_loaded = m_texture.loadFromFile(texturePath);
    if (!m_loaded) {
        LOG_WARN("NineSliceSprite: не удалось загрузить текстуру \"" + texturePath + "\"");
        return;
    }
    for (sf::Sprite& piece : m_pieces) {
        piece.setTexture(m_texture);
    }
}

void NineSliceSprite::setRect(sf::Vector2f position, sf::Vector2f size)
{
    if (!m_loaded) {
        return;
    }

    int textureWidth = static_cast<int>(m_texture.getSize().x);
    int textureHeight = static_cast<int>(m_texture.getSize().y);
    int corner = m_cornerSize;
    int centerTextureWidth = std::max(1, textureWidth - corner * 2);
    int centerTextureHeight = std::max(1, textureHeight - corner * 2);

    // Середина панели тянется, кайма — никогда: иначе орнамент по краям текстуры расползся бы вместе с фоном.
    float centerWidth = std::max(0.f, size.x - corner * 2.f);
    float centerHeight = std::max(0.f, size.y - corner * 2.f);

    auto place = [&](int index, int rectX, int rectY, int rectW, int rectH, float x, float y, float scaleX, float scaleY) {
        m_pieces[index].setTextureRect(sf::IntRect(rectX, rectY, rectW, rectH));
        m_pieces[index].setPosition(position.x + x, position.y + y);
        m_pieces[index].setScale(scaleX, scaleY);
    };

    // Раскладка 0..8 — три ряда по три куска: углы (0,2,6,8) без растяжения, грани (1,3,5,7) растянуты по одной оси, центр (4) растянут по обеим.
    place(0, 0, 0, corner, corner, 0.f, 0.f, 1.f, 1.f);
    place(1, corner, 0, centerTextureWidth, corner, (float)corner, 0.f, centerWidth / centerTextureWidth, 1.f);
    place(2, textureWidth - corner, 0, corner, corner, corner + centerWidth, 0.f, 1.f, 1.f);

    place(3, 0, corner, corner, centerTextureHeight, 0.f, (float)corner, 1.f, centerHeight / centerTextureHeight);
    place(4, corner, corner, centerTextureWidth, centerTextureHeight, (float)corner, (float)corner,
        centerWidth / centerTextureWidth, centerHeight / centerTextureHeight);
    place(5, textureWidth - corner, corner, corner, centerTextureHeight, corner + centerWidth, (float)corner, 1.f,
        centerHeight / centerTextureHeight);

    place(6, 0, textureHeight - corner, corner, corner, 0.f, corner + centerHeight, 1.f, 1.f);
    place(7, corner, textureHeight - corner, centerTextureWidth, corner, (float)corner, corner + centerHeight,
        centerWidth / centerTextureWidth, 1.f);
    place(
        8, textureWidth - corner, textureHeight - corner, corner, corner, corner + centerWidth, corner + centerHeight, 1.f, 1.f);
}

void NineSliceSprite::draw(sf::RenderWindow& window) const
{
    if (!m_loaded) {
        return;
    }
    for (const sf::Sprite& piece : m_pieces) {
        window.draw(piece);
    }
}
