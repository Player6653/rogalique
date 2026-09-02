#pragma once
#include "EngineExport.h"
#include <SFML/System/Vector2.hpp>
#include <vector>

// Сетка проходимости комнаты + A* по ней. Клетка = квадрат cellSize мировых пикселей, обычно совпадает с
// TILE_SIZE уровня. Строится один раз при сборке сцены (см. GameWorld::buildNavGrid) сканированием кинематических
// ColliderComponent (стены и т.п.) — не отслеживает препятствия, появляющиеся ПОСЛЕ этого сама сетка статична.
class ENGINE_API NavGrid {
public:
    // origin — мировые координаты левого верхнего угла сетки (клетка [0,0]).
    NavGrid(sf::Vector2f origin, int widthCells, int heightCells, float cellSize);

    // Сканирует GameWorld::instance().getColliders() и помечает клетки, перекрытые кинематическими коллайдерами,
    // непроходимыми. Звать один раз, уже после того как все стены/препятствия добавлены в сцену.
    void rebuildFromColliders();

    bool isWalkableWorld(sf::Vector2f worldPos) const;

    // A* по 8-направленной сетке (диагональ не режет углы — если оба ортогональных соседа заблокированы, по
    // диагонали между ними тоже нельзя). Возвращает точки-центры клеток пути в мировых координатах, БЕЗ стартовой
    // точки; пустой вектор — старт и цель в одной клетке или путь недостижим.
    std::vector<sf::Vector2f> findPath(sf::Vector2f fromWorld, sf::Vector2f toWorld) const;

    // Грубая проверка прямой видимости — сэмплирует отрезок с шагом в половину клетки и требует, чтобы каждая
    // сэмплированная точка была проходима. Не настоящий физический рейкаст (может не заметить очень тонкий угол
    // стены), но при клетке в TILE_SIZE этого достаточно для решения "вижу/не вижу".
    bool hasLineOfSight(sf::Vector2f fromWorld, sf::Vector2f toWorld) const;

private:
    sf::Vector2i worldToCell(sf::Vector2f worldPos) const;
    sf::Vector2f cellToWorld(sf::Vector2i cell) const;
    bool inBounds(int x, int y) const
    {
        return x >= 0 && y >= 0 && x < m_width && y < m_height;
    }
    bool isWalkableCell(int x, int y) const;
    int index(int x, int y) const
    {
        return y * m_width + x;
    }

    sf::Vector2f m_origin;
    int m_width;
    int m_height;
    float m_cellSize;
    std::vector<bool> m_blocked;

    // Буферы A* (см. findPath() в .cpp) — переиспользуются между вызовами вместо переаллокации+обнуления целиком
    // на каждый поиск (раньше так и было: 3 вектора размером во всю карту на каждый вызов — с ростом карты
    // (чанки от игрока, уже сотни клеток на сторону) и числа ботов эта постоянная накладная стоимость сама по
    // себе, независимо от длины реального пути, начала ощутимо бить по FPS в бою). Вместо обнуления всего массива
    // каждый раз — "поколение": ячейка считается нетронутой в ТЕКУЩЕМ поиске, если её m_*Gen не совпадает с
    // m_searchGeneration (который просто увеличивается на 1 в начале каждого findPath()); реальная работа за
    // вызов — O(размер буферов) на выделение один раз в конструкторе плюс O(реально пройденных клеток) на сам
    // поиск, а не O(вся карта) на КАЖДЫЙ вызов. mutable — findPath() концептуально по-прежнему только читает
    // сетку (const для вызывающего кода), эти поля лишь внутренний рабочий кэш между вызовами.
    mutable std::vector<float> m_gScore;
    mutable std::vector<int> m_cameFrom;
    mutable std::vector<int> m_touchedGen;
    mutable std::vector<int> m_closedGen;
    mutable int m_searchGeneration = 0;
};
