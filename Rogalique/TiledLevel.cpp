#include "TiledLevel.h"
#include "Log.h"
#include "MiniJson.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace
{
    // Флаги поворота/отражения тайла — старшие 3 бита GID (см. официальную документацию формата Tiled). Не
    // поддержаны (см. TiledLevel.h) — просто отбрасываются, чтобы не мешать резолву тайлсета по firstgid.
    constexpr unsigned FLIP_FLAGS_MASK = 0xE0000000u;
    constexpr unsigned GID_MASK = ~FLIP_FLAGS_MASK;

    // Схлопывает ".."/"." в пути img относительно директории dir (не абсолютный путь — просто join двух
    // относительных путей, ровно то, что нужно для путей картинок внутри .tmj, всегда относительных самому файлу).
    std::string resolveRelativePath(const std::string& dir, const std::string& img)
    {
        std::vector<std::string> parts;
        std::istringstream dirStream(dir);
        std::string segment;
        while (std::getline(dirStream, segment, '/')) {
            if (!segment.empty()) {
                parts.push_back(segment);
            }
        }
        std::istringstream imgStream(img);
        while (std::getline(imgStream, segment, '/')) {
            if (segment.empty() || segment == ".") {
                continue;
            }
            if (segment == "..") {
                if (!parts.empty()) {
                    parts.pop_back();
                }
                continue;
            }
            parts.push_back(segment);
        }
        std::string result;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                result += '/';
            }
            result += parts[i];
        }
        return result;
    }

    // Директория .tmj-файла (для resolveRelativePath) — всё до последнего '/', либо "" если путь без директории.
    std::string dirOf(const std::string& path)
    {
        size_t slash = path.find_last_of('/');
        return slash == std::string::npos ? std::string() : path.substr(0, slash);
    }

    bool readFile(const std::string& path, std::string& out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        out = buffer.str();
        return true;
    }

    // Диапазон одного тайлсета из "tilesets" карты — живёт только на время парсинга одного файла (см. loadTiledLevel),
    // в отличие от старой версии не хранится в TiledLevel: результат резолва (ResolvedTile) записывается в
    // wallTiles/floorTiles/decorTiles сразу при чтении слоёв, сырые GID наружу из этой единицы трансляции не выходят.
    struct TilesetRange {
        int firstGid = 0;
        int tileCount = 0;
        std::string sheetImagePath; // Обычная сетка (image+columns) — пусто для tiles-коллекции, см. ниже.
        int columns = 0;
        int tileWidth = 0;
        int tileHeight = 0;
        std::map<int, std::string> perTileImages; // "Коллекция изображений" — localId -> путь.
        std::map<int, sf::Vector2i> perTileSizes; // localId -> (imagewidth,imageheight).
    };

    ResolvedTile resolveTile(const std::vector<TilesetRange>& tilesets, int gid)
    {
        ResolvedTile result;
        unsigned maskedGid = static_cast<unsigned>(gid) & GID_MASK;
        if (maskedGid == 0) {
            return result; // Пустая клетка — не ошибка, просто !isValid.
        }

        const TilesetRange* range = nullptr;
        for (const TilesetRange& candidate : tilesets) {
            if (static_cast<unsigned>(candidate.firstGid) <= maskedGid) {
                range = &candidate;
            } else {
                break; // tilesets отсортирован по firstGid по возрастанию — дальше только выше maskedGid.
            }
        }
        if (!range) {
            LOG_WARN("TiledLevel: GID " + std::to_string(maskedGid) + " меньше firstgid любого тайлсета");
            return result;
        }

        int localId = static_cast<int>(maskedGid) - range->firstGid;
        if (localId < 0 || localId >= range->tileCount) {
            LOG_WARN("TiledLevel: GID " + std::to_string(maskedGid)
                     + " вне диапазона тайлсета (localId=" + std::to_string(localId) + ")");
            return result;
        }

        if (!range->sheetImagePath.empty()) {
            int col = localId % range->columns;
            int row = localId / range->columns;
            result.texturePath = range->sheetImagePath;
            result.rect = sf::IntRect(col * range->tileWidth, row * range->tileHeight, range->tileWidth, range->tileHeight);
            result.isValid = true;
            return result;
        }

        auto imgIt = range->perTileImages.find(localId);
        if (imgIt == range->perTileImages.end()) {
            LOG_WARN("TiledLevel: у тайла localId=" + std::to_string(localId) + " коллекции нет картинки");
            return result;
        }
        result.texturePath = imgIt->second;
        auto sizeIt = range->perTileSizes.find(localId);
        sf::Vector2i size = sizeIt == range->perTileSizes.end() ? sf::Vector2i(48, 48) : sizeIt->second;
        result.rect = sf::IntRect(0, 0, size.x, size.y);
        result.isValid = true;
        return result;
    }

    std::vector<ResolvedTile> resolveGidArray(const JsonValue& data, const std::vector<TilesetRange>& tilesets)
    {
        std::vector<ResolvedTile> result;
        result.reserve(data.size());
        for (const JsonValue& v : data.items()) {
            result.push_back(resolveTile(tilesets, static_cast<int>(v.asUInt())));
        }
        return result;
    }

    TiledObject parseObject(const JsonValue& obj)
    {
        TiledObject result;
        result.type = obj["type"].asString();
        result.name = obj["name"].asString();
        result.x = obj["x"].asFloat();
        result.y = obj["y"].asFloat();
        result.width = obj["width"].asFloat();
        result.height = obj["height"].asFloat();
        for (const JsonValue& prop : obj["properties"].items()) {
            std::string name = prop["name"].asString();
            std::string propType = prop["type"].asString();
            if (propType == "int" || propType == "float") {
                result.intProps[name] = prop["value"].asInt();
            } else if (propType == "bool") {
                // JsonValue у bool-свойства имеет тип Bool, не String — asString() на нём дал бы пустую строку
                // (тихий fallback, см. MiniJson.h), значение потерялось бы молча. Материализуем явно в "true"/
                // "false", тем же приёмом читают через stringProps (см. FixedItem/requiresInteract в SceneFacade.cpp).
                result.stringProps[name] = prop["value"].asBool() ? "true" : "false";
            } else {
                // string/color/object и т.п. — как строку; DoorComponent-подобным читателям (см. Chest в
                // SceneFacade.cpp) этого достаточно, отдельный тип под color пока не нужен ни одному объекту.
                result.stringProps[name] = prop["value"].asString();
            }
        }
        return result;
    }
} // namespace

bool loadTiledLevel(const std::string& path, TiledLevel& out)
{
    std::string text;
    if (!readFile(path, text)) {
        LOG_ERROR("loadTiledLevel: не удалось открыть файл \"" + path + "\"");
        return false;
    }

    JsonValue root;
    if (!JsonValue::parse(text, root) || root.type() != JsonType::Object) {
        LOG_ERROR("loadTiledLevel: \"" + path + "\" не распарсился как JSON-объект");
        return false;
    }

    TiledLevel result;
    result.widthTiles = root["width"].asInt();
    result.heightTiles = root["height"].asInt();
    result.tileSize = root["tilewidth"].asInt(48);
    if (result.widthTiles <= 0 || result.heightTiles <= 0) {
        LOG_ERROR("loadTiledLevel: \"" + path + "\" — некорректные width/height карты");
        return false;
    }

    std::string baseDir = dirOf(path);

    std::vector<TilesetRange> tilesets;
    for (const JsonValue& tileset : root["tilesets"].items()) {
        TilesetRange range;
        range.firstGid = tileset["firstgid"].asInt();
        range.tileCount = tileset["tilecount"].asInt();
        range.tileWidth = tileset["tilewidth"].asInt(result.tileSize);
        range.tileHeight = tileset["tileheight"].asInt(result.tileSize);

        const JsonValue& perTile = tileset["tiles"];
        if (perTile.type() == JsonType::Array && perTile.size() > 0) {
            for (const JsonValue& tile : perTile.items()) {
                int localId = tile["id"].asInt();
                std::string image = tile["image"].asString();
                if (!image.empty()) {
                    range.perTileImages[localId] = resolveRelativePath(baseDir, image);
                }
                range.perTileSizes[localId]
                    = sf::Vector2i(tile["imagewidth"].asInt(range.tileWidth), tile["imageheight"].asInt(range.tileHeight));
            }
        } else {
            std::string image = tileset["image"].asString();
            range.sheetImagePath = resolveRelativePath(baseDir, image);
            range.columns = tileset["columns"].asInt();
            if (range.columns <= 0) {
                LOG_ERROR("loadTiledLevel: тайлсет \"" + tileset["name"].asString("?") + "\" без columns и без tiles-коллекции");
                return false;
            }
        }
        tilesets.push_back(std::move(range));
    }
    std::sort(
        tilesets.begin(), tilesets.end(), [](const TilesetRange& a, const TilesetRange& b) { return a.firstGid < b.firstGid; });

    for (const JsonValue& layer : root["layers"].items()) {
        std::string layerType = layer["type"].asString();
        std::string layerName = layer["name"].asString();
        if (layerType == "tilelayer") {
            std::vector<ResolvedTile> tiles = resolveGidArray(layer["data"], tilesets);
            if (layerName == "Walls") {
                result.wallTiles = std::move(tiles);
            } else if (layerName == "Floor") {
                result.floorTiles = std::move(tiles);
            } else if (layerName == "Decor") {
                result.decorTiles = std::move(tiles);
            } else {
                LOG_WARN("loadTiledLevel: тайловый слой с неизвестным именем \"" + layerName + "\" пропущен");
            }
        } else if (layerType == "objectgroup") {
            for (const JsonValue& obj : layer["objects"].items()) {
                result.objects.push_back(parseObject(obj));
            }
        }
    }

    size_t expectedCellCount = static_cast<size_t>(result.widthTiles) * static_cast<size_t>(result.heightTiles);
    if (!result.wallTiles.empty() && result.wallTiles.size() != expectedCellCount) {
        LOG_ERROR("loadTiledLevel: слой Walls " + std::to_string(result.wallTiles.size()) + " клеток, ожидалось "
                  + std::to_string(expectedCellCount));
        return false;
    }
    if (!result.floorTiles.empty() && result.floorTiles.size() != expectedCellCount) {
        LOG_ERROR("loadTiledLevel: слой Floor " + std::to_string(result.floorTiles.size()) + " клеток, ожидалось "
                  + std::to_string(expectedCellCount));
        return false;
    }
    if (!result.decorTiles.empty() && result.decorTiles.size() != expectedCellCount) {
        LOG_ERROR("loadTiledLevel: слой Decor " + std::to_string(result.decorTiles.size()) + " клеток, ожидалось "
                  + std::to_string(expectedCellCount));
        return false;
    }

    out = std::move(result);
    return true;
}
