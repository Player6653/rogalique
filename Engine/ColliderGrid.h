#pragma once
#include "EngineExport.h"
#include <SFML/Graphics.hpp>
#include <vector>

class ColliderComponent;

// Пространственная сетка кинематических (см. ColliderComponent::isKinematic — стены, Pit и т.п.) коллайдеров.
// MovementComponent проверяет столкновение с ними на каждом шаге КАЖДОГО движущегося актёра КАЖДЫЙ кадр — раньше
// это был перебор буквально всех коллайдеров сцены (стен уже многие сотни, растёт с каждой новой комнатой от
// игрока), теперь — только те несколько клеток сетки, которые реально пересекаются с проверяемой областью.
// Строится один раз (как NavGrid, тем же принципом) сканированием уже добавленных в сцену кинематических
// коллайдеров — они статичны (стены/яма не двигаются после создания), перестраивать после этого не нужно.
class ENGINE_API ColliderGrid {
public:
    ColliderGrid(sf::Vector2f origin, int widthCells, int heightCells, float cellSize);

    // Сканирует переданный список (см. GameWorld::getColliders()) и раскладывает кинематические по клеткам —
    // остальные (актёры) игнорирует, для них и без сетки достаточно быстро перебрать напрямую, их мало.
    void rebuild(const std::vector<ColliderComponent*>& allColliders);

    // Кинематические коллайдеры в клетках, пересекающихся с bounds — может вернуть немного лишних (весь
    // прямоугольник затронутых клеток, не только реально задетые bounds) и, для коллайдеров крупнее клетки,
    // один и тот же коллайдер дважды — вызывающий код всё равно сам проверяет intersects() по месту.
    std::vector<ColliderComponent*> query(sf::FloatRect bounds) const;

private:
    sf::Vector2i cellOf(sf::Vector2f worldPos) const;

    sf::Vector2f m_origin;
    int m_width;
    int m_height;
    float m_cellSize;
    std::vector<std::vector<ColliderComponent*>> m_cells;
};
