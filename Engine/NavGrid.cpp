#include "pch.h"
#include "NavGrid.h"
#include "ColliderComponent.h"
#include "GameWorld.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace
{
    struct NodeRecord {
        float f;
        int index;
        bool operator>(const NodeRecord& other) const
        {
            return f > other.f;
        }
    };

    constexpr float SQRT2 = 1.41421356f;

    // Потолок числа развёрнутых узлов за один findPath() — без него недостижимая (или просто очень далёкая, через
    // полкарты) цель заставляет A* перебрать почти всю сетку целиком (карта уже выросла до сотен клеток на
    // сторону, а ботов — десятки), это ощутимо бьёт по FPS в бою (см. чат с игроком). 1500 узлов с большим запасом
    // хватает на любой реалистичный путь внутри одной цепочки чанков; если не уложились — считаем недостижимым,
    // тот же fallback, что и для честно недостижимой цели (см. moveTowardGoal в ChaseComponent.cpp).
    constexpr int MAX_EXPANDED_NODES = 1500;

    float octileHeuristic(int dx, int dy)
    {
        int adx = std::abs(dx);
        int ady = std::abs(dy);
        return (adx > ady) ? static_cast<float>(adx - ady) + ady * SQRT2 : static_cast<float>(ady - adx) + adx * SQRT2;
    }
} // namespace

NavGrid::NavGrid(sf::Vector2f origin, int widthCells, int heightCells, float cellSize)
    : m_origin(origin),
      m_width(widthCells),
      m_height(heightCells),
      m_cellSize(cellSize),
      m_blocked(static_cast<std::size_t>(widthCells) * static_cast<std::size_t>(heightCells), false)
{
    // Буферы A* (см. .h) — выделяются один раз здесь, не на каждый findPath(). m_touchedGen/m_closedGen начинают
    // с 0, а m_searchGeneration с 0 тоже — первый же findPath() увеличит его до 1 перед использованием, так что
    // 0 всегда означает "не тронуто ни в одном настоящем поиске", коллизии с начальным состоянием нет.
    std::size_t cellCount = static_cast<std::size_t>(widthCells) * static_cast<std::size_t>(heightCells);
    m_gScore.resize(cellCount);
    m_cameFrom.resize(cellCount);
    m_touchedGen.assign(cellCount, 0);
    m_closedGen.assign(cellCount, 0);
}

void NavGrid::rebuildFromColliders()
{
    std::fill(m_blocked.begin(), m_blocked.end(), false);

    for (ColliderComponent* collider : GameWorld::instance().getColliders()) {
        if (!collider->isKinematic()) {
            continue;
        }
        sf::FloatRect bounds = collider->getBounds();
        sf::Vector2i minCell = worldToCell(sf::Vector2f(bounds.left, bounds.top));
        sf::Vector2i maxCell = worldToCell(sf::Vector2f(bounds.left + bounds.width, bounds.top + bounds.height));
        for (int y = std::max(0, minCell.y); y <= std::min(m_height - 1, maxCell.y); ++y) {
            for (int x = std::max(0, minCell.x); x <= std::min(m_width - 1, maxCell.x); ++x) {
                m_blocked[index(x, y)] = true;
            }
        }
    }
}

sf::Vector2i NavGrid::worldToCell(sf::Vector2f worldPos) const
{
    return sf::Vector2i(static_cast<int>(std::floor((worldPos.x - m_origin.x) / m_cellSize)),
        static_cast<int>(std::floor((worldPos.y - m_origin.y) / m_cellSize)));
}

sf::Vector2f NavGrid::cellToWorld(sf::Vector2i cell) const
{
    return sf::Vector2f(m_origin.x + (cell.x + 0.5f) * m_cellSize, m_origin.y + (cell.y + 0.5f) * m_cellSize);
}

bool NavGrid::isWalkableCell(int x, int y) const
{
    if (!inBounds(x, y)) {
        return false;
    }
    return !m_blocked[index(x, y)];
}

bool NavGrid::isWalkableWorld(sf::Vector2f worldPos) const
{
    sf::Vector2i cell = worldToCell(worldPos);
    return isWalkableCell(cell.x, cell.y);
}

std::vector<sf::Vector2f> NavGrid::findPath(sf::Vector2f fromWorld, sf::Vector2f toWorld) const
{
    sf::Vector2i start = worldToCell(fromWorld);
    sf::Vector2i goal = worldToCell(toWorld);

    if (!inBounds(start.x, start.y) || !inBounds(goal.x, goal.y)) {
        return {};
    }
    if (start == goal) {
        return {};
    }
    if (!isWalkableCell(goal.x, goal.y)) {
        return {}; // цель в стене — идти некуда
    }

    // "Поколение" вместо переаллокации/обнуления буферов целиком (см. m_searchGeneration в .h) — O(1) вместо
    // O(вся карта) накладных расходов на старте КАЖДОГО findPath(), которых раньше было ровно столько же, сколько
    // и вызовов A* в секунду по всем ботам сразу, независимо от того, насколько короткий сам путь.
    ++m_searchGeneration;
    auto gScoreAt = [this](int i) { return m_touchedGen[i] == m_searchGeneration ? m_gScore[i] : -1.f; };
    auto setGScore = [this](int i, float value, int cameFromIndex) {
        m_gScore[i] = value;
        m_cameFrom[i] = cameFromIndex;
        m_touchedGen[i] = m_searchGeneration;
    };
    auto isClosed = [this](int i) { return m_closedGen[i] == m_searchGeneration; };
    auto setClosed = [this](int i) { m_closedGen[i] = m_searchGeneration; };

    std::priority_queue<NodeRecord, std::vector<NodeRecord>, std::greater<NodeRecord>> open;

    int startIndex = index(start.x, start.y);
    int goalIndex = index(goal.x, goal.y);
    setGScore(startIndex, 0.f, -1);
    open.push({octileHeuristic(goal.x - start.x, goal.y - start.y), startIndex});

    constexpr int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    constexpr int DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    const float STEP_COST[8] = {1.f, 1.f, 1.f, 1.f, SQRT2, SQRT2, SQRT2, SQRT2};

    int expandedNodes = 0;
    while (!open.empty()) {
        int currentIndex = open.top().index;
        open.pop();
        if (isClosed(currentIndex)) {
            continue;
        }
        setClosed(currentIndex);
        ++expandedNodes;

        if (currentIndex == goalIndex) {
            break;
        }
        if (expandedNodes >= MAX_EXPANDED_NODES) {
            break; // см. MAX_EXPANDED_NODES — gScore[goalIndex] остался -1, дальше отработает как "недостижимо".
        }

        int cx = currentIndex % m_width;
        int cy = currentIndex / m_width;

        for (int dir = 0; dir < 8; ++dir) {
            int nx = cx + DX[dir];
            int ny = cy + DY[dir];
            if (!isWalkableCell(nx, ny)) {
                continue;
            }
            // Не режем угол по диагонали, если оба ортогональных соседа заблокированы.
            if (DX[dir] != 0 && DY[dir] != 0) {
                if (!isWalkableCell(cx + DX[dir], cy) && !isWalkableCell(cx, cy + DY[dir])) {
                    continue;
                }
            }

            int neighborIndex = index(nx, ny);
            if (isClosed(neighborIndex)) {
                continue;
            }

            float tentativeG = gScoreAt(currentIndex) + STEP_COST[dir];
            float existingG = gScoreAt(neighborIndex);
            if (existingG < 0.f || tentativeG < existingG) {
                setGScore(neighborIndex, tentativeG, currentIndex);
                float f = tentativeG + octileHeuristic(goal.x - nx, goal.y - ny);
                open.push({f, neighborIndex});
            }
        }
    }

    if (gScoreAt(goalIndex) < 0.f) {
        return {}; // недостижимо
    }

    std::vector<sf::Vector2f> path;
    int current = goalIndex;
    while (current != startIndex) {
        path.push_back(cellToWorld(sf::Vector2i(current % m_width, current / m_width)));
        current = m_cameFrom[current];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

bool NavGrid::hasLineOfSight(sf::Vector2f fromWorld, sf::Vector2f toWorld) const
{
    sf::Vector2f delta = toWorld - fromWorld;
    float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (distance <= 0.0001f) {
        return true;
    }

    int steps = std::max(1, static_cast<int>(distance / (m_cellSize * 0.5f)));
    for (int i = 0; i <= steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        sf::Vector2f sample = fromWorld + delta * t;
        if (!isWalkableWorld(sample)) {
            return false;
        }
    }
    return true;
}
