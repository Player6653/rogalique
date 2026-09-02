#include "pch.h"
#include "GameObject.h"
#include "TransientComponent.h"
#include <algorithm>

GameObject::GameObject(sf::Vector2f position)
    : m_position(position)
{
}

GameObject::~GameObject() = default;

GameObject& GameObject::addChild(std::unique_ptr<GameObject> child)
{
    GameObject& ref = *child;
    m_children.push_back(std::move(child));
    return ref;
}

void GameObject::clearChildren()
{
    m_children.clear();
}

void GameObject::destroyChild(GameObject* child)
{
    m_children.erase(std::remove_if(m_children.begin(), m_children.end(),
                         [child](const std::unique_ptr<GameObject>& c) { return c.get() == child; }),
        m_children.end());
}

void GameObject::resetComponents()
{
    for (auto& component : m_components) {
        component->reset();
    }
}

void GameObject::destroyTransientChildren()
{
    // Немедленно, не через mark-and-sweep (destroy()/isMarkedForDestruction) — тот сметается только на СЛЕДУЮЩЕМ
    // update() этого объекта, а полный ребут уровня зовётся из обработчика пункта меню (m_uiRoot), пока world
    // может быть на паузе (update() дерева actors не будет вызван вовсе, пока не начнётся новая игра) — до тех
    // пор помеченный, но не сметённый ребёнок продолжал бы РИСОВАТЬСЯ (draw() паузу не проверяет), то есть был бы
    // виден на фоне главного меню. Безопасно звать отсюда: это НЕ во время итерации m_children в update().
    m_children.erase(
        std::remove_if(m_children.begin(), m_children.end(),
            [](const std::unique_ptr<GameObject>& child) { return child->getComponent<TransientComponent>() != nullptr; }),
        m_children.end());
}

void GameObject::update(sf::Time dt)
{
    for (auto& component : m_components) {
        component->update(dt);
    }
    for (auto& child : m_children) {
        child->update(dt);
    }

    // Сметаем помеченных на удаление детей уже ПОСЛЕ того, как все они отработали этот кадр — правило destroy().
    m_children.erase(std::remove_if(m_children.begin(), m_children.end(),
                         [](const std::unique_ptr<GameObject>& child) { return child->isMarkedForDestruction(); }),
        m_children.end());
}

void GameObject::draw(sf::RenderWindow& window) const
{
    for (auto& component : m_components) {
        component->draw(window);
    }

    if (!m_sortChildrenByY) {
        for (auto& child : m_children) {
            child->draw(window);
        }
        return;
    }

    // Сортируем указатели, а не сам m_children (порядок отрисовки на кадр).
    std::vector<GameObject*> sortedByY;
    sortedByY.reserve(m_children.size());
    for (auto& child : m_children) {
        sortedByY.push_back(child.get());
    }
    std::sort(sortedByY.begin(), sortedByY.end(),
        [](const GameObject* a, const GameObject* b) { return a->getPosition().y < b->getPosition().y; });
    for (GameObject* child : sortedByY) {
        child->draw(window);
    }
}

void GameObject::setPosition(sf::Vector2f position)
{
    m_position = position;
    for (auto& component : m_components) {
        component->onOwnerMoved(m_position);
    }
}

void GameObject::move(sf::Vector2f offset)
{
    setPosition(m_position + offset);
}
