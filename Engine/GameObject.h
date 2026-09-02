#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>
#include <memory>
#include <type_traits>
#include <vector>

// Игровой объект сцены — паттерн Компоновщик хранит и дочерние GameObject (тот же тип), и компоненты-стратегии (IComponent), которые определяют его конкретное поведение.
class ENGINE_API GameObject {
public:
    explicit GameObject(sf::Vector2f position = sf::Vector2f(0.f, 0.f));
    virtual ~GameObject();

    // Владеет своими детьми и компонентами через unique_ptr копирование не имеет смысла (и без явного запрета MSVC не может инстанцировать неявный operator= для dllexport-класса).
    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;

    // Дочерний объект того же типа — так дерево сцены строится произвольной глубины.
    GameObject& addChild(std::unique_ptr<GameObject> child);

    // Явно уничтожает всех детей (и их компоненты, включая GPU-ресурсы вроде sf::Texture).
    void clearChildren();

    // Немедленно (не mark-and-sweep, см. destroy()) убирает ОДНОГО конкретного прямого ребёнка по указателю — та
    // же гарантия безопасности, что и у destroyTransientChildren() (звать не во время итерации m_children в
    // update(), например из обработчика пункта меню). Нужен пересборке уровня "на лету" (см. SceneFacade.cpp —
    // rerollContent), когда число/состав "лишних" ботов и подбираемых предметов зависит от формы уровня и должно
    // быть безопасно пересоздано целиком, а не просто переставлено по новым позициям.
    void destroyChild(GameObject* child);

    // Создаёт компонент на месте, привязывает его к этому объекту и сразу синхронизирует с позицией.
    template <typename ComponentType, typename... Args> ComponentType& addComponent(Args&&... args)
    {
        static_assert(std::is_base_of<IComponent, ComponentType>::value, "ComponentType должен наследоваться от IComponent");

        auto component = std::make_unique<ComponentType>(std::forward<Args>(args)...);
        ComponentType& ref = *component;
        ref.setOwner(this);
        ref.onOwnerMoved(m_position);
        m_components.push_back(std::move(component));
        return ref;
    }

    // Первый найденный компонент нужного типа или nullptr, если такого нет.
    template <typename ComponentType> ComponentType* getComponent()
    {
        for (auto& component : m_components) {
            if (auto* casted = dynamic_cast<ComponentType*>(component.get())) {
                return casted;
            }
        }
        return nullptr;
    }

    // Все компоненты нужного типа у себя и рекурсивно у всех детей — нужно, например, чтобы MovementComponent мог найти все ColliderComponent на сцене и проверить столкновения.
    template <typename ComponentType> std::vector<ComponentType*> getComponentsInChildren()
    {
        std::vector<ComponentType*> result;
        for (auto& component : m_components) {
            if (auto* casted = dynamic_cast<ComponentType*>(component.get())) {
                result.push_back(casted);
            }
        }
        for (auto& child : m_children) {
            auto childResult = child->getComponentsInChildren<ComponentType>();
            result.insert(result.end(), childResult.begin(), childResult.end());
        }
        return result;
    }

    // Сбрасывает все свои компоненты (см. IComponent::reset) к начальному состоянию — не трогает детей.
    void resetComponents();

    // Немедленно убирает всех ПРЯМЫХ детей с TransientComponent (см. тот же файл) — объекты, заспавненные
    // динамически во время игры (например, дети деления слизи), а не расставленные изначально в SceneFacade.
    // Полный ребут уровня зовёт это наравне с resetComponents() у каждого актёра из изначального ростера — иначе
    // такие "лишние" объекты пережили бы рестарт. Не рекурсивно (как и resetComponents()) и не через mark-and-sweep
    // — см. комментарий в определении, почему это важно звать не во время собственной итерации m_children.
    void destroyTransientChildren();

    // Обновляет свои компоненты (в порядке добавления) и рекурсивно всех детей.
    virtual void update(sf::Time dt);
    // Рисует свои компоненты и рекурсивно всех детей — в порядке добавления, если не включён Y-sort (см. ниже).
    virtual void draw(sf::RenderWindow& window) const;

    // Включает Y-sort для СВОих прямых детей.
    void setSortChildrenByY(bool enabled)
    {
        m_sortChildrenByY = enabled;
    }

    sf::Vector2f getPosition() const
    {
        return m_position;
    }
    void setPosition(sf::Vector2f position);
    void move(sf::Vector2f offset);

    // Помечает объект на удаление из дерева нужно снарядам (Projectile), которые должны исчезать после попадания
    // или истечения дальности. Само удаление — в конце update() родителя (см. ниже), а не сразу: component->update()
    // может звать destroy() у СВОЕГО ЖЕ владельца, пока родитель ещё итерирует m_children этим же кадром, а стирать
    // элемент вектора прямо во время такой итерации — undefined behavior.
    void destroy()
    {
        m_markedForDestruction = true;
    }
    bool isMarkedForDestruction() const
    {
        return m_markedForDestruction;
    }

private:
    sf::Vector2f m_position;
    std::vector<std::unique_ptr<IComponent>> m_components;
    std::vector<std::unique_ptr<GameObject>> m_children;
    bool m_sortChildrenByY = false;
    bool m_markedForDestruction = false;
};
