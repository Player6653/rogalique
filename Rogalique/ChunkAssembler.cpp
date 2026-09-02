#include "ChunkAssembler.h"
#include "Log.h"
#include <algorithm>
#include <deque>
#include <random>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace
{
    // Взято как есть из существующих ассетов уровня — гарантированно на диске (см. Rogalique/Wall.cpp в старой
    // версии/Resources/Map/Tiles), запасной тайл для "заделки" неиспользованных проходов чанков и для пола под
    // прорезаемыми коридорами между чанками (у самих коридоров нет собственного тайлсета — они не часть ни
    // одного .tmj, их рисует сборщик).
    ResolvedTile fallbackWallTile()
    {
        ResolvedTile tile;
        tile.texturePath = "Resources/Map/Tiles/wall.png";
        tile.rect = sf::IntRect(0, 0, 48, 48);
        tile.isValid = true;
        return tile;
    }

    ResolvedTile fallbackFloorTile()
    {
        ResolvedTile tile;
        tile.texturePath = "Resources/Map/Tiles/floor.png";
        tile.rect = sf::IntRect(0, 0, 48, 48);
        tile.isValid = true;
        return tile;
    }

    // Стена "в стиле" конкретного куска (хаба или чанка пула) — первый валидный тайл из его собственного слоя
    // Walls. Раньше заделка/боковые стены коридора везде рисовались одним и тем же fallbackWallTile() (Tiles/
    // wall.png) независимо от того, каким тайлсетом на самом деле нарисованы стены этого куска (у room3, например,
    // не тот тайлсет, что у Tiles/wall.png, заделка выглядела чужеродно). fallbackWallTile() остаётся
    // только на случай пустого/некорректного чанка без единого валидного тайла в Walls вовсе.
    ResolvedTile pieceWallTile(const TiledLevel& level)
    {
        for (const ResolvedTile& tile : level.wallTiles) {
            if (tile.isValid) {
                return tile;
            }
        }
        return fallbackWallTile();
    }

    enum class Direction { North, South, East, West };
    constexpr Direction ALL_DIRECTIONS[] = {Direction::North, Direction::South, Direction::East, Direction::West};

    sf::Vector2i directionVector(Direction dir)
    {
        switch (dir) {
        case Direction::North:
            return sf::Vector2i(0, -1);
        case Direction::South:
            return sf::Vector2i(0, 1);
        case Direction::East:
            return sf::Vector2i(1, 0);
        case Direction::West:
            return sf::Vector2i(-1, 0);
        }
        return sf::Vector2i(0, 0);
    }

    // Один уже размещённый в общих координатах кусок (хаб или чанк пула) — offsetXTiles/offsetYTiles одного
    // предварительного (до финального сдвига к (0,0), см. assembleChunkedLevel) координатного пространства.
    struct PlacedPiece {
        const TiledLevel* level;
        int offsetXTiles;
        int offsetYTiles;
        // Какие из 4 сторон реально соединены коридором с соседним куском — остальные сторона сборщик заделает
        // стеной (см. sealUnconnectedEdges), если только это не хаб (хаб не пул-чанк, не трогаем его собственные
        // непройденные стороны — та же логика, что раньше просто оставляла их как нарисовал игрок).
        bool connected[4] = {false, false, false, false}; // Индекс — см. ALL_DIRECTIONS.
    };

    int directionIndex(Direction dir)
    {
        for (int i = 0; i < 4; ++i) {
            if (ALL_DIRECTIONS[i] == dir) {
                return i;
            }
        }
        return -1;
    }

    // Индекс клетки в итоговой (уже сдвинутой к (0,0)) сетке ширины width.
    int cellIndex(int x, int y, int width)
    {
        return y * width + x;
    }

    void blitPiece(TiledLevel& dest, const PlacedPiece& piece, int shiftX, int shiftY)
    {
        const TiledLevel& src = *piece.level;
        int destOffsetX = piece.offsetXTiles + shiftX;
        int destOffsetY = piece.offsetYTiles + shiftY;
        for (int y = 0; y < src.heightTiles; ++y) {
            for (int x = 0; x < src.widthTiles; ++x) {
                int srcIndex = cellIndex(x, y, src.widthTiles);
                int dx = destOffsetX + x;
                int dy = destOffsetY + y;
                if (dx < 0 || dy < 0 || dx >= dest.widthTiles || dy >= dest.heightTiles) {
                    continue; // Не должно случаться при корректном расчёте канвы ниже, но не падать, если вдруг.
                }
                int destIndex = cellIndex(dx, dy, dest.widthTiles);
                if (!src.wallTiles.empty()) {
                    dest.wallTiles[destIndex] = src.wallTiles[srcIndex];
                }
                if (!src.floorTiles.empty()) {
                    dest.floorTiles[destIndex] = src.floorTiles[srcIndex];
                }
                if (!src.decorTiles.empty()) {
                    dest.decorTiles[destIndex] = src.decorTiles[srcIndex];
                }
            }
        }
        for (const TiledObject& obj : src.objects) {
            TiledObject shifted = obj;
            shifted.x += (destOffsetX) * static_cast<float>(dest.tileSize);
            shifted.y += (destOffsetY) * static_cast<float>(dest.tileSize);
            dest.objects.push_back(std::move(shifted));
        }
    }

    // Прорезает коридор шириной config.corridorWidth между двумя гранями, обращёнными друг к другу вдоль dir —
    // пол по всей длине зазора, плюс стены по бокам (см. ниже, sideWallTile — см. pieceWallTile(), в стиле того
    // куска, К КОТОРОМУ прокладывается коридор) на всю эту же длину. Гарантированно кладёт floor/убирает wall
    // независимо от того, что было в этой полосе раньше (там либо пусто — щель между кусками, — либо край
    // чанка, который мы намеренно перекрываем, чтобы проход не зависел от того, насколько точно игрок попал в
    // конвенцию "проём по центру").
    void carveCorridor(TiledLevel& dest, int fromRightEdgeOrBottomX, int toLeftEdgeOrTopX, int centerPerp, Direction dir,
        int corridorWidth, int shiftX, int shiftY, const ResolvedTile& sideWallTile)
    {
        auto placeWallIfInBounds = [&dest, &sideWallTile](int x, int y) {
            if (x < 0 || y < 0 || x >= dest.widthTiles || y >= dest.heightTiles) {
                return;
            }
            dest.wallTiles[cellIndex(x, y, dest.widthTiles)] = sideWallTile;
        };

        int half = corridorWidth / 2;
        if (dir == Direction::East || dir == Direction::West) {
            int x0 = std::min(fromRightEdgeOrBottomX, toLeftEdgeOrTopX) + shiftX;
            int x1 = std::max(fromRightEdgeOrBottomX, toLeftEdgeOrTopX) + shiftX;
            int y0 = centerPerp + shiftY - half;
            int y1 = y0 + corridorWidth;
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    if (x < 0 || y < 0 || x >= dest.widthTiles || y >= dest.heightTiles) {
                        continue;
                    }
                    int idx = cellIndex(x, y, dest.widthTiles);
                    dest.wallTiles[idx] = ResolvedTile();
                    dest.floorTiles[idx] = fallbackFloorTile();
                }
            }
            // Боковые стены зазора — иначе за пределами связанных дверей коридор был бы открыт в пустоту сбоку
            // (пустые, ничем не занятые клетки канвы вне самих кусков), и игрок мог бы выйти сквозь них за пределы
            // уровня. Ряд НАД (y0-1) и ряд ПОД (y1) полосой прохода, на всю его длину по x.
            for (int x = x0; x < x1; ++x) {
                placeWallIfInBounds(x, y0 - 1);
                placeWallIfInBounds(x, y1);
            }
        } else {
            int y0 = std::min(fromRightEdgeOrBottomX, toLeftEdgeOrTopX) + shiftY;
            int y1 = std::max(fromRightEdgeOrBottomX, toLeftEdgeOrTopX) + shiftY;
            int x0 = centerPerp + shiftX - half;
            int x1 = x0 + corridorWidth;
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    if (x < 0 || y < 0 || x >= dest.widthTiles || y >= dest.heightTiles) {
                        continue;
                    }
                    int idx = cellIndex(x, y, dest.widthTiles);
                    dest.wallTiles[idx] = ResolvedTile();
                    dest.floorTiles[idx] = fallbackFloorTile();
                }
            }
            // Симметрично — столбец СЛЕВА (x0-1) и столбец СПРАВА (x1) от полосы прохода, на всю его длину по y.
            for (int y = y0; y < y1; ++y) {
                placeWallIfInBounds(x0 - 1, y);
                placeWallIfInBounds(x1, y);
            }
        }
    }

    // Заделывает стеной фиксированную центральную полосу (corridorWidth) на стороне dir куска piece — вызывается
    // только для НЕ соединённых сторон, см. connected[] у PlacedPiece. Тот же принцип "центр стороны", что и сама
    // конвенция для проходов чанков (см. ChunkAssembler.h) — сборщик режет/заделывает ровно там же, где по
    // конвенции игрок обязан был оставить проём.
    void sealEdge(TiledLevel& dest, const PlacedPiece& piece, Direction dir, int corridorWidth, int shiftX, int shiftY)
    {
        ResolvedTile wallTile = pieceWallTile(*piece.level);
        int half = corridorWidth / 2;
        int offsetX = piece.offsetXTiles + shiftX;
        int offsetY = piece.offsetYTiles + shiftY;
        int w = piece.level->widthTiles;
        int h = piece.level->heightTiles;
        int centerX = offsetX + w / 2;
        int centerY = offsetY + h / 2;

        int x0, x1, y0, y1;
        if (dir == Direction::North) {
            x0 = centerX - half;
            x1 = x0 + corridorWidth;
            y0 = offsetY;
            y1 = offsetY + 1;
        } else if (dir == Direction::South) {
            x0 = centerX - half;
            x1 = x0 + corridorWidth;
            y0 = offsetY + h - 1;
            y1 = offsetY + h;
        } else if (dir == Direction::East) {
            y0 = centerY - half;
            y1 = y0 + corridorWidth;
            x0 = offsetX + w - 1;
            x1 = offsetX + w;
        } else {
            y0 = centerY - half;
            y1 = y0 + corridorWidth;
            x0 = offsetX;
            x1 = offsetX + 1;
        }
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                if (x < 0 || y < 0 || x >= dest.widthTiles || y >= dest.heightTiles) {
                    continue;
                }
                dest.wallTiles[cellIndex(x, y, dest.widthTiles)] = wallTile;
            }
        }
    }
} // namespace

bool assembleChunkedLevel(const ChunkAssemblerConfig& config, unsigned seed, TiledLevel& out)
{
    TiledLevel hub;
    if (!loadTiledLevel(config.hubPath, hub)) {
        LOG_ERROR("assembleChunkedLevel: не удалось загрузить хаб \"" + config.hubPath + "\"");
        return false;
    }

    // Перебор *.tmj в poolDir через WinAPI, а не std::filesystem — тот не подключается без правки языкового
    // стандарта проекта целиком (см. чат), а ради одной папки это лишний риск для остального кода.
    std::vector<std::string> poolFiles;
    {
        std::string searchPattern = config.poolDir + "\\*.tmj";
        WIN32_FIND_DATAA findData;
        HANDLE findHandle = FindFirstFileA(searchPattern.c_str(), &findData);
        if (findHandle != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    poolFiles.push_back(config.poolDir + "/" + findData.cFileName);
                }
            } while (FindNextFileA(findHandle, &findData));
            FindClose(findHandle);
        }
    }
    if (poolFiles.empty() && config.chainLength > 0) {
        LOG_ERROR("assembleChunkedLevel: папка чанков \"" + config.poolDir + "\" пуста или не найдена, а chainLength > 0");
        return false;
    }

    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> poolDist(0, poolFiles.empty() ? 0 : poolFiles.size() - 1);

    // Все загруженные TiledLevel живут здесь до конца функции — PlacedPiece::level указывает внутрь этого
    // контейнера, deque не инвалидирует указатели на существующие элементы при push_back (в отличие от vector).
    std::deque<TiledLevel> loadedChunks;
    std::vector<PlacedPiece> placed;

    PlacedPiece hubPiece;
    hubPiece.level = &hub;
    hubPiece.offsetXTiles = 0;
    hubPiece.offsetYTiles = 0;

    for (Direction dir : ALL_DIRECTIONS) {
        sf::Vector2i step = directionVector(dir);
        // Текущая "точка стыковки" — где заканчивается предыдущий кусок цепочки (изначально хаб).
        int prevOffsetX = 0, prevOffsetY = 0, prevW = hub.widthTiles, prevH = hub.heightTiles;
        size_t prevPieceIndexInPlaced = static_cast<size_t>(-1); // -1 == хаб, ещё не в placed.

        for (int i = 0; i < config.chainLength; ++i) {
            int attempts = 0;
            const TiledLevel* chunkLevel = nullptr;

            // Последнее звено цепочки в эту сторону — если для неё задан keyRoomPaths (см. .h), берём ИМЕННО
            // этот файл, не случайный из пула, чтобы содержимое комнаты (обычно ключ) гарантированно попадало на
            // карту каждый забег.
            bool isLastSlot = (i == config.chainLength - 1);
            const std::string& forcedPath = config.keyRoomPaths[directionIndex(dir)];
            if (isLastSlot && !forcedPath.empty()) {
                TiledLevel candidate;
                if (loadTiledLevel(forcedPath, candidate) && candidate.widthTiles == config.poolChunkSizeTiles
                    && candidate.heightTiles == config.poolChunkSizeTiles) {
                    loadedChunks.push_back(std::move(candidate));
                    chunkLevel = &loadedChunks.back();
                } else {
                    LOG_ERROR("assembleChunkedLevel: не удалось загрузить принудительную комнату \"" + forcedPath
                              + "\" (направление " + std::to_string(directionIndex(dir)) + ") — падаю обратно на случайный чанк");
                }
            }

            while (!chunkLevel && attempts < 8 && !poolFiles.empty()) {
                ++attempts;
                const std::string& path = poolFiles[poolDist(rng)];
                TiledLevel candidate;
                if (!loadTiledLevel(path, candidate)) {
                    continue;
                }
                if (candidate.widthTiles != config.poolChunkSizeTiles || candidate.heightTiles != config.poolChunkSizeTiles) {
                    LOG_WARN("assembleChunkedLevel: чанк \"" + path + "\" размера " + std::to_string(candidate.widthTiles) + "x"
                             + std::to_string(candidate.heightTiles) + ", ожидался " + std::to_string(config.poolChunkSizeTiles)
                             + "x" + std::to_string(config.poolChunkSizeTiles) + " — пропущен");
                    continue;
                }
                loadedChunks.push_back(std::move(candidate));
                chunkLevel = &loadedChunks.back();
                break;
            }
            if (!chunkLevel) {
                break; // Не нашли подходящий чанк за разумное число попыток — обрываем цепочку в эту сторону здесь.
            }

            int w = chunkLevel->widthTiles;
            int h = chunkLevel->heightTiles;
            int offsetX, offsetY;
            if (dir == Direction::East) {
                offsetX = prevOffsetX + prevW + config.corridorLength;
                offsetY = prevOffsetY + prevH / 2 - h / 2;
            } else if (dir == Direction::West) {
                offsetX = prevOffsetX - config.corridorLength - w;
                offsetY = prevOffsetY + prevH / 2 - h / 2;
            } else if (dir == Direction::South) {
                offsetY = prevOffsetY + prevH + config.corridorLength;
                offsetX = prevOffsetX + prevW / 2 - w / 2;
            } else {
                offsetY = prevOffsetY - config.corridorLength - h;
                offsetX = prevOffsetX + prevW / 2 - w / 2;
            }

            PlacedPiece piece;
            piece.level = chunkLevel;
            piece.offsetXTiles = offsetX;
            piece.offsetYTiles = offsetY;
            // Сторона, обращённая НАЗАД к предыдущему куску цепочки (к хабу или предыдущему чанку) — всегда
            // считается соединённой (мы только что туда прорезали коридор, см. ниже после цикла).
            Direction backDir = dir == Direction::East    ? Direction::West
                                : dir == Direction::West  ? Direction::East
                                : dir == Direction::South ? Direction::North
                                                          : Direction::South;
            piece.connected[directionIndex(backDir)] = true;
            placed.push_back(piece);
            size_t thisPieceIndex = placed.size() - 1;

            // Отмечаем у ПРЕДЫДУЩЕГО куска (хаб или чанк) сторону dir как соединённую — теперь известно, что там
            // действительно есть сосед, эту сторону заделывать не нужно.
            if (prevPieceIndexInPlaced == static_cast<size_t>(-1)) {
                hubPiece.connected[directionIndex(dir)] = true;
            } else {
                placed[prevPieceIndexInPlaced].connected[directionIndex(dir)] = true;
            }

            prevOffsetX = offsetX;
            prevOffsetY = offsetY;
            prevW = w;
            prevH = h;
            prevPieceIndexInPlaced = thisPieceIndex;
        }
    }

    // Итоговая канва — ограничивающий прямоугольник хаба и всех расставленных чанков, плюс запас в 1 тайл со всех
    // сторон (на случай, если заделка края придётся ровно на границу канвы).
    int minX = 0, minY = 0, maxX = hub.widthTiles, maxY = hub.heightTiles;
    for (const PlacedPiece& piece : placed) {
        minX = std::min(minX, piece.offsetXTiles);
        minY = std::min(minY, piece.offsetYTiles);
        maxX = std::max(maxX, piece.offsetXTiles + piece.level->widthTiles);
        maxY = std::max(maxY, piece.offsetYTiles + piece.level->heightTiles);
    }
    constexpr int MARGIN = 1;
    int shiftX = -minX + MARGIN;
    int shiftY = -minY + MARGIN;

    TiledLevel result;
    result.widthTiles = (maxX - minX) + MARGIN * 2;
    result.heightTiles = (maxY - minY) + MARGIN * 2;
    result.tileSize = hub.tileSize;
    size_t cellCount = static_cast<size_t>(result.widthTiles) * static_cast<size_t>(result.heightTiles);
    result.wallTiles.resize(cellCount);
    result.floorTiles.resize(cellCount);
    result.decorTiles.resize(cellCount);

    blitPiece(result, hubPiece, shiftX, shiftY);
    for (const PlacedPiece& piece : placed) {
        blitPiece(result, piece, shiftX, shiftY);
    }

    // Коридоры между хабом/чанками — отдельный проход, повторяющий ту же арифметику offset'ов, что и в основном
    // цикле расстановки выше (проще и надёжнее, чем хранить связи между кусками отдельным списком).
    for (Direction dir : ALL_DIRECTIONS) {
        int prevOffsetX = 0, prevOffsetY = 0, prevW = hub.widthTiles, prevH = hub.heightTiles;
        for (const PlacedPiece& piece : placed) {
            // Пропускаем куски не из текущей цепочки: у куска этой цепочки offset вдоль step монотонно растёт от
            // хаба, а перпендикулярная координата центра совпадает с prev — проверяем именно это.
            sf::Vector2i step = directionVector(dir);
            bool matchesChain;
            if (step.x != 0) {
                matchesChain
                    = (piece.offsetYTiles + piece.level->heightTiles / 2 == prevOffsetY + prevH / 2)
                      && ((step.x > 0 && piece.offsetXTiles == prevOffsetX + prevW + config.corridorLength)
                          || (step.x < 0 && piece.offsetXTiles == prevOffsetX - config.corridorLength - piece.level->widthTiles));
            } else {
                matchesChain = (piece.offsetXTiles + piece.level->widthTiles / 2 == prevOffsetX + prevW / 2)
                               && ((step.y > 0 && piece.offsetYTiles == prevOffsetY + prevH + config.corridorLength)
                                   || (step.y < 0
                                       && piece.offsetYTiles == prevOffsetY - config.corridorLength - piece.level->heightTiles));
            }
            if (!matchesChain) {
                continue;
            }
            ResolvedTile sideWallTile = pieceWallTile(*piece.level);
            if (step.x > 0) {
                carveCorridor(result, prevOffsetX + prevW, piece.offsetXTiles, prevOffsetY + prevH / 2, dir, config.corridorWidth,
                    shiftX, shiftY, sideWallTile);
            } else if (step.x < 0) {
                carveCorridor(result, prevOffsetX, piece.offsetXTiles + piece.level->widthTiles, prevOffsetY + prevH / 2, dir,
                    config.corridorWidth, shiftX, shiftY, sideWallTile);
            } else if (step.y > 0) {
                carveCorridor(result, prevOffsetY + prevH, piece.offsetYTiles, prevOffsetX + prevW / 2, dir, config.corridorWidth,
                    shiftX, shiftY, sideWallTile);
            } else {
                carveCorridor(result, prevOffsetY, piece.offsetYTiles + piece.level->heightTiles, prevOffsetX + prevW / 2, dir,
                    config.corridorWidth, shiftX, shiftY, sideWallTile);
            }
            prevOffsetX = piece.offsetXTiles;
            prevOffsetY = piece.offsetYTiles;
            prevW = piece.level->widthTiles;
            prevH = piece.level->heightTiles;
        }
    }

    // Заделка неиспользованных сторон — хаб трогаем только по тем направлениям, где цепочка вообще не
    // выросла (chainLength==0 либо не нашлось подходящего чанка с первой же попытки цепочки).
    for (Direction dir : ALL_DIRECTIONS) {
        if (!hubPiece.connected[directionIndex(dir)]) {
            sealEdge(result, hubPiece, dir, config.corridorWidth, shiftX, shiftY);
        }
    }
    for (const PlacedPiece& piece : placed) {
        for (Direction dir : ALL_DIRECTIONS) {
            if (!piece.connected[directionIndex(dir)]) {
                sealEdge(result, piece, dir, config.corridorWidth, shiftX, shiftY);
            }
        }
    }

    out = std::move(result);
    LOG_INFO("assembleChunkedLevel: собрано " + std::to_string(placed.size()) + " чанков вокруг хаба, канва "
             + std::to_string(out.widthTiles) + "x" + std::to_string(out.heightTiles));
    return true;
}
