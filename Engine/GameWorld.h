#pragma once
#include "ColliderGrid.h"
#include "EngineExport.h"
#include "GameObject.h"
#include "NavGrid.h"
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

class ColliderComponent;
class HealthComponent;
class CameraComponent;

// Синглтон — единственное дерево игровых объектов сцены, корень паттерна Компоновщик.
class ENGINE_API GameWorld {
public:
    static GameWorld& instance();

    GameWorld(const GameWorld&) = delete;
    GameWorld& operator=(const GameWorld&) = delete;

    GameObject& getRoot()
    {
        return m_root;
    }
    GameObject& getUIRoot()
    {
        return m_uiRoot;
    }

    // На паузе мировое дерево (враги, игрок, физика, таймеры) не обновляется вовсе.
    bool isPaused() const
    {
        return m_paused;
    }
    void setPaused(bool paused)
    {
        m_paused = paused;
    }

    // Отдельно от паузы.
    bool hasStarted() const
    {
        return m_started;
    }
    // Уход в главное меню всегда означает конец текущего забега — заодно снимает game over/победу, чтобы они не пережили переход.
    void setStarted(bool started)
    {
        m_started = started;
        if (!started) {
            m_gameOver = false;
            m_victory = false;
        }
    }

    // Модальный оверлей (например, экран титров) забирает ввод целиком.
    bool isModalOpen() const
    {
        return m_modalOpen;
    }
    void setModalOpen(bool open)
    {
        m_modalOpen = open;
    }

    // Игрок умер, на экране поражение. Мир НЕ на паузе (см. PlayerDeathComponent) — он продолжает жить, пока не
    // выбрали пункт меню, чтобы анимация смерти доигрывала, а не замирала на первом кадре.
    bool isGameOver() const
    {
        return m_gameOver;
    }
    void setGameOver(bool gameOver)
    {
        m_gameOver = gameOver;
    }

    // Открыта главная дверь (см. Rogalique/Door.h) — экран победы, тот же принцип, что и isGameOver(): мир не
    // ставим на паузу сами (см. симметричный комментарий у isGameOver), просто показываем оверлей поверх.
    bool isVictory() const
    {
        return m_victory;
    }
    void setVictory(bool victory)
    {
        m_victory = victory;
    }

    void update(sf::Time dt)
    {
        if (!m_paused) {
            m_root.update(dt);
        }
        m_uiRoot.update(dt);

        // Отложенный спавн выполняется здесь, а не сразу из вызывающего кода (например, RangedAttackComponent,
        // SlimeSplitComponent), потому что addChild() пушит в тот же вектор m_children, который в этот момент мог
        // итерировать update() у m_root/её потомков ещё выше по стеку вызовов — push_back туда во время такой
        // итерации мог бы инвалидировать текущий range-for и уронить кадр.
        for (auto& pending : m_pendingSpawns) {
            pending.first->addChild(std::move(pending.second));
        }
        m_pendingSpawns.clear();
    }
    void draw(sf::RenderWindow& window) const
    {
        m_root.draw(window);
    }
    void drawUI(sf::RenderWindow& window) const
    {
        m_uiRoot.draw(window);
    }

    // Единственный безопасный способ добавить объект в дерево сцены во время кадра — см. комментарий в update().
    void spawnInRoot(std::unique_ptr<GameObject> object)
    {
        spawnIn(m_root, std::move(object));
    }
    // То же самое, но в произвольного родителя, а не обязательно в корень — например, чтобы дети деления слизи
    // (см. SlimeSplitComponent) попали в тот же Y-sort контейнер actors, что и все остальные актёры, а не рисовались
    // поверх всех через m_root (у того сортировки по Y нет).
    void spawnIn(GameObject& parent, std::unique_ptr<GameObject> object)
    {
        m_pendingSpawns.emplace_back(&parent, std::move(object));
    }

    // Уничтожает всю сцену (и GPU-ресурсы её объектов) прямо сейчас, не дожидаясь уничтожения синглтона при выходе из процесса — см. GameObject::clearChildren.
    void clear()
    {
        m_root.clearChildren();
        m_uiRoot.clearChildren();
    }

    // Реестр живых коллайдеров. m_dynamicColliders — те же, но только НЕ кинематические (актёры — их единицы-
    // десятки, в отличие от кинематических стен, которых уже многие сотни) — отдельным маленьким списком, чтобы
    // MovementComponent мог перебирать актёров напрямую, не через ColliderGrid (та только под кинематические) и
    // не через полный m_colliders (пришлось бы пропускать сотни стен ради поиска среди них десятка актёров).
    // Определения — в .cpp: нужен полный ColliderComponent (isKinematic()), а тут он только forward-declared.
    void registerCollider(ColliderComponent* collider);
    void unregisterCollider(ColliderComponent* collider);
    const std::vector<ColliderComponent*>& getColliders() const
    {
        return m_colliders;
    }
    const std::vector<ColliderComponent*>& getDynamicColliders() const
    {
        return m_dynamicColliders;
    }

    void registerHealth(HealthComponent* health)
    {
        m_healthComponents.push_back(health);
    }
    void unregisterHealth(HealthComponent* health)
    {
        m_healthComponents.erase(
            std::remove(m_healthComponents.begin(), m_healthComponents.end(), health), m_healthComponents.end());
    }
    const std::vector<HealthComponent*>& getHealthComponents() const
    {
        return m_healthComponents;
    }

    // Та же идея, что у реестра коллайдеров/HP выше — раньше активную камеру КАЖДЫЙ кадр искали заново через
    // getComponentsInChildren<CameraComponent>() по всему дереву сцены (см. Engine::run()); с разросшимся деревом
    // (декор от игрока — тысячи объектов) это была ощутимая и совершенно не нужная постоянная нагрузка каждый
    // кадр, независимо от боя/ботов. Камера в игре ровно одна и живёт весь процесс (у Player, не пересоздаётся),
    // так что просто последняя зарегистрированная — этого достаточно.
    void registerCamera(CameraComponent* camera)
    {
        m_activeCamera = camera;
    }
    void unregisterCamera(CameraComponent* camera)
    {
        if (m_activeCamera == camera) {
            m_activeCamera = nullptr;
        }
    }
    CameraComponent* getActiveCamera()
    {
        return m_activeCamera;
    }

    // Строит сетку проходимости комнаты для A*-pathfinding (см. NavGrid) — сканирует уже добавленные в сцену
    // кинематические коллайдеры (стены). Звать один раз из SceneFacade, после того как все стены уже в дереве.
    void buildNavGrid(sf::Vector2f origin, int widthCells, int heightCells, float cellSize)
    {
        m_navGrid = std::make_unique<NavGrid>(origin, widthCells, heightCells, cellSize);
        m_navGrid->rebuildFromColliders();
    }
    // nullptr, если buildNavGrid ещё не вызывали — вызывающий код (ChaseComponent и т.п.) должен быть готов к
    // этому и падать обратно на поведение без pathfinding, а не разыменовывать вслепую.
    NavGrid* getNavGrid()
    {
        return m_navGrid.get();
    }

    // Пространственная сетка кинематических коллайдеров (см. ColliderGrid) — звать один раз из SceneFacade, тем
    // же моментом, что и buildNavGrid (после того как все стены/Pit уже в дереве). Те же параметры, что у
    // NavGrid — одна и та же сетка клеток на уровень, значения совпадают буквально (TILE_SIZE), поэтому SceneFacade
    // зовёт оба builder'а подряд с одними и теми же origin/widthCells/heightCells/cellSize.
    void buildColliderGrid(sf::Vector2f origin, int widthCells, int heightCells, float cellSize)
    {
        m_colliderGrid = std::make_unique<ColliderGrid>(origin, widthCells, heightCells, cellSize);
        m_colliderGrid->rebuild(m_colliders);
    }
    // Кинематические коллайдеры рядом с bounds — см. ColliderGrid::query(). Пустой вектор, если buildColliderGrid
    // ещё не вызывали (мир на паузе до конца сборки сцены, MovementComponent до этого момента не тикает).
    std::vector<ColliderComponent*> queryKinematicColliders(sf::FloatRect bounds) const
    {
        return m_colliderGrid ? m_colliderGrid->query(bounds) : std::vector<ColliderComponent*>{};
    }

private:
    GameWorld() = default;

    // Объявлены ДО m_root/m_uiRoot нарочно: порядок разрушения членов класса — обратный порядку объявления, а
    // деструкторы ColliderComponent::~ColliderComponent()/HealthComponent::~HealthComponent() зовут
    // unregisterCollider()/unregisterHealth(), обращаясь к этим же векторам того же синглтона. Если бы они были
    // объявлены позже m_root/m_uiRoot, то разрушались бы РАНЬШЕ дерева сцены — любой компонент, ещё живущий в
    // дереве на момент уничтожения GameWorld (например, если Engine::run() прервался необработанным исключением
    // до штатного world.clear() в конце — см. Engine.cpp), обратился бы к уже разрушенным векторам: UB, порча
    // памяти/крэш при выходе из процесса вместо честного сообщения об исходной ошибке (найдено при аудите движка).
    std::vector<ColliderComponent*> m_colliders;
    std::vector<ColliderComponent*> m_dynamicColliders;
    std::vector<HealthComponent*> m_healthComponents;
    CameraComponent* m_activeCamera = nullptr;

    GameObject m_root;
    GameObject m_uiRoot;
    bool m_paused = true;
    bool m_started = false;
    bool m_modalOpen = false;
    bool m_gameOver = false;
    bool m_victory = false;

    std::vector<std::pair<GameObject*, std::unique_ptr<GameObject>>> m_pendingSpawns;
    std::unique_ptr<NavGrid> m_navGrid;
    std::unique_ptr<ColliderGrid> m_colliderGrid;
};
