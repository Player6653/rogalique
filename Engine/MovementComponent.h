#pragma once
#include "EngineExport.h"
#include "IComponent.h"
#include <SFML/Graphics.hpp>

class GameObject;
class ColliderComponent;

// Стратегия перемещения, берёт направление у соседнего IDirectionProvider.
class ENGINE_API MovementComponent : public IComponent {
public:
    explicit MovementComponent(float speed);

    void update(sf::Time dt) override;

    void setSpeed(float speed)
    {
        m_speed = speed;
    }

private:
    float m_speed;

    // (стены) — рывок даёт неуязвимость именно затем, чтобы проскочить мимо/сквозь врага, а не только визуально.
    // ignoresPits — см. IDirectionProvider::ignoresObstacles()/PitComponent: прыжок игнорирует яму/лаву отдельно
    // от passThroughActors (та не спасает от кинематических препятствий вообще, см. .cpp).
    bool collidesAt(GameObject* owner, ColliderComponent* myCollider, sf::Vector2f proposedPosition, bool passThroughActors,
        bool ignoresPits) const;

    // true, если owner СЕЙЧАС (без предложенного смещения) хотя бы частично стоит в яме/лаве — прыжок закончился,
    // не долетев до другого края (см. update()).
    bool isOverlappingAnyPit(GameObject* owner, ColliderComponent* myCollider) const;
    // Первая точка вдоль direction (с шагом, см. .cpp), где owner больше не пересекает ни одну яму/лаву — куда
    // выталкивает застрявшего после неудачного прыжка (см. update()).
    sf::Vector2f findSafeEjectPosition(GameObject* owner, ColliderComponent* myCollider, sf::Vector2f direction) const;

    // Плавное отталкивание из ямы/лавы (см. update()) — не мгновенный телепорт, а интерполяция позиции за
    // m_pitEjectDuration. m_pitEjectElapsed >= m_pitEjectDuration (в том числе оба Zero по умолчанию, до первого
    // застревания) означает "сейчас не отталкивает" — так же, как для обычных таймеров-кулдаунов в проекте.
    sf::Vector2f m_pitEjectStart;
    sf::Vector2f m_pitEjectTarget;
    sf::Time m_pitEjectElapsed;
    sf::Time m_pitEjectDuration;
};
