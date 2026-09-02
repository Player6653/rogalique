#pragma once
#include "EngineExport.h"
#include <SFML/Graphics.hpp>
#include <vector>

// Синглтон пула частиц (Graphics: вспышки/ауры/искры, см. docs/DESIGN_DOC.md) — тот же приём, что у AudioSystem/
// ToastNotificationSystem: любой код (например, компонент, следящий за HP игрока) зовёт spawnBurst() напрямую,
// не имея ссылки на конкретный отрисовщик. Сама отрисовка/тик — в ParticleSystemComponent (см. тот же каталог),
// который должен быть добавлен ровно один раз в МИРОВОЕ дерево сцены (не UI — частицы летают в мировых
// координатах и должны двигаться вместе с камерой, в отличие от тостов).
class ENGINE_API ParticleSystem {
public:
    static ParticleSystem& instance();

    // Разовый всплеск count частиц из одной точки. Каждая частица летит в случайном направлении со случайной
    // скоростью в [speedMin, speedMax] и живёт lifetime секунд, плавно гаснет (alpha 255->0) и чуть уменьшается
    // к концу жизни. sizeMin/sizeMax — радиус в пикселях. color — только rgb, alpha частицы считает сама по
    // остатку жизни, входной альфа-канал color игнорируется.
    void spawnBurst(sf::Vector2f position, int count, sf::Color color, float speedMin, float speedMax, float sizeMin,
        float sizeMax, sf::Time lifetime);

    // Продвигает все активные частицы — звать раз в кадр из ParticleSystemComponent::update(), не откуда-либо ещё.
    void update(sf::Time dt);
    void draw(sf::RenderWindow& window) const;

private:
    ParticleSystem() = default;

    struct Particle {
        sf::Vector2f position;
        sf::Vector2f velocity;
        sf::Color color;
        float radius;
        float initialRadius;
        sf::Time lifetime;
        sf::Time remaining;
    };

    std::vector<Particle> m_particles;
};
