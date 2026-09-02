#include "pch.h"
#include "ColliderGrid.h"
#include "ColliderComponent.h"
#include <algorithm>
#include <cmath>

ColliderGrid::ColliderGrid(sf::Vector2f origin, int widthCells, int heightCells, float cellSize)
    : m_origin(origin),
      m_width(widthCells),
      m_height(heightCells),
      m_cellSize(cellSize),
      m_cells(static_cast<std::size_t>(widthCells) * static_cast<std::size_t>(heightCells))
{
}

sf::Vector2i ColliderGrid::cellOf(sf::Vector2f worldPos) const
{
    return sf::Vector2i(static_cast<int>(std::floor((worldPos.x - m_origin.x) / m_cellSize)),
        static_cast<int>(std::floor((worldPos.y - m_origin.y) / m_cellSize)));
}

void ColliderGrid::rebuild(const std::vector<ColliderComponent*>& allColliders)
{
    for (auto& cell : m_cells) {
        cell.clear();
    }
    for (ColliderComponent* collider : allColliders) {
        if (!collider->isKinematic()) {
            continue;
        }
        sf::FloatRect bounds = collider->getBounds();
        sf::Vector2i minCell = cellOf(sf::Vector2f(bounds.left, bounds.top));
        sf::Vector2i maxCell = cellOf(sf::Vector2f(bounds.left + bounds.width, bounds.top + bounds.height));
        int x0 = std::max(0, minCell.x);
        int x1 = std::min(m_width - 1, maxCell.x);
        int y0 = std::max(0, minCell.y);
        int y1 = std::min(m_height - 1, maxCell.y);
        for (int y = y0; y <= y1; ++y) {
            for (int x = x0; x <= x1; ++x) {
                m_cells[static_cast<std::size_t>(y) * m_width + x].push_back(collider);
            }
        }
    }
}

std::vector<ColliderComponent*> ColliderGrid::query(sf::FloatRect bounds) const
{
    std::vector<ColliderComponent*> result;
    sf::Vector2i minCell = cellOf(sf::Vector2f(bounds.left, bounds.top));
    sf::Vector2i maxCell = cellOf(sf::Vector2f(bounds.left + bounds.width, bounds.top + bounds.height));
    int x0 = std::max(0, minCell.x);
    int x1 = std::min(m_width - 1, maxCell.x);
    int y0 = std::max(0, minCell.y);
    int y1 = std::min(m_height - 1, maxCell.y);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const std::vector<ColliderComponent*>& cell = m_cells[static_cast<std::size_t>(y) * m_width + x];
            result.insert(result.end(), cell.begin(), cell.end());
        }
    }
    return result;
}
