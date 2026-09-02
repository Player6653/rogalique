#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <string>

// Стратегия отображения (рендера), рисует настоящую текстуру, если она загружена, иначе цветную заглушку-прямоугольник, чтобы объект был виден ещё до появления арта.
class ENGINE_API SpriteComponent : public IComponent {
public:
    explicit SpriteComponent(sf::Vector2f size);

    void setPlaceholderColor(sf::Color color);

    // Отражает спрайт по горизонтали — нужно паку с одним нарисованным направлением, а также тайлам с флагом
    // Flip Horizontally из Tiled (см. TiledLevel.h/ResolvedTile::flippedHorizontally и
    // SceneFacade.cpp::spawnTiledTileAt).
    void setFlippedX(bool flipped);
    // Отражает спрайт по вертикали — нужен тайлам с флагом Flip Vertically из Tiled (см. TiledLevel.h/
    // ResolvedTile::flippedVertically и SceneFacade.cpp::spawnTiledTileAt).
    void setFlippedY(bool flipped);
    // Поворот в градусах — снарядам (Projectile) развернуть спрайт по направлению полёта, а также тайлам с
    // диагональным флагом (Rotate) из Tiled, см. SceneFacade.cpp::spawnTiledTileAt.
    void setRotation(float degrees);
    void setColor(sf::Color color);
    void clearColor();

    // Сдвигает спрайт относительно позиции владельца (по умолчанию (0,0) — ровно на владельце, как раньше). Нужен
    // существам без своей текстуры тени (см. Boss.cpp/VampireSpawnMinion.cpp) — плейсхолдер-пятно тени рисуют не в
    // центре персонажа (там читается как полоса на груди), а смещённым вниз, ближе к ногам.
    void setPositionOffset(sf::Vector2f offset)
    {
        m_positionOffset = offset;
        m_placeholder.setPosition(m_ownerPosition + m_positionOffset);
        m_sprite.setPosition(m_ownerPosition + m_positionOffset);
    }

    // true при успехе; при неудаче заглушка остаётся видимой. Статичный кадр вся текстура целиком.
    bool loadTexture(const std::string& path);

    // Как loadTexture(), но берёт не всю картинку, а один прямоугольный вырез из неё (в пикселях исходного файла)
    // — без анимации. Нужен для тайлов, вырезаемых из общего листа тайлсета "на лету" по данным Tiled-карты
    // (см. Rogalique/TiledLevel.h), вместо того чтобы заранее вручную сохранять каждый тайл отдельным PNG-файлом.
    // repeat=true — rect может быть БОЛЬШЕ самой текстуры, тогда картинка не растягивается/обрезается, а
    // замощается повтором (см. sf::Texture::setRepeated) — на случай, когда одной большой заливкой дешевле для
    // сцены, чем тайл-объект на каждую клетку (сейчас такого использования в игре нет — и подземелье, и арена
    // рисуются по клетке через spawnTiledTileAt, см. SceneFacade.cpp).
    bool loadTextureRegion(const std::string& path, sf::IntRect rect, bool repeat = false);

    // Полоса из frameCount одинаковых кадров подряд по горизонтали. rowCount>1 — лист поделён на несколько таких
    // полос по вертикали (например, по направлениям), row выбирает нужную (0 — самая верхняя).
    // preserveAspect=true — не растягивать кадр по осям независимо (как обычно, см. m_scaleY), а вписать с
    // сохранением исходных пропорций кадра в size, одним общим множителем (нужно для иконок предметов — их
    // кадр не обязательно квадратный, см. ItemPickup.cpp).
    bool loadAnimation(const std::string& path, int frameCount, sf::Time frameDuration, bool loop = true, int row = 0,
        int rowCount = 1, bool preserveAspect = false);

    void update(sf::Time dt) override;
    void draw(sf::RenderWindow& window) const override;
    void onOwnerMoved(sf::Vector2f newPosition) override;

private:
    void applyFrame(int frameIndex);
    // Общий кэш-лукап для loadAnimation()/loadTextureRegion() — nullptr, если файл не удалось загрузить (кэш сам
    // логирует и очищает неудачную запись, см. .cpp).
    static const sf::Texture* getCachedTexture(const std::string& path);

    sf::Vector2f m_size;
    sf::RectangleShape m_placeholder;
    sf::Color m_placeholderBaseColor = sf::Color::White;
    // Невладеющий — указывает на запись в общем кэше текстур (см. .cpp), а не на собственную копию: одна и та же
    // картинка на диске (например, тайл пола) не должна грузиться и жить в видеопамяти по разу на каждый объект,
    // которых на большой карте могут быть тысячи.
    const sf::Texture* m_texture = nullptr;
    sf::Sprite m_sprite;
    bool m_hasTexture = false;

    int m_frameWidth = 0;
    int m_frameHeight = 0;
    int m_row = 0;
    int m_frameCount = 1;
    sf::Time m_frameDuration;
    sf::Time m_frameElapsed;
    int m_currentFrame = 0;
    bool m_loop = true;

    // Модуль масштаба по X/Y отдельно — большинство спрайтов в игре квадратные или size уже подобран под
    // пропорции исходной картинки, так что раньше хватало одного числа на обе оси (сохранял пропорции). Отдельная
    // m_scaleY понадобилась объектам, где size сознательно НЕ повторяет пропорции файла (например, Pit — узкая
    // растянутая полоса лавы поверх картинки лавовой лужи, см. Rogalique/Pit.cpp): без независимого Y один и тот
    // же множитель раздувал бы высоту на ту же величину, что и ширину, и объект оказывался в разы выше заданного.
    float m_scale = 1.f;
    float m_scaleY = 1.f;
    bool m_flippedX = false;
    bool m_flippedY = false;

    sf::Vector2f m_ownerPosition;
    sf::Vector2f m_positionOffset;
};
