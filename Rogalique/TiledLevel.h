#pragma once
#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <vector>

// Разбирает уровень, собранный в Tiled Map Editor (mapeditor.org) и экспортированный как JSON (.tmj, тайлсеты
// встроены в файл — см. Resources/Level/rogalique.tmj и комментарий у loadTiledLevel()). Знает конкретно схему
// Tiled поверх общего MiniJson.h — какие слои/объекты/свойства ожидать, как считать GID тайла в (текстура,
// прямоугольник-вырез). Форма уровня и содержимое расставляются заранее в самом Tiled; SceneFacade.cpp/
// ChunkAssembler.cpp только интерпретируют результат — подробности слоёв/типов объектов см. в комментарии у
// loadTiledLevel().

// Один объект со слоя объектов ("Entities" в rogalique.tmj) — точка (width=height=0) или прямоугольник (Pit).
// x/y — верхний левый угол в мировых пикселях (для чанков — уже со смещением их слота, см. ChunkAssembler.cpp).
struct TiledObject {
    std::string type;
    std::string name;
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;
    std::map<std::string, std::string> stringProps;
    std::map<std::string, int> intProps;
};

// Куда рисовать тайл: файл-исходник + вырез из него в пикселях (см. SpriteComponent::loadTextureRegion).
// flipped* — три старших бита GID (см. официальный формат Tiled), выставляются кнопками "Flip Horizontally/
// Vertically"/"Rotate" в редакторе тайлов Tiled поверх обычного клика по тайлу — сохраняются в самих данных
// слоя, отдельно от того, какой это тайл. См. применение в SceneFacade.cpp::spawnTiledTileAt.
struct ResolvedTile {
    std::string texturePath;
    sf::IntRect rect;
    bool isValid = false;
    bool flippedHorizontally = false;
    bool flippedVertically = false;
    bool flippedDiagonally = false;
};

// wallTiles/floorTiles/decorTiles — уже РЕЗОЛВЛЕНЫ (текстура+вырез, не сырой GID) прямо при загрузке файла, а не
// лениво по требованию, как раньше. Причина: GID — это номер тайла ВНУТРИ ОДНОГО файла (у каждого файла своя
// нумерация тайлсетов, с 1) — при склейке нескольких чанков в один уровень (см. ChunkAssembler.cpp) сырые GID
// разных файлов бессмысленно сравнивать/копировать напрямую, а вот уже резолвленные (текстура, прямоугольник)
// корректно копируются между уровнями как есть, независимо от того, из какого файла они пришли.
struct TiledLevel {
    int widthTiles = 0;
    int heightTiles = 0;
    int tileSize = 48;

    // По одной записи на клетку, widthTiles*heightTiles каждый, индекс = y*widthTiles+x; !isValid — клетка пуста.
    // Слоёв с такими именами в файле может не быть вовсе (тогда вектор пуст) — не ошибка, просто нечего рисовать.
    std::vector<ResolvedTile> wallTiles;
    std::vector<ResolvedTile> floorTiles;
    std::vector<ResolvedTile> decorTiles;

    std::vector<TiledObject> objects; // Все объекты со всех object-слоёв разом, порядок как в файле.
};

// false + LOG_ERROR при любой проблеме (файл не найден, не парсится, схема не похожа на ожидаемую) — вызывающий
// код сам решает, что делать (SceneFacade::run()/ChunkAssembler.cpp — не могут продолжить: без уровня/чанка
// строить сцену нечего).
bool loadTiledLevel(const std::string& path, TiledLevel& out);
